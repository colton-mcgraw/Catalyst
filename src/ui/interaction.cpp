/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The interaction state machine declared in interaction.hpp.
 */

#include <catalyst/ui/hit_test.hpp>
#include <catalyst/ui/interaction.hpp>

namespace catalyst::ui
{

    interaction::interaction(const tree &t, events::bus &b) noexcept : tree_(&t), bus_(&b) {}

    void interaction::set_root(node root) noexcept
    {
        root_ = root;
    }

    void interaction::validate() noexcept
    {
        const auto drop_if_stale = [this](node &n)
        {
            if (!is_null(n) && !tree_->is_valid(n))
                n = null_node;
        };
        drop_if_stale(root_);
        drop_if_stale(hovered_);
        drop_if_stale(focused_);
        drop_if_stale(captured_);
        drop_if_stale(pressed_);
        if (is_null(pressed_))
            pressed_button_ = pointer_button::none;
    }

    node interaction::target_at(const point &p) const noexcept
    {
        if (!is_null(captured_))
            return captured_;
        return hit_test(*tree_, root_, p);
    }

    point interaction::local_of(node n, const point &p) const noexcept
    {
        if (is_null(n))
            return p;
        return p - tree_->layout_of(n).position;
    }

    void interaction::update_hover(node n, const point &position, const modifiers &mods)
    {
        if (n == hovered_)
            return;

        if (!is_null(hovered_))
        {
            pointer_leave_event leave;
            leave.target = hovered_;
            leave.position = position;
            leave.local = local_of(hovered_, position);
            leave.mods = mods;
            bus_->dispatch(leave);
        }

        hovered_ = n;

        if (!is_null(n))
        {
            pointer_enter_event enter;
            enter.target = n;
            enter.position = position;
            enter.local = local_of(n, position);
            enter.mods = mods;
            bus_->dispatch(enter);
        }
    }

    void interaction::pointer_moved(const point &position, const modifiers &mods)
    {
        validate();

        const point delta = has_position_ ? position - last_position_ : point{};
        last_position_ = position;
        has_position_ = true;

        const node target = target_at(position);
        update_hover(target, position, mods);

        pointer_move_event move;
        move.target = target;
        move.position = position;
        move.local = local_of(target, position);
        move.mods = mods;
        move.delta = delta;
        bus_->dispatch(move);
    }

    void interaction::pointer_pressed(pointer_button button, const point &position, const modifiers &mods)
    {
        validate();
        last_position_ = position;
        has_position_ = true;

        const node target = target_at(position);
        update_hover(target, position, mods);

        pressed_ = target;
        pressed_button_ = button;

        pointer_down_event down;
        down.target = target;
        down.position = position;
        down.local = local_of(target, position);
        down.mods = mods;
        down.button = button;
        bus_->dispatch(down);

        set_focus(target);
    }

    void interaction::pointer_released(pointer_button button, const point &position, const modifiers &mods)
    {
        validate();
        last_position_ = position;
        has_position_ = true;

        const node target = target_at(position);
        update_hover(target, position, mods);

        pointer_up_event up;
        up.target = target;
        up.position = position;
        up.local = local_of(target, position);
        up.mods = mods;
        up.button = button;
        bus_->dispatch(up);

        // A click is a press and a release on the same node with the same button. The press is
        // forgotten either way: a release with the other button ends the gesture too.
        const bool click = !is_null(target) && target == pressed_ && button == pressed_button_;
        pressed_ = null_node;
        pressed_button_ = pointer_button::none;

        if (click)
        {
            click_event c;
            c.target = target;
            c.position = position;
            c.local = local_of(target, position);
            c.mods = mods;
            c.button = button;
            bus_->dispatch(c);
        }
    }

    void interaction::pointer_left()
    {
        validate();
        update_hover(null_node, last_position_, modifiers{});
        has_position_ = false;
    }

    void interaction::wheel(const point &delta, const point &position, const modifiers &mods)
    {
        validate();
        const node target = target_at(position);
        update_hover(target, position, mods);

        wheel_event w;
        w.target = target;
        w.position = position;
        w.local = local_of(target, position);
        w.mods = mods;
        w.delta = delta;
        bus_->dispatch(w);
    }

    void interaction::key_pressed(std::uint32_t key, const modifiers &mods, bool repeat)
    {
        validate();
        key_down_event e;
        e.target = focused_;
        e.key = key;
        e.mods = mods;
        e.repeat = repeat;
        bus_->dispatch(e);
    }

    void interaction::key_released(std::uint32_t key, const modifiers &mods)
    {
        validate();
        key_up_event e;
        e.target = focused_;
        e.key = key;
        e.mods = mods;
        bus_->dispatch(e);
    }

    void interaction::text_input(char32_t code_point)
    {
        validate();
        if (is_null(focused_))
            return;
        text_input_event e;
        e.target = focused_;
        e.code_point = code_point;
        bus_->dispatch(e);
    }

    void interaction::set_focus(node n)
    {
        validate();
        if (!is_null(n) && !tree_->is_valid(n))
            n = null_node;
        if (n == focused_)
            return;

        const node previous = focused_;
        if (!is_null(previous))
        {
            focus_lost_event lost;
            lost.target = previous;
            lost.next = n;
            bus_->dispatch(lost);
        }

        focused_ = n;

        if (!is_null(n))
        {
            focus_gained_event gained;
            gained.target = n;
            gained.previous = previous;
            bus_->dispatch(gained);
        }
    }

    void interaction::capture(node n) noexcept
    {
        captured_ = tree_->is_valid(n) ? n : null_node;
    }

    void interaction::release_capture() noexcept
    {
        captured_ = null_node;
    }

} // namespace catalyst::ui
