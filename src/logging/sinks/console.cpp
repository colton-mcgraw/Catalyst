/**
 * @file console.cpp
 * @brief Implements `console_sink`.
 * @details Whether the sink colours at all is decided in terminal.cpp; this file only picks the
 * palette accordingly and writes the line. The write goes through an `osyncstream` so that two
 * threads logging at once produce two lines rather than one interleaved mess, which is what lets
 * the sink declare `reentrant` while holding no lock of its own.
 * License: MIT (see LICENSE).
 */

#include <catalyst/logging/sinks/console.hpp>

#include <iostream>
// <syncstream> is C++20, and libc++ does not implement it -- __cpp_lib_syncbuf is how the
// standard lets us ask. Where it is missing, one mutex gives the guarantee that actually matters
// here; see console_sink::write.
#if defined(__cpp_lib_syncbuf)
#  include <syncstream>
#else
#  include <mutex>
#endif

namespace catalyst::logging
{

    namespace
    {
        /// The palette a sink writing somewhere that is not a terminal uses: none.
        const line_style plain{};
    } // namespace

    console_sink::console_sink() noexcept : stream(&std::clog) {}

    console_sink::console_sink(std::ostream &out, line_format format) noexcept : stream(&out), format(format) {}

    void console_sink::write(const log_event &event) const
    {
        line_cache cache(event);
        write(event, cache);
    }

    void console_sink::write(const log_event &event, line_cache &cache) const
    {
        const std::string &line = cache.line(format, colored() ? style : plain);

#if defined(__cpp_lib_syncbuf)
        // osyncstream so that lines from different threads do not interleave.
        std::osyncstream output(*stream);
        output << line << '\n';
#else
        // No <syncstream>. Every console write goes through here, so one mutex held across the
        // whole line gives the same property osyncstream was bought for: a line is emitted whole
        // rather than interleaved with another thread's. Neither version synchronises against code
        // that writes to the same stream without going through this sink.
        static std::mutex write_mutex;
        const std::scoped_lock lock{write_mutex};
        *stream << line << '\n';
#endif
    }

    void console_sink::flush() const
    {
        stream->flush();
    }

    bool console_sink::colored() const noexcept
    {
        switch (color)
        {
        case color_mode::always:
            return true;
        case color_mode::never:
            return false;
        case color_mode::automatic:
            break;
        }
        return terminal_supports_color(*stream);
    }

} // namespace catalyst::logging
