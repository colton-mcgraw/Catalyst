#include <catalyst/input/input.hpp>

#include "detail_backend.hpp"

namespace catalyst::input {

const char* module_name() {
  return detail::backend_name();
}

} // namespace catalyst::input
