#include <catalyst/input/keyboard.hpp>

namespace catalyst::input
{

    text_input_event::text_input_event() noexcept : text(buffer.data(), 0) {}

    text_input_event::text_input_event(std::span<const char32_t> input) noexcept : text(buffer.data(), 0)
    {
        assign(input);
    }

    text_input_event::text_input_event(const text_input_event &other) noexcept
        : buffer(other.buffer), length(other.length), text(buffer.data(), other.length)
    {
    }

    text_input_event &text_input_event::operator=(const text_input_event &other) noexcept
    {
        if (this == &other)
            return *this;
        buffer = other.buffer;
        length = other.length;
        text = {buffer.data(), other.length};
        return *this;
    }

    text_input_event::text_input_event(text_input_event &&other) noexcept
        : buffer(other.buffer), length(other.length), text(buffer.data(), other.length)
    {
    }

    text_input_event &text_input_event::operator=(text_input_event &&other) noexcept
    {
        buffer = other.buffer;
        length = other.length;
        text = {buffer.data(), other.length};
        return *this;
    }

    void text_input_event::assign(std::span<const char32_t> input) noexcept
    {
        const std::size_t n = std::min<std::size_t>(input.size(), buffer.size());
        length = static_cast<std::uint8_t>(n);
        std::copy_n(input.begin(), n, buffer.begin());
        text = {buffer.data(), n};
    }
}