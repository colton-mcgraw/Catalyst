#pragma once

#include "../detail_backend.hpp"

namespace catalyst::audio::detail
{

    // Implemented in `asio/asio_backend_win32.cpp`
    std::unique_ptr<backend> create_asio_backend_win32(const engine_config &config) noexcept;

} // namespace catalyst::audio::detail
