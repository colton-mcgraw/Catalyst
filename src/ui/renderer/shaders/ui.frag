// Fragment stage of the Catalyst UI renderer. Vertex colours are straight alpha; the output is premultiplied, and the
// pipeline blends with (one, one_minus_src_alpha), which draws the same as straight-alpha blending for solid geometry
// and makes a layer's offscreen image composite correctly when it is sampled back.
//
// `pc.mode` says what the bound texture (set 2 / set 3, slot 0) means for this draw:
//   0  solid: the texture is ignored (a white 1x1 is bound so the sampler is never unbound).
//   1  mask: a single-channel coverage atlas, as a glyph rasteriser fills; its red channel scales alpha.
//   2  colour: a straight-alpha colour texture multiplied by the vertex colour, for images and colour glyphs.
//   3  layer: a premultiplied image the renderer drew itself, blended in at `pc.opacity` (group opacity).
#version 450

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;

layout(set = 2, binding = 0) uniform texture2D u_texture;
layout(set = 3, binding = 0) uniform sampler u_sampler;

layout(push_constant) uniform constants
{
    vec2 scale;
    vec2 translate;
    uint mode;
    float opacity;
} pc;

layout(location = 0) out vec4 out_color;

void main()
{
    if (pc.mode == 3u)
    {
        out_color = texture(sampler2D(u_texture, u_sampler), v_uv) * pc.opacity;
        return;
    }

    vec4 c = v_color;
    if (pc.mode == 1u)
        c.a *= texture(sampler2D(u_texture, u_sampler), v_uv).r;
    else if (pc.mode == 2u)
        c *= texture(sampler2D(u_texture, u_sampler), v_uv);

    out_color = vec4(c.rgb * c.a, c.a) * pc.opacity;
}
