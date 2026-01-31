#include <catalyst/platform/platform.hpp>

#include "detail_backend.hpp"

namespace catalyst::platform {

const char* module_name() {
  return detail::backend_name();
}

} // namespace catalyst::platform
