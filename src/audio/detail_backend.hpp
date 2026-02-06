#pragma once

#include <memory>
#include <vector>
#include <string_view>

#include <catalyst/audio/engine.hpp>

namespace catalyst::audio::detail {

class backend
{
public:
	virtual ~backend() = default;

	virtual std::string_view name() const noexcept = 0;
	virtual std::vector<std::string_view> enumerate_devices() const = 0;
	virtual bool initialize() = 0;
	virtual bool start() = 0;
	virtual void stop() noexcept = 0;
	virtual bool is_running() const noexcept = 0;
	virtual void shutdown() noexcept = 0;
};

std::unique_ptr<backend> create_backend(const engine_config& config) noexcept;

} // namespace catalyst::audio::detail
