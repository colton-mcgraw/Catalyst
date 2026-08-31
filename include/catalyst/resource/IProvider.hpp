/**
 * Resource provider interface.
 */

#pragma once

#include <string_view>

namespace catalyst::resource
{
    /**
     * @interface IProvider
     * @brief The IProvider interface defines a contract for resource providers that can load and manage resources such as textures, models, or other assets. Implementations of this interface are responsible for providing access to resources based on identifiers (e.g. file paths, URLs) and may include functionality for caching, asynchronous loading, and resource lifecycle management. By defining a common interface for resource providers, we can allow for flexible and interchangeable implementations that can be easily integrated into different parts of the application.
     */
    class IProvider
    {
    public:
        virtual ~IProvider() = default;

        /**
         * @fn load_resource
         * @brief Loads a resource based on the provided identifier. The specific type of resource and the loading mechanism will depend on the implementation of the provider. This function may return a handle or pointer to the loaded resource, or it may manage resources internally and provide access through other means.
         * @param identifier A string representing the unique identifier for the resource to be loaded (e.g. file path, URL).
         * @return A handle or pointer to the loaded resource, or an appropriate status indicating success or failure of the loading operation.
         */
        virtual void *load_resource(const std::string_view& identifier) = 0;
    };
}