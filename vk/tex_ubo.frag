/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 1, binding = 0) uniform UBO {
    vec4 color_scale;
} ubo;

layout(location = 0) in vec2 in_texcoord;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(tex, in_texcoord) * ubo.color_scale;
}
