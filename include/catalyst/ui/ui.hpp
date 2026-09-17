/**
 * @file ui.hpp
 * @brief Umbrella header for the catalyst::ui module.
 * @details Including this header pulls in the whole UI module: the CSS-like measurement types, the
 * geometry and color primitives, the style properties, the retained node tree, the layout engine,
 * the paint pass and the draw batch it fills, hit testing, the interaction state machine and its
 * events, and the text seam. Individual headers can be included instead when only part of the
 * module is needed.
 * License: MIT (see LICENSE).
 */

#pragma once

#include <catalyst/ui/batch.hpp>
#include <catalyst/ui/color.hpp>
#include <catalyst/ui/events.hpp>
#include <catalyst/ui/geometry.hpp>
#include <catalyst/ui/hit_test.hpp>
#include <catalyst/ui/interaction.hpp>
#include <catalyst/ui/layout.hpp>
#include <catalyst/ui/measurement.hpp>
#include <catalyst/ui/node.hpp>
#include <catalyst/ui/paint.hpp>
#include <catalyst/ui/style.hpp>
#include <catalyst/ui/text.hpp>

/**
 * @namespace catalyst::ui
 * @brief Retained, declarative user interface: a tree of styled nodes, a flexbox layout engine, and
 * the draw list they produce.
 * @details Applications build a `tree` of `node` handles, describe each one with a `style` written in
 * CSS-like units (`px`, `dp`, `em`, `rem`, `%`, `vw`, `vh`, and physical units), call `layout` to
 * turn that description into positioned boxes, and `paint` to turn the boxes into a `render_batch`.
 * The module never talks to a graphics API itself: the batch is backend-agnostic vertices, indices
 * and scissored draw commands that a renderer submits, which keeps layout and painting testable
 * without a device. Content the module cannot see, such as text, enters through a pair of callbacks
 * on the node: `measure_fn` reports its size to layout and `paint_fn` draws it. Input arrives
 * through `interaction`, which hit-tests the tree and publishes `pointer_enter_event`, `click_event`
 * and the rest on a `catalyst::events::bus`.
 */
namespace catalyst::ui
{

    /**
     * @fn module_name
     * @brief Returns the name of this module as a string. This can be used for logging, debugging, or any situation
     * where you want to identify the module by name.
     * @return A string literal representing the name of this module.
     */
    const char *module_name();

} // namespace catalyst::ui
