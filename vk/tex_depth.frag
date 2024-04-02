/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(binding = 0) uniform usampler2D tex;

layout(location = 0) in vec2 in_texcoord;
layout(location = 0) out vec4 out_color;

void main()
{
    uvec4 v = texture(tex, in_texcoord * 2.0);
    out_color = vec4(float(v.r) / 10.0, float(v.g), float(v.b), float(v.a));
}
