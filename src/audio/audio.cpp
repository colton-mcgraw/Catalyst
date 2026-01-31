#include <catalyst/audio/audio.hpp>

#include "detail_backend.hpp"

namespace catalyst::audio {

const char* module_name() {
  return detail::backend_name();
}

} // namespace catalyst::audio
