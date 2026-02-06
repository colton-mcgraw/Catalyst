#include "../detail_backend.hpp"

namespace catalyst::audio::detail
{

  namespace
  {

    class null_backend final : public backend
    {
    public:
      std::string_view name() const noexcept override { return "null"; }

      std::vector<std::string_view> enumerate_devices() const override
      {
        return {};
      }

      bool initialize() override
      {
        return true;
      }

      bool start() override
      {
        running_ = true;
        return true;
      }

      void stop() noexcept override
      {
        running_ = false;
      }

      bool is_running() const noexcept override
      {
        return running_;
      }

      void shutdown() noexcept override {}

    private:
      bool running_ = false;
    };

  } // namespace
  
  std::unique_ptr<backend> create_backend(const engine_config &config) noexcept
  {
    // Always return the null backend.
    (void)config;
    return std::make_unique<null_backend>();
  }
} // namespace catalyst::audio::detail
