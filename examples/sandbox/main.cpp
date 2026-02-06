// examples/sandbox/main.cpp

#include <catalyst/catalyst.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <thread>

namespace {

struct tone_state
{
  double phase = 0.0;
  double phase_step = 0.0;
  uint64_t fade_samples = 0;

  std::atomic<uint64_t> sample_index{0};
  std::atomic<uint64_t> stop_begin{std::numeric_limits<uint64_t>::max()};
};

void render_test_tone(void* user, float* output_interleaved, uint32_t frames, uint32_t channels)
{
  auto* state = static_cast<tone_state*>(user);
  if (!state || !output_interleaved || frames == 0 || channels == 0)
    return;

  constexpr float base_gain = 0.2f;

  for (uint32_t f = 0; f < frames; ++f)
  {
    const uint64_t si = state->sample_index.fetch_add(1, std::memory_order_relaxed);

    float gain = base_gain;

    // Fade in.
    if (state->fade_samples != 0 && si < state->fade_samples)
      gain *= static_cast<float>(static_cast<double>(si) / static_cast<double>(state->fade_samples));

    // Fade out once requested.
    const uint64_t sb = state->stop_begin.load(std::memory_order_relaxed);
    if (sb != std::numeric_limits<uint64_t>::max())
    {
      if (si >= sb)
      {
        const uint64_t n = si - sb;
        if (state->fade_samples != 0)
        {
          const double t = static_cast<double>(n) / static_cast<double>(state->fade_samples);
          const double k = (t >= 1.0) ? 0.0 : (1.0 - t);
          gain *= static_cast<float>(k);
        }
        else
        {
          gain = 0.0f;
        }
      }
    }

    const float sample = gain * static_cast<float>(std::sin(state->phase));
    state->phase += state->phase_step;
    if (state->phase >= 6.2831853071795864769)
      state->phase -= 6.2831853071795864769;

    const uint32_t base = f * channels;
    for (uint32_t c = 0; c < channels; ++c)
      output_interleaved[base + c] = sample;
  }
}

} // namespace

int main()
{
  catalyst::catalyst_version_anchor();

  auto audio_engine = catalyst::audio::engine{};

  tone_state tone;
  constexpr double freq_hz = 440.0;

  catalyst::audio::engine_config cfg;
  cfg.sample_rate = 48000;
  cfg.channels = 2;
  cfg.frames_per_buffer = 512;
  cfg.callback = &render_test_tone;
  cfg.user = &tone;

  tone.phase = 0.0;
  tone.phase_step = (6.2831853071795864769 * freq_hz) / static_cast<double>(cfg.sample_rate);
  tone.fade_samples = cfg.sample_rate / 50; // 20ms

  if (!audio_engine.initialize(cfg))
  {
    std::fprintf(stderr, "Failed to initialize audio engine\n");
    return 1;
  }

  if (!audio_engine.start())
  {
    std::fprintf(stderr, "Failed to start audio engine\n");
    return 1;
  }

  std::printf("Audio engine started with backend: %s\n", audio_engine.backend_name().data());

  for (auto device : audio_engine.enumerate_devices())
  {
    std::printf(" - Device: %s\n", device.data());
  }

  std::printf("Playing 440Hz test tone for 2 seconds...\n");
  std::this_thread::sleep_for(std::chrono::seconds(2));

  // Request a short fade-out before stopping to reduce clicks.
  tone.stop_begin.store(tone.sample_index.load(std::memory_order_relaxed), std::memory_order_relaxed);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  audio_engine.stop();
  audio_engine.shutdown();

  return 0;
}