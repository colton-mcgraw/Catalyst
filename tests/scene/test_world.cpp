#include <catalyst/scene/world.hpp>

#include "test_common.hpp"

#include <string>
#include <unordered_set>
#include <utility>

using namespace catalyst::scene;

namespace
{
    struct health
    {
        int value = 100;
    };

    struct tag_only
    {
    };

    struct move_only
    {
        int value = 0;
        move_only() = default;
        explicit move_only(int v) : value(v) {}
        move_only(move_only &&) noexcept = default;
        move_only &operator=(move_only &&) noexcept = default;
        move_only(const move_only &) = delete;
        move_only &operator=(const move_only &) = delete;
    };

    void test_create_and_validity()
    {
        world w;
        CT_REQUIRE(w.entity_count() == 0u);
        CT_REQUIRE(!w.is_valid(null_entity));
        CT_REQUIRE(is_null(null_entity));

        const entity a = w.create();
        CT_REQUIRE(w.is_valid(a));
        CT_REQUIRE(!is_null(a));
        CT_REQUIRE(w.entity_count() == 1u);
        CT_REQUIRE(is_null(w.parent_of(a)));
        CT_REQUIRE(w.child_count(a) == 0u);
        CT_REQUIRE(w.roots().size() == 1u);
        CT_REQUIRE(w.roots()[0] == a);

        // A destroyed handle stays invalid after its slot is reused.
        w.destroy(a);
        CT_REQUIRE(!w.is_valid(a));
        CT_REQUIRE(w.entity_count() == 0u);
        const entity b = w.create();
        CT_REQUIRE(b.index == a.index);
        CT_REQUIRE(b.generation != a.generation);
        CT_REQUIRE(w.is_valid(b));
        CT_REQUIRE(!w.is_valid(a));

        // Entities hash, so they can key application tables.
        std::unordered_set<entity> set;
        set.insert(a);
        set.insert(b);
        CT_REQUIRE(set.size() == 2u);
    }

    void test_names()
    {
        world w;
        const entity a = w.create("player");
        CT_REQUIRE(w.name_of(a) == "player");
        w.set_name(a, "hero");
        CT_REQUIRE(w.name_of(a) == "hero");

        const entity b = w.create();
        CT_REQUIRE(w.name_of(b).empty());
        CT_REQUIRE(w.name_of(null_entity).empty());
        w.set_name(null_entity, "ignored");
    }

    void test_hierarchy()
    {
        world w;
        const entity root = w.create();
        const entity a = w.create_child(root);
        const entity b = w.create_child(root);

        CT_REQUIRE(w.child_count(root) == 2u);
        CT_REQUIRE(w.children_of(root)[0] == a);
        CT_REQUIRE(w.children_of(root)[1] == b);
        CT_REQUIRE(w.parent_of(a) == root);
        CT_REQUIRE(w.roots().size() == 1u);

        // Reparenting moves rather than duplicates.
        w.set_parent(b, a);
        CT_REQUIRE(w.child_count(root) == 1u);
        CT_REQUIRE(w.parent_of(b) == a);
        CT_REQUIRE(w.children_of(a)[0] == b);

        // Detaching to the root list.
        w.set_parent(b, null_entity);
        CT_REQUIRE(is_null(w.parent_of(b)));
        CT_REQUIRE(w.child_count(a) == 0u);
        CT_REQUIRE(w.roots().size() == 2u);
        CT_REQUIRE(w.roots()[1] == b);

        // A stale parent is ignored, and so is a cycle.
        const entity gone = w.create();
        w.destroy(gone);
        w.set_parent(b, gone);
        CT_REQUIRE(is_null(w.parent_of(b)));

        w.set_parent(a, a);
        CT_REQUIRE(w.parent_of(a) == root);
        w.set_parent(root, a);
        CT_REQUIRE(is_null(w.parent_of(root)));
        CT_REQUIRE(w.parent_of(a) == root);
    }

    void test_destroy_subtree()
    {
        world w;
        const entity root = w.create();
        const entity a = w.create_child(root);
        const entity aa = w.create_child(a);
        const entity b = w.create_child(root);
        w.add<health>(aa, health{7});
        w.add<health>(b, health{9});

        w.destroy(a);
        CT_REQUIRE(!w.is_valid(a));
        CT_REQUIRE(!w.is_valid(aa));
        CT_REQUIRE(w.is_valid(b));
        CT_REQUIRE(w.child_count(root) == 1u);
        CT_REQUIRE(w.children_of(root)[0] == b);
        CT_REQUIRE(w.entity_count() == 2u);

        // The destroyed subtree's components went with it; the survivor's did not.
        CT_REQUIRE(w.count<health>() == 1u);
        CT_REQUIRE(w.get<health>(b)->value == 9);
        CT_REQUIRE(w.get<health>(aa) == nullptr);

        // Destroying a root removes it from the root list.
        w.destroy(root);
        CT_REQUIRE(w.entity_count() == 0u);
        CT_REQUIRE(w.roots().empty());
        CT_REQUIRE(w.count<health>() == 0u);
    }

    void test_components()
    {
        world w;
        const entity a = w.create();
        const entity b = w.create();

        CT_REQUIRE(!w.has<health>(a));
        CT_REQUIRE(w.get<health>(a) == nullptr);
        CT_REQUIRE(w.count<health>() == 0u);
        CT_REQUIRE(!w.remove<health>(a));

        health &h = w.add<health>(a);
        CT_REQUIRE(h.value == 100);
        h.value = 42;
        CT_REQUIRE(w.get<health>(a)->value == 42);
        CT_REQUIRE(w.has<health>(a));
        CT_REQUIRE(w.count<health>() == 1u);

        // Adding again replaces rather than duplicates.
        w.add<health>(a, health{5});
        CT_REQUIRE(w.count<health>() == 1u);
        CT_REQUIRE(w.get<health>(a)->value == 5);

        w.add<health>(b, health{6});
        w.add<tag_only>(b);
        CT_REQUIRE(w.count<health>() == 2u);
        CT_REQUIRE(w.count<tag_only>() == 1u);
        CT_REQUIRE(w.entities_with<health>().size() == 2u);

        // Visiting sees every component exactly once and may edit it.
        int visited = 0;
        w.each<health>(
            [&](entity, health &hp)
            {
                ++visited;
                hp.value += 1;
            });
        CT_REQUIRE(visited == 2);
        CT_REQUIRE(w.get<health>(a)->value == 6);
        CT_REQUIRE(w.get<health>(b)->value == 7);

        const world &cw = w;
        int sum = 0;
        cw.each<health>([&](entity, const health &hp) { sum += hp.value; });
        CT_REQUIRE(sum == 13);

        // Removal swaps the last element into the hole; the survivor is still found.
        CT_REQUIRE(w.remove<health>(a));
        CT_REQUIRE(!w.has<health>(a));
        CT_REQUIRE(w.get<health>(b)->value == 7);
        CT_REQUIRE(w.count<health>() == 1u);
        CT_REQUIRE(w.entities_with<health>()[0] == b);

        // An invalid entity gets a scratch value rather than a crash, and stores nothing.
        health &scratch = w.add<health>(null_entity, health{1});
        scratch.value = 99;
        CT_REQUIRE(w.count<health>() == 1u);
        CT_REQUIRE(w.get<health>(null_entity) == nullptr);

        // Move-only components are fine: the bar is movable, not copyable.
        w.add<move_only>(a, move_only{3});
        CT_REQUIRE(w.get<move_only>(a)->value == 3);
    }

    void test_stale_handle_misses_reused_slot()
    {
        world w;
        const entity a = w.create();
        w.add<health>(a, health{1});
        w.destroy(a);

        const entity b = w.create();
        CT_REQUIRE(b.index == a.index);
        w.add<health>(b, health{2});

        CT_REQUIRE(w.get<health>(a) == nullptr);
        CT_REQUIRE(w.get<health>(b)->value == 2);
        CT_REQUIRE(!w.remove<health>(a));
        CT_REQUIRE(w.count<health>() == 1u);
    }

    void test_clear()
    {
        world w;
        const entity a = w.create();
        const entity b = w.create_child(a);
        w.add<health>(b);

        w.clear();
        CT_REQUIRE(w.entity_count() == 0u);
        CT_REQUIRE(!w.is_valid(a));
        CT_REQUIRE(!w.is_valid(b));
        CT_REQUIRE(w.roots().empty());
        CT_REQUIRE(w.count<health>() == 0u);

        // Handles from before the clear do not alias entities created after it.
        const entity c = w.create();
        CT_REQUIRE(w.is_valid(c));
        CT_REQUIRE(!w.is_valid(a));
        CT_REQUIRE(!w.is_valid(b));
    }

    void test_move()
    {
        world w;
        const entity a = w.create("a");
        w.add<health>(a, health{3});

        world moved = std::move(w);
        CT_REQUIRE(moved.is_valid(a));
        CT_REQUIRE(moved.name_of(a) == "a");
        CT_REQUIRE(moved.get<health>(a)->value == 3);

        world assigned;
        (void)assigned.create();
        assigned = std::move(moved);
        CT_REQUIRE(assigned.entity_count() == 1u);
        CT_REQUIRE(assigned.is_valid(a));
    }

} // namespace

int main()
{
    test_create_and_validity();
    test_names();
    test_hierarchy();
    test_destroy_subtree();
    test_components();
    test_stale_handle_misses_reused_slot();
    test_clear();
    test_move();
    return 0;
}
