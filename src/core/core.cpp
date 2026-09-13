#include <catalyst/config.hpp>
#include <catalyst/core/core.hpp>

namespace catalyst::core
{

    const char *module_name()
    {
        return "catalyst::core";
    }

} // namespace catalyst::core

namespace catalyst
{

    const char *version() noexcept
    {
        return CATALYST_VERSION_STRING;
    }

} // namespace catalyst
