/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Tests for catalyst::core: the version string and move_only_function.
 */

#include <catalyst/config.hpp>
#include <catalyst/core/core.hpp>
#include <catalyst/core/detail/move_only_function.hpp>

#include "../test_common.hpp"

#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
    using catalyst::core::detail::move_only_function;

    void test_version()
    {
        // The runtime string and the compile-time macros come from the same configure, and the
        // consumer fixture (tests/consumer) relies on that agreement across an install boundary.
        CT_REQUIRE(std::string_view{catalyst::version()} == CATALYST_VERSION_STRING);
        static_assert(CATALYST_VERSION ==
                      CATALYST_VERSION_ENCODE(CATALYST_VERSION_MAJOR, CATALYST_VERSION_MINOR, CATALYST_VERSION_PATCH));
        static_assert(CATALYST_VERSION_ENCODE(0, 2, 0) > CATALYST_VERSION_ENCODE(0, 1, 99));
        static_assert(CATALYST_VERSION_ENCODE(1, 0, 0) > CATALYST_VERSION_ENCODE(0, 99, 99));

        CT_REQUIRE(std::string_view{catalyst::core::module_name()} == "catalyst::core");
    }

    void test_move_only_function_empty()
    {
        move_only_function<int(int)> fn;
        CT_REQUIRE(!fn);
        CT_REQUIRE(fn == nullptr);

        move_only_function<int(int)> from_null{nullptr};
        CT_REQUIRE(!from_null);
    }

    void test_move_only_function_holds_move_only_callable()
    {
        // The reason the type exists: std::function cannot hold this lambda, because a unique_ptr
        // capture makes it non-copyable.
        move_only_function<int(int)> fn = [held = std::make_unique<int>(3)](int x) { return *held + x; };
        CT_REQUIRE(fn);
        CT_REQUIRE(fn(4) == 7);

        static_assert(!std::is_copy_constructible_v<move_only_function<int(int)>>);
        static_assert(!std::is_copy_assignable_v<move_only_function<int(int)>>);
        static_assert(std::is_nothrow_move_constructible_v<move_only_function<int(int)>>);
    }

    void test_move_only_function_move_and_reset()
    {
        move_only_function<int()> a = [] { return 1; };
        move_only_function<int()> b = std::move(a);
        CT_REQUIRE(!a); // NOLINT(bugprone-use-after-move): the moved-from state is the point.
        CT_REQUIRE(b);
        CT_REQUIRE(b() == 1);

        a = std::move(b);
        CT_REQUIRE(a && !b); // NOLINT(bugprone-use-after-move)
        CT_REQUIRE(a() == 1);

        a = nullptr;
        CT_REQUIRE(!a);

        // Assigning a new callable over an existing one replaces it.
        a = [] { return 2; };
        a = [] { return 3; };
        CT_REQUIRE(a() == 3);
    }

    void test_move_only_function_swap()
    {
        move_only_function<int()> one = [] { return 1; };
        move_only_function<int()> two = [] { return 2; };

        one.swap(two);
        CT_REQUIRE(one() == 2 && two() == 1);

        swap(one, two);
        CT_REQUIRE(one() == 1 && two() == 2);

        move_only_function<int()> empty;
        swap(one, empty);
        CT_REQUIRE(!one && empty() == 1);
    }

    void test_move_only_function_arguments()
    {
        // References are forwarded, not copied: a callback that mutates its argument has to see
        // the caller's object.
        move_only_function<void(int &)> increment = [](int &v) { ++v; };
        int value = 0;
        increment(value);
        increment(value);
        CT_REQUIRE(value == 2);

        move_only_function<std::unique_ptr<int>(std::unique_ptr<int>)> pass_through = [](std::unique_ptr<int> p)
        { return p; };
        auto out = pass_through(std::make_unique<int>(9));
        CT_REQUIRE(out && *out == 9);

        struct counter
        {
            int calls = 0;
            void operator()() { ++calls; }
        };
        counter c;
        move_only_function<void()> by_ref = std::ref(c);
        by_ref();
        by_ref();
        CT_REQUIRE(c.calls == 2);
    }

    void test_move_only_function_destroys_callable()
    {
        int alive = 0;
        struct tracker
        {
            int *alive;
            explicit tracker(int *a) : alive(a) { ++*alive; }
            tracker(tracker &&other) noexcept : alive(other.alive) { other.alive = nullptr; }
            tracker(const tracker &) = delete;
            ~tracker()
            {
                if (alive)
                    --*alive;
            }
            void operator()() const {}
        };

        {
            move_only_function<void()> fn = tracker{&alive};
            CT_REQUIRE(alive == 1);
            fn = nullptr;
            CT_REQUIRE(alive == 0);

            fn = tracker{&alive};
            CT_REQUIRE(alive == 1);
        }
        CT_REQUIRE(alive == 0);
    }
} // namespace

int main()
{
    test_version();
    test_move_only_function_empty();
    test_move_only_function_holds_move_only_callable();
    test_move_only_function_move_and_reset();
    test_move_only_function_swap();
    test_move_only_function_arguments();
    test_move_only_function_destroys_callable();
    return 0;
}
