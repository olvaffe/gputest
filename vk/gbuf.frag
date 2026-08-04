#version 450

layout(input_attachment_index = 1, set = 0, binding = 0) uniform subpassInput gbuf;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = subpassLoad(gbuf) + vec4(0.3, 0.0, 0.0, 0.0);
}
