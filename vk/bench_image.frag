/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform texture2D tex;

layout(location = 0) out vec4 out_color;

void main()
{
    const ivec2 coord = ivec2(gl_FragCoord);
    out_color = texelFetch(tex, coord, 0);
}
