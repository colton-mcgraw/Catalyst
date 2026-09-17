#include <catalyst/ui/interaction.hpp>
#include <catalyst/ui/layout.hpp>

#include "test_common.hpp"

#include <string>
#include <vector>

using namespace catalyst::ui;
using catalyst::tests::near;

namespace
{
    void run(tree &t, node root, float w, float h)
    {
        layout(t, root, layout_params::for_viewport(extent{w, h}));
    }

    node sized(tree &t, node parent, float w, float h)
    {
        const node n = t.create_child(parent);
        t.mutable_style(n).width = px(w);
        t.mutable_style(n).height = px(h);
        return n;
    }

    /** @brief Records every UI event in order as "name:target-index". */
    struct recorder
    {
        std::vector<std::string> log;
        std::vector<catalyst::events::token> tokens;
        point last_local{};
        point last_delta{};
        pointer_button last_button = pointer_button::none;
        std::uint32_t last_key = 0;
        char32_t last_text = 0;
        node last_focus_previous = null_node;

        explicit recorder(catalyst::events::bus &b)
        {
            const auto name = [](node n) { return is_null(n) ? std::string{"null"} : std::to_string(n.index); };
            tokens.push_back(b.add_listener<pointer_enter_event>(
                [&, name](const pointer_enter_event &e)
                {
                    log.push_back("enter:" + name(e.target));
                    last_local = e.local;
                }));
            tokens.push_back(b.add_listener<pointer_leave_event>([&, name](const pointer_leave_event &e)
                                                                 { log.push_back("leave:" + name(e.target)); }));
            tokens.push_back(b.add_listener<pointer_move_event>(
                [&, name](const pointer_move_event &e)
                {
                    log.push_back("move:" + name(e.target));
                    last_local = e.local;
                    last_delta = e.delta;
                }));
            tokens.push_back(b.add_listener<pointer_down_event>(
                [&, name](const pointer_down_event &e)
                {
                    log.push_back("down:" + name(e.target));
                    last_button = e.button;
                }));
            tokens.push_back(b.add_listener<pointer_up_event>([&, name](const pointer_up_event &e)
                                                              { log.push_back("up:" + name(e.target)); }));
            tokens.push_back(b.add_listener<click_event>(
                [&, name](const click_event &e)
                {
                    log.push_back("click:" + name(e.target));
                    last_button = e.button;
                }));
            tokens.push_back(b.add_listener<wheel_event>(
                [&, name](const wheel_event &e)
                {
                    log.push_back("wheel:" + name(e.target));
                    last_delta = e.delta;
                }));
            tokens.push_back(b.add_listener<key_down_event>(
                [&, name](const key_down_event &e)
                {
                    log.push_back("keydown:" + name(e.target));
                    last_key = e.key;
                }));
            tokens.push_back(b.add_listener<key_up_event>([&, name](const key_up_event &e)
                                                          { log.push_back("keyup:" + name(e.target)); }));
            tokens.push_back(b.add_listener<text_input_event>(
                [&, name](const text_input_event &e)
                {
                    log.push_back("text:" + name(e.target));
                    last_text = e.code_point;
                }));
            tokens.push_back(b.add_listener<focus_gained_event>(
                [&, name](const focus_gained_event &e)
                {
                    log.push_back("focus:" + name(e.target));
                    last_focus_previous = e.previous;
                }));
            tokens.push_back(b.add_listener<focus_lost_event>([&, name](const focus_lost_event &e)
                                                              { log.push_back("blur:" + name(e.target)); }));
        }

        [[nodiscard]] std::string joined() const
        {
            std::string out;
            for (const std::string &s : log)
            {
                if (!out.empty())
                    out += ' ';
                out += s;
            }
            return out;
        }

        void reset() { log.clear(); }
    };

    struct fixture
    {
        tree t;
        node root;
        node a;
        node b;
        catalyst::events::bus bus;
        recorder rec{bus};
        interaction ix{t, bus};

        fixture()
        {
            root = t.create();
            t.mutable_style(root).width = px(200.0f);
            t.mutable_style(root).height = px(100.0f);
            a = sized(t, root, 100.0f, 100.0f);
            b = sized(t, root, 100.0f, 100.0f);
            run(t, root, 200.0f, 100.0f);
            ix.set_root(root);
        }
    };

    void test_hover_enter_leave_move()
    {
        fixture f;
        f.ix.pointer_moved(point{10.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() == "enter:" + std::to_string(f.a.index) + " move:" + std::to_string(f.a.index));
        CT_REQUIRE(f.ix.hovered() == f.a);
        CT_REQUIRE(near(f.rec.last_delta.x(), 0.0f));

        f.rec.reset();
        f.ix.pointer_moved(point{150.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() == "leave:" + std::to_string(f.a.index) + " enter:" + std::to_string(f.b.index) +
                                         " move:" + std::to_string(f.b.index));
        CT_REQUIRE(near(f.rec.last_delta.x(), 140.0f));
        // Local coordinates are relative to the target's border box.
        CT_REQUIRE(near(f.rec.last_local.x(), 50.0f));
        CT_REQUIRE(near(f.rec.last_local.y(), 10.0f));

        f.rec.reset();
        f.ix.pointer_moved(point{160.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() == "move:" + std::to_string(f.b.index));

        f.rec.reset();
        f.ix.pointer_moved(point{500.0f, 500.0f});
        CT_REQUIRE(f.rec.joined() == "leave:" + std::to_string(f.b.index) + " move:null");
        CT_REQUIRE(is_null(f.ix.hovered()));

        f.ix.pointer_moved(point{10.0f, 10.0f});
        f.rec.reset();
        f.ix.pointer_left();
        CT_REQUIRE(f.rec.joined() == "leave:" + std::to_string(f.a.index));
        CT_REQUIRE(is_null(f.ix.hovered()));
    }

    void test_press_release_click_and_focus()
    {
        fixture f;
        const std::string a = std::to_string(f.a.index);
        const std::string b = std::to_string(f.b.index);

        f.ix.pointer_pressed(pointer_button::left, point{10.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() == "enter:" + a + " down:" + a + " focus:" + a);
        CT_REQUIRE(f.ix.pressed() == f.a);
        CT_REQUIRE(f.ix.focused() == f.a);
        CT_REQUIRE(f.rec.last_button == pointer_button::left);
        CT_REQUIRE(is_null(f.rec.last_focus_previous));

        f.rec.reset();
        f.ix.pointer_released(pointer_button::left, point{20.0f, 20.0f});
        CT_REQUIRE(f.rec.joined() == "up:" + a + " click:" + a);
        CT_REQUIRE(is_null(f.ix.pressed()));

        // Press on a, release on b: no click.
        f.rec.reset();
        f.ix.pointer_pressed(pointer_button::left, point{10.0f, 10.0f});
        f.ix.pointer_released(pointer_button::left, point{150.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() == "down:" + a + " leave:" + a + " enter:" + b + " up:" + b);

        // The other button releasing does not click either.
        f.rec.reset();
        f.ix.pointer_pressed(pointer_button::left, point{150.0f, 10.0f});
        f.ix.pointer_released(pointer_button::right, point{150.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() == "down:" + b + " blur:" + a + " focus:" + b + " up:" + b);
        CT_REQUIRE(f.rec.last_focus_previous == f.a);
        CT_REQUIRE(f.ix.focused() == f.b);

        // Pressing on nothing clears focus.
        f.rec.reset();
        f.ix.pointer_pressed(pointer_button::left, point{500.0f, 500.0f});
        CT_REQUIRE(f.rec.joined() == "leave:" + b + " down:null blur:" + b);
        CT_REQUIRE(is_null(f.ix.focused()));

        // set_focus is idempotent and validates.
        f.rec.reset();
        f.ix.set_focus(f.a);
        f.ix.set_focus(f.a);
        CT_REQUIRE(f.rec.joined() == "focus:" + a);
        f.ix.set_focus(node{99u, 5u});
        CT_REQUIRE(is_null(f.ix.focused()));
    }

    void test_capture()
    {
        fixture f;
        const std::string a = std::to_string(f.a.index);

        f.ix.pointer_pressed(pointer_button::left, point{10.0f, 10.0f});
        f.ix.capture(f.a);
        CT_REQUIRE(f.ix.captured() == f.a);

        // While captured, everything goes to a, wherever the pointer is; and the release there still
        // counts as a click on a.
        f.rec.reset();
        f.ix.pointer_moved(point{150.0f, 10.0f});
        f.ix.pointer_released(pointer_button::left, point{150.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() == "move:" + a + " up:" + a + " click:" + a);
        CT_REQUIRE(near(f.rec.last_local.x(), 150.0f));

        f.ix.release_capture();
        CT_REQUIRE(is_null(f.ix.captured()));
        f.rec.reset();
        f.ix.pointer_moved(point{150.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() ==
                   "leave:" + a + " enter:" + std::to_string(f.b.index) + " move:" + std::to_string(f.b.index));

        f.ix.capture(node{99u, 5u});
        CT_REQUIRE(is_null(f.ix.captured()));
    }

    void test_keys_text_and_wheel()
    {
        fixture f;
        const std::string a = std::to_string(f.a.index);

        // No focus: keys still go out with a null target, text is dropped.
        f.ix.key_pressed(65u);
        f.ix.text_input(U'x');
        CT_REQUIRE(f.rec.joined() == "keydown:null");

        f.ix.set_focus(f.a);
        f.rec.reset();
        f.ix.key_pressed(66u, modifiers{.shift = true});
        f.ix.key_released(66u);
        f.ix.text_input(U'B');
        CT_REQUIRE(f.rec.joined() == "keydown:" + a + " keyup:" + a + " text:" + a);
        CT_REQUIRE(f.rec.last_key == 66u);
        CT_REQUIRE(f.rec.last_text == U'B');

        f.rec.reset();
        f.ix.wheel(point{0.0f, 3.0f}, point{10.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() == "enter:" + a + " wheel:" + a);
        CT_REQUIRE(near(f.rec.last_delta.y(), 3.0f));
    }

    void test_destroyed_nodes_are_dropped()
    {
        fixture f;
        f.ix.pointer_pressed(pointer_button::left, point{10.0f, 10.0f});
        f.ix.capture(f.a);
        CT_REQUIRE(f.ix.hovered() == f.a && f.ix.focused() == f.a && f.ix.captured() == f.a);

        f.t.destroy(f.a);
        run(f.t, f.root, 200.0f, 100.0f);

        // No leave or blur for a node that no longer exists; the handles are just gone, and the
        // next event is routed by a fresh hit test.
        f.rec.reset();
        f.ix.pointer_moved(point{10.0f, 10.0f});
        CT_REQUIRE(is_null(f.ix.captured()));
        CT_REQUIRE(is_null(f.ix.focused()));
        CT_REQUIRE(f.ix.hovered() == f.b);
        CT_REQUIRE(f.rec.log.front() == "enter:" + std::to_string(f.b.index));

        // Without a root nothing is hit.
        f.ix.set_root(null_node);
        f.rec.reset();
        f.ix.pointer_moved(point{10.0f, 10.0f});
        CT_REQUIRE(f.rec.joined() == "leave:" + std::to_string(f.b.index) + " move:null");
    }

} // namespace

int main()
{
    test_hover_enter_leave_move();
    test_press_release_click_and_focus();
    test_capture();
    test_keys_text_and_wheel();
    test_destroyed_nodes_are_dropped();
    return 0;
}
