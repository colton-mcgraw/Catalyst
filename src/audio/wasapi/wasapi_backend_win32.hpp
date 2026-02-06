#pragma once

#include "../detail_backend.hpp"

namespace catalyst::audio::detail
{

    std::unique_ptr<backend> create_wasapi_backend_win32(const engine_config &config) noexcept;

} // namespace catalyst::audio::detail
