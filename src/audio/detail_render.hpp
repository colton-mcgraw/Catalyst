/**
 * @file detail_render.hpp
 * @brief Internal helpers shared by every audio backend: the atomic health counters behind
 * `stream_stats`, and the dispatcher that invokes the user's render callback with a correctly
 * populated `render_context`. Centralising these keeps stream-time bookkeeping, silence
 * fallback and load measurement identical across WASAPI, ASIO, offline and null.
 * License: CDDL-1.0 (see LICENSE).
 */

#pragma once

#include <catalyst/audio/engine.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>

namespace catalyst::audio::detail
{

    /// Health counters for one stream.
    ///
    /// Counters are published with relaxed atomics: the render thread is the only writer for
    /// the timing fields, so no read-modify-write (and therefore no CAS loop) happens on the
    /// real-time path. `add_xrun` and `add_device_change` may be called from any thread.
    /// `reset()` racing a live render thread is benign -- it can lose at most one block.
    class stats_block
    {
    public:
        /// Render thread. Marks the start of a callback for load measurement.
        void begin_block() noexcept
        {
            block_start_ = std::chrono::steady_clock::now();
        }

        /// Render thread. Records a completed block of `frames` at `sample_rate`.
        void end_block(uint32_t frames, uint32_t sample_rate) noexcept
        {
            const auto elapsed = std::chrono::duration<double>(
                                     std::chrono::steady_clock::now() - block_start_)
                                     .count();

            if (elapsed > peak_seconds_)
                peak_seconds_ = elapsed;

            if (sample_rate != 0 && frames != 0)
            {
                const double budget = static_cast<double>(frames) / static_cast<double>(sample_rate);
                const double load = elapsed / budget;
                if (load > peak_load_)
                    peak_load_ = load;
            }

            frames_.fetch_add(frames, std::memory_order_relaxed);
            callbacks_.fetch_add(1, std::memory_order_relaxed);
            last_seconds_.store(elapsed, std::memory_order_relaxed);
            peak_seconds_pub_.store(peak_seconds_, std::memory_order_relaxed);
            peak_load_pub_.store(peak_load_, std::memory_order_relaxed);
        }

        /// Any thread. Records audio that was dropped or not produced in time.
        void add_xrun(uint64_t count = 1) noexcept
        {
            xruns_.fetch_add(count, std::memory_order_relaxed);
        }

        /// Any thread. Records a device topology change.
        void add_device_change() noexcept
        {
            device_changes_.fetch_add(1, std::memory_order_relaxed);
        }

        stream_stats snapshot() const noexcept
        {
            stream_stats out;
            out.frames_rendered = frames_.load(std::memory_order_relaxed);
            out.callback_count = callbacks_.load(std::memory_order_relaxed);
            out.xruns = xruns_.load(std::memory_order_relaxed);
            out.device_changes = device_changes_.load(std::memory_order_relaxed);
            out.last_callback_seconds = last_seconds_.load(std::memory_order_relaxed);
            out.peak_callback_seconds = peak_seconds_pub_.load(std::memory_order_relaxed);
            out.peak_load = peak_load_pub_.load(std::memory_order_relaxed);
            return out;
        }

        void reset() noexcept
        {
            peak_seconds_ = 0.0;
            peak_load_ = 0.0;

            frames_.store(0, std::memory_order_relaxed);
            callbacks_.store(0, std::memory_order_relaxed);
            xruns_.store(0, std::memory_order_relaxed);
            device_changes_.store(0, std::memory_order_relaxed);
            last_seconds_.store(0.0, std::memory_order_relaxed);
            peak_seconds_pub_.store(0.0, std::memory_order_relaxed);
            peak_load_pub_.store(0.0, std::memory_order_relaxed);
        }

    private:
        // Render-thread-owned scratch; never read by other threads.
        std::chrono::steady_clock::time_point block_start_{};
        double peak_seconds_ = 0.0;
        double peak_load_ = 0.0;

        std::atomic<uint64_t> frames_{0};
        std::atomic<uint64_t> callbacks_{0};
        std::atomic<uint64_t> xruns_{0};
        std::atomic<uint64_t> device_changes_{0};
        std::atomic<double> last_seconds_{0.0};
        std::atomic<double> peak_seconds_pub_{0.0};
        std::atomic<double> peak_load_pub_{0.0};
    };

    /// Fills `output` with silence. Shared so no backend rolls its own.
    inline void write_silence(float *output, uint32_t frames, uint32_t channels) noexcept
    {
        if (!output)
            return;

        const size_t count = static_cast<size_t>(frames) * static_cast<size_t>(channels);
        for (size_t i = 0; i < count; ++i)
            output[i] = 0.0f;
    }

    /// Invokes the user callback for one block, maintaining stream time and stats.
    ///
    /// Owns the monotonic frame counter that backs `render_context::stream_time_frames`, which
    /// is the only correct clock for callbacks: it is a 64-bit integer, so unlike a float
    /// accumulator it stays exact for longer than any process will run.
    class render_dispatcher
    {
    public:
        render_dispatcher(render_callback callback, void *user, stats_block &stats) noexcept
            : callback_(callback), user_(user), stats_(&stats) {}

        /// Render thread. `output` may be null for input-only streams; `input` may be null.
        /// Guarantees `output` is fully written even when no callback is installed.
        void dispatch(
            float *output,
            const float *input,
            uint32_t frames,
            uint32_t output_channels,
            uint32_t input_channels,
            uint32_t sample_rate) noexcept
        {
            if (frames == 0)
                return;

            if (!callback_)
            {
                write_silence(output, frames, output_channels);
                stream_frames_.fetch_add(frames, std::memory_order_relaxed);
                return;
            }

            render_context context;
            context.output = output;
            context.input = input;
            context.frames = frames;
            context.output_channels = output_channels;
            context.input_channels = input_channels;
            context.sample_rate = sample_rate;
            context.stream_time_frames = stream_frames_.load(std::memory_order_relaxed);
            context.user = user_;

            stats_->begin_block();
            callback_(context);
            stats_->end_block(frames, sample_rate);

            stream_frames_.fetch_add(frames, std::memory_order_relaxed);
        }

        /// Frames dispatched since the last `reset_time()`.
        uint64_t stream_frames() const noexcept
        {
            return stream_frames_.load(std::memory_order_relaxed);
        }

        void reset_time() noexcept
        {
            stream_frames_.store(0, std::memory_order_relaxed);
        }

        bool has_callback() const noexcept { return callback_ != nullptr; }

    private:
        render_callback callback_ = nullptr;
        void *user_ = nullptr;
        stats_block *stats_ = nullptr;
        std::atomic<uint64_t> stream_frames_{0};
    };

} // namespace catalyst::audio::detail
