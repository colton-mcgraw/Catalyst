#include <catalyst/audio/engine.hpp>

#include "detail_backend.hpp"

#include <utility>

namespace catalyst::audio {

namespace {

void render_silence(void*, float* output_interleaved, uint32_t frames, uint32_t channels)
{
  const uint32_t count = frames * channels;
  for (uint32_t i = 0; i < count; ++i)
    output_interleaved[i] = 0.0f;
}

} // namespace

struct engine::impl
{
  engine_config config{};
  std::unique_ptr<detail::backend> backend;
  bool initialized = false;
  bool running = false;

  mutable std::vector<std::string> device_names_cache;
  mutable std::vector<std::string_view> device_views_cache;
};

engine::engine() : impl_(std::make_unique<impl>()) {}

engine::~engine() = default;

engine::engine(engine&& other) noexcept = default;
engine& engine::operator=(engine&& other) noexcept = default;

std::vector<std::string_view> engine::enumerate_devices() const
{
  if (!impl_)
    return {};

  impl_->device_names_cache.clear();
  impl_->device_views_cache.clear();

  // If we're not initialized yet, create a temporary backend for enumeration.
  std::unique_ptr<detail::backend> tmp;
  const detail::backend* b = impl_->backend.get();
  if (!b)
  {
    tmp = detail::create_backend(impl_->config);
    b = tmp.get();
  }

  if (!b)
    return {};

  const auto names = b->enumerate_devices();
  impl_->device_names_cache.reserve(names.size());
  for (const auto sv : names)
    impl_->device_names_cache.emplace_back(sv);

  impl_->device_views_cache.reserve(impl_->device_names_cache.size());
  for (const auto& s : impl_->device_names_cache)
    impl_->device_views_cache.emplace_back(s);

  return impl_->device_views_cache;
}

std::vector<std::string_view> engine::enumerate_devices(engine_backend backend) const
{
  if (!impl_)
    return {};

  impl_->device_names_cache.clear();
  impl_->device_views_cache.clear();

  engine_config cfg = impl_->config;
  cfg.preferred_backend = backend;

  auto tmp = detail::create_backend(cfg);
  if (!tmp)
    return {};

  const auto names = tmp->enumerate_devices();
  impl_->device_names_cache.reserve(names.size());
  for (const auto sv : names)
    impl_->device_names_cache.emplace_back(sv);

  impl_->device_views_cache.reserve(impl_->device_names_cache.size());
  for (const auto& s : impl_->device_names_cache)
    impl_->device_views_cache.emplace_back(s);

  return impl_->device_views_cache;
}

bool engine::initialize(const engine_config& config)
{
  if (impl_->initialized)
    return true;

  impl_->config = config;

  if (!impl_->config.callback)
    impl_->config.callback = &render_silence;

  impl_->backend = detail::create_backend(impl_->config);

  if (!impl_->backend)
    return false;

  impl_->initialized = impl_->backend->initialize();
  impl_->running = false;
  return impl_->initialized;
}

bool engine::start()
{
  if (!impl_ || !impl_->initialized || !impl_->backend)
    return false;
  if (impl_->running)
    return true;

  impl_->running = impl_->backend->start();
  return impl_->running;
}

void engine::stop() noexcept
{
  if (!impl_ || !impl_->backend)
    return;
  if (!impl_->running)
    return;

  impl_->backend->stop();
  impl_->running = false;
}

void engine::shutdown() noexcept
{
  if (!impl_ || !impl_->initialized)
    return;

  if (impl_->backend)
  {
    stop();
    impl_->backend->shutdown();
  }

  impl_->backend.reset();
  impl_->initialized = false;
  impl_->running = false;
}

bool engine::is_initialized() const noexcept
{
  return impl_ && impl_->initialized;
}

bool engine::is_running() const noexcept
{
  return impl_ && impl_->running;
}

std::string_view engine::backend_name() const noexcept
{
  if (!impl_ || !impl_->backend)
    return {};
  return impl_->backend->name();
}

} // namespace catalyst::audio
