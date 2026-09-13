/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief `catalyst::core::detail::move_only_function`, a local stand-in for
 * `std::move_only_function` (P0288).
 * @details It exists because libc++ does not implement `std::move_only_function`, and that one
 * absence was enough to make Catalyst unbuildable on every Clang and on macOS -- libc++ is the
 * standard library Apple ships, and there is no swapping it out there. Two modules needed the type,
 * so it lives here rather than being written twice.
 *
 * It is deliberately not a full P0288 implementation. There is no small-object optimisation, and
 * none of the cv-, ref- or noexcept-qualified specialisations: every caller in Catalyst stores a
 * lambda in a registry and calls it through a non-const lvalue, which is the plain `R(Args...)`
 * form. What it does promise is what those callers rely on -- move-only, type-erased, callable, and
 * contextually convertible to bool.
 *
 * Why this is in `core`: it is vocabulary, used by `catalyst::events` (listener and middleware
 * storage) and `catalyst::logging` (middleware storage), and neither module should depend on the
 * other. The header is self-contained, so including it costs a consumer nothing at link time.
 */

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace catalyst::core::detail
{

    template <typename Signature>
    class move_only_function;

    /**
     * @brief A move-only, type-erased callable: `std::function` without the copyability, which is
     * what lets it hold a lambda that captured a `unique_ptr`, a coroutine handle or a promise.
     * @tparam R The return type.
     * @tparam Args The call arguments.
     */
    template <typename R, typename... Args>
    class move_only_function<R(Args...)>
    {
    public:
        using result_type = R;

        move_only_function() noexcept = default;
        move_only_function(std::nullptr_t) noexcept {}

        /**
         * @brief Wraps any callable that can be invoked as `R(Args...)`.
         * @details The constraint excludes `move_only_function` itself so that this never competes
         * with the move constructor, which would otherwise be a better match for a non-const
         * lvalue and quietly wrap one function inside another.
         */
        template <typename F>
            requires(!std::is_same_v<std::remove_cvref_t<F>, move_only_function>) &&
                    std::is_invocable_r_v<R, std::decay_t<F> &, Args...>
        move_only_function(F &&fn) : held_(std::make_unique<model<std::decay_t<F>>>(std::forward<F>(fn)))
        {
        }

        move_only_function(move_only_function &&) noexcept = default;
        move_only_function &operator=(move_only_function &&) noexcept = default;

        move_only_function(const move_only_function &) = delete;
        move_only_function &operator=(const move_only_function &) = delete;

        ~move_only_function() = default;

        move_only_function &operator=(std::nullptr_t) noexcept
        {
            held_.reset();
            return *this;
        }

        /**
         * @brief Invokes the stored callable.
         * @warning Calling an empty `move_only_function` is undefined, exactly as it is for
         * `std::move_only_function` and `std::function`'s call operator. Check with `operator bool`
         * where emptiness is possible.
         */
        R operator()(Args... args) { return held_->call(std::forward<Args>(args)...); }

        /** @brief True when a callable is held. */
        [[nodiscard]] explicit operator bool() const noexcept { return held_ != nullptr; }

        void swap(move_only_function &other) noexcept { held_.swap(other.held_); }

        friend void swap(move_only_function &a, move_only_function &b) noexcept { a.swap(b); }

        [[nodiscard]] friend bool operator==(const move_only_function &fn, std::nullptr_t) noexcept { return !fn; }

    private:
        struct callable_base
        {
            virtual ~callable_base() = default;
            virtual R call(Args... args) = 0;
        };

        template <typename F>
        struct model final : callable_base
        {
            F fn;

            explicit model(F fn) : fn(std::move(fn)) {}

            R call(Args... args) override
            {
                // `if constexpr` rather than a plain `return`: a callable whose own return type is
                // non-void may still be stored in a `void(...)` signature, and returning its value
                // from a void function would not compile.
                if constexpr (std::is_void_v<R>)
                    std::invoke(fn, std::forward<Args>(args)...);
                else
                    return std::invoke(fn, std::forward<Args>(args)...);
            }
        };

        std::unique_ptr<callable_base> held_;
    };

} // namespace catalyst::core::detail
