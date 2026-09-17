#include <catalyst/ui/batch.hpp>

#include "test_common.hpp"

#include <array>

using namespace catalyst::ui;
using catalyst::tests::near;

namespace
{
    rect box(float x, float y, float w, float h)
    {
        return rect::from_xywh(x, y, w, h);
    }

    void test_rect_geometry_and_merging()
    {
        render_batch batch;
        batch_builder b{batch};
        CT_REQUIRE(batch.empty());

        b.add_rect(box(0, 0, 10, 10), colors::red);
        CT_REQUIRE(batch.vertices.size() == 4u);
        CT_REQUIRE(batch.indices.size() == 6u);
        CT_REQUIRE(batch.commands.size() == 1u);
        CT_REQUIRE(batch.commands[0].first_index == 0u);
        CT_REQUIRE(batch.commands[0].index_count == 6u);
        CT_REQUIRE(batch.commands[0].texture == no_texture);
        CT_REQUIRE(batch.vertices[0].color == colors::red.to_rgba8());
        CT_REQUIRE(near(batch.vertices[2].position.x(), 10.0f));
        CT_REQUIRE(near(batch.vertices[2].position.y(), 10.0f));

        // Same clip, same texture: the second rectangle joins the first command.
        b.add_rect(box(20, 0, 10, 10), colors::blue);
        CT_REQUIRE(batch.commands.size() == 1u);
        CT_REQUIRE(batch.commands[0].index_count == 12u);

        // A texture change opens a new command; changing back opens another, never merges backwards.
        b.set_texture(7u);
        b.add_rect(box(40, 0, 10, 10), box(0, 0, 1, 1), colors::white);
        CT_REQUIRE(batch.commands.size() == 2u);
        CT_REQUIRE(batch.commands[1].texture == 7u);
        CT_REQUIRE(batch.commands[1].first_index == 12u);
        CT_REQUIRE(near(batch.vertices[10].uv.x(), 1.0f));
        b.set_texture(no_texture);
        b.add_rect(box(60, 0, 10, 10), colors::red);
        CT_REQUIRE(batch.commands.size() == 3u);

        // Transparent geometry is not emitted at all.
        b.add_rect(box(80, 0, 10, 10), colors::transparent);
        CT_REQUIRE(batch.commands.size() == 3u);
        CT_REQUIRE(batch.vertices.size() == 16u);

        batch.clear();
        CT_REQUIRE(batch.empty());
        CT_REQUIRE(batch.vertices.empty());
    }

    void test_clip_stack_and_culling()
    {
        render_batch batch;
        batch_builder b{batch};
        CT_REQUIRE(!b.clip().is_empty());

        b.push_clip(box(0, 0, 100, 100));
        CT_REQUIRE(b.clip() == box(0, 0, 100, 100));

        // Nested clips only shrink.
        b.push_clip(box(50, 50, 100, 100));
        CT_REQUIRE(b.clip() == box(50, 50, 50, 50));

        b.add_rect(box(60, 60, 10, 10), colors::red); // inside
        b.add_rect(box(0, 0, 10, 10), colors::red);   // outside the inner clip: culled
        b.add_rect(box(95, 95, 10, 10), colors::red); // straddles: kept, the scissor trims it
        CT_REQUIRE(batch.vertices.size() == 8u);
        CT_REQUIRE(batch.commands.size() == 1u);
        CT_REQUIRE(batch.commands[0].clip == box(50, 50, 50, 50));

        b.pop_clip();
        CT_REQUIRE(b.clip() == box(0, 0, 100, 100));
        b.add_rect(box(0, 0, 10, 10), colors::red);
        CT_REQUIRE(batch.commands.size() == 2u);
        CT_REQUIRE(batch.commands[1].clip == box(0, 0, 100, 100));

        // The bottom of the stack cannot be popped away.
        b.pop_clip();
        b.pop_clip();
        CT_REQUIRE(!b.clip().is_empty());

        // An empty clip culls everything.
        b.push_clip(box(200, 200, 0, 0));
        b.add_rect(box(0, 0, 1000, 1000), colors::red);
        CT_REQUIRE(batch.vertices.size() == 12u);
    }

    void test_rounded_rect()
    {
        render_batch batch;
        batch_builder b{batch};

        // All-zero radii take the four-vertex path.
        b.add_rounded_rect(box(0, 0, 10, 10), corners_px{}, colors::red);
        CT_REQUIRE(batch.vertices.size() == 4u);

        batch.clear();
        b.add_rounded_rect(box(0, 0, 100, 100), corners_px::all(10.0f), colors::red);
        const std::uint32_t segments = corner_segments(10.0f);
        CT_REQUIRE(segments >= 2u);
        // A centre vertex plus 4 * (segments + 1) outline points, one triangle per outline point.
        CT_REQUIRE(batch.vertices.size() == 1u + 4u * (segments + 1u));
        CT_REQUIRE(batch.indices.size() == 3u * 4u * (segments + 1u));

        // Every outline point lies inside the box, and the leftmost point of the top-left arc sits
        // on the left edge at y = radius.
        for (std::size_t i = 1; i < batch.vertices.size(); ++i)
        {
            const point p = batch.vertices[i].position;
            CT_REQUIRE(p.x() >= -1e-3f && p.x() <= 100.0f + 1e-3f);
            CT_REQUIRE(p.y() >= -1e-3f && p.y() <= 100.0f + 1e-3f);
        }
        CT_REQUIRE(near(batch.vertices[1].position.x(), 0.0f));
        CT_REQUIRE(near(batch.vertices[1].position.y(), 10.0f));
        // The last point of the top-left arc is the top edge at x = radius.
        CT_REQUIRE(near(batch.vertices[1u + segments].position.x(), 10.0f));
        CT_REQUIRE(near(batch.vertices[1u + segments].position.y(), 0.0f));

        // Radii larger than the box are clamped as CSS clamps them, so the shape stays inside it.
        batch.clear();
        b.add_rounded_rect(box(0, 0, 20, 100), corners_px::all(50.0f), colors::red);
        for (const vertex &v : batch.vertices)
        {
            CT_REQUIRE(v.position.x() >= -1e-3f && v.position.x() <= 20.0f + 1e-3f);
            CT_REQUIRE(v.position.y() >= -1e-3f && v.position.y() <= 100.0f + 1e-3f);
        }

        CT_REQUIRE(corner_segments(0.0f) == 1u);
        CT_REQUIRE(corner_segments(4.0f) <= corner_segments(100.0f));
        CT_REQUIRE(corner_segments(1.0e6f) == 32u);
    }

    void test_border()
    {
        render_batch batch;
        batch_builder b{batch};

        // Square corners: a ring of two 4-point outlines, eight vertices and eight triangles.
        b.add_border(box(0, 0, 100, 50), edges_px::all(2.0f), corners_px{}, colors::black);
        CT_REQUIRE(batch.vertices.size() == 8u);
        CT_REQUIRE(batch.indices.size() == 24u);
        // Outer ring first, then inner ring inset by the width.
        CT_REQUIRE(near(batch.vertices[0].position.x(), 0.0f));
        CT_REQUIRE(near(batch.vertices[4].position.x(), 2.0f));
        CT_REQUIRE(near(batch.vertices[4].position.y(), 2.0f));

        // Zero widths draw nothing.
        batch.clear();
        b.add_border(box(0, 0, 100, 50), edges_px{}, corners_px{}, colors::black);
        CT_REQUIRE(batch.empty());

        // Widths that meet in the middle become a fill.
        b.add_border(box(0, 0, 10, 10), edges_px::all(5.0f), corners_px{}, colors::black);
        CT_REQUIRE(batch.vertices.size() == 4u);

        // Rounded: both rings have the same point count.
        batch.clear();
        b.add_border(box(0, 0, 100, 100), edges_px::all(4.0f), corners_px::all(16.0f), colors::black);
        const std::uint32_t segments = corner_segments(16.0f);
        CT_REQUIRE(batch.vertices.size() == 2u * 4u * (segments + 1u));
        CT_REQUIRE(batch.indices.size() == 6u * 4u * (segments + 1u));
    }

    void test_line_and_triangles()
    {
        render_batch batch;
        batch_builder b{batch};

        // The quad straddles the segment: the first two vertices are half a thickness to one side,
        // the last two to the other. For a left-to-right line the offset is (0, +t/2) first.
        b.add_line(point{0, 0}, point{10, 0}, 2.0f, colors::red);
        CT_REQUIRE(batch.vertices.size() == 4u);
        CT_REQUIRE(near(batch.vertices[0].position.y(), 1.0f));
        CT_REQUIRE(near(batch.vertices[2].position.y(), -1.0f));
        CT_REQUIRE(near(batch.vertices[1].position.x(), 10.0f));

        // A hairline exactly on the clip edge is still emitted.
        b.push_clip(box(0, 0, 100, 100));
        b.add_line(point{0, 0}, point{100, 0}, 1.0f, colors::red);
        CT_REQUIRE(batch.vertices.size() == 8u);
        b.pop_clip();

        const std::array<vertex, 3> tri = {
            vertex{point{0, 0}, point{}, 0xFFu},
            vertex{point{5, 0}, point{}, 0xFFu},
            vertex{point{0, 5}, point{}, 0xFFu},
        };
        const std::array<index, 3> idx = {0u, 1u, 2u};
        b.add_triangles(tri, idx);
        CT_REQUIRE(batch.vertices.size() == 11u);
        // Rebased onto the batch's own vertex range.
        CT_REQUIRE(batch.indices.back() == 10u);

        b.push_clip(box(500, 500, 10, 10));
        b.add_triangles(tri, idx);
        CT_REQUIRE(batch.vertices.size() == 11u);
    }

} // namespace

int main()
{
    test_rect_geometry_and_merging();
    test_clip_stack_and_culling();
    test_rounded_rect();
    test_border();
    test_line_and_triangles();
    return 0;
}
