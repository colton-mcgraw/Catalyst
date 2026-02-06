#include "../detail_backend.hpp"

#include "../asio/asio_backend_win32.hpp"
#include "../wasapi/wasapi_backend_win32.hpp"

namespace catalyst::audio::detail
{

  std::unique_ptr<backend> create_backend(const engine_config &config) noexcept
  {
    auto selected = config.preferred_backend;
    if (selected == engine_backend::engine_backend_unknown)
      selected = engine_backend::engine_backend_wasapi;

    switch (selected)
    {
    case engine_backend::engine_backend_asio:
      return create_asio_backend_win32(config);
    case engine_backend::engine_backend_wasapi:
      return create_wasapi_backend_win32(config);
    default:
      // Unsupported backend for this implementation.
      return nullptr;
    }
  }

} // namespace catalyst::audio::detail
