/**
 * @file registry.hpp
 * @brief Header for the resource registry, which manages resource providers.
 */

#pragma once

#include "IProvider.hpp"

namespace catalyst::resource
{
    /**
     * @class registry
     * @brief The registry class manages resource providers and allows for registration and retrieval of providers based on resource types or identifiers. It serves as a central point for managing the various resource providers that may be used in an application, enabling flexible and efficient resource management. The registry can support multiple providers for different resource types, allowing for easy integration of new providers as needed.
     */
    class registry
    {
    public:
        /**
         * @fn register_provider
         * @brief Registers a resource provider with the registry. This allows the provider to be used for loading resources of the appropriate type. The specific mechanism for identifying the provider (e.g. by resource type, identifier) will depend on the implementation of the registry.
         * @param provider A pointer to the resource provider to be registered.
         */
        void register_provider(IProvider* provider);

        // Additional functions for retrieving providers, managing resources, etc. would be declared here.
    };
}
