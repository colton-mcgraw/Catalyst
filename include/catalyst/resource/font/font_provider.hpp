/**
 * @file font_provider.hpp
 * @brief Header for the font resource provider, which manages font resources.
 */

#pragma once

#include "catalyst/resource/IProvider.hpp"

namespace catalyst::resource
{
    /**
     * @class font_provider
     * @brief The font_provider class is responsible for managing font resources within the Catalyst Resource library. It implements the IProvider interface, allowing it to load and provide access to font resources based on unique identifiers (e.g. file paths). The font_provider may include functionality for caching loaded fonts, handling different font formats, and managing the lifecycle of font resources to ensure efficient memory usage and performance in applications that require text rendering.
     */
    class font_provider : public IProvider
    {
    public:
        // Implementation of IProvider functions for loading and managing font resources would be declared here.
    };
}