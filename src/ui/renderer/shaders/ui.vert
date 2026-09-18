// Vertex stage of the Catalyst UI renderer: maps a ui::vertex from pixels to clip space.
// Matches `ui::vertex` (vec2 position at 0, vec2 uv at 8, packed RGBA8 colour at 16, read as rgba8_unorm) via the
// vertex_layout in renderer.cpp. The push constants carry the pixel-to-clip transform, which the renderer computes from
// the viewport (or, for a layer pass, from the layer's placement) so the same batch draws at any window size, plus the
// per-draw mode and opacity the fragment stage reads; both stages declare the block whole because Vulkan gives every
// stage the same 128-byte range.
#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(push_constant) uniform constants
{
    vec2 scale;
    vec2 translate;
    uint mode;
    float opacity;
} pc;

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;

void main()
{
    gl_Position = vec4(in_position * pc.scale + pc.translate, 0.0, 1.0);
    v_color = in_color;
    v_uv = in_uv;
}
