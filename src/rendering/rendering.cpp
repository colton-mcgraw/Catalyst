#include <catalyst/rendering/rendering.hpp>

#include "detail_backend.hpp"

namespace catalyst::rendering {

const char* module_name() {
  return detail::backend_name();
}

} // namespace catalyst::rendering
