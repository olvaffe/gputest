/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(vertices = 3) out;

layout(location = 0) in vec2 in_position[];
layout(location = 1) in vec3 in_color[];
layout(location = 0) out vec2 out_position[];
layout(location = 1) out vec3 out_color[];

void main()
{
    out_position[gl_InvocationID] = in_position[gl_InvocationID];
    out_color[gl_InvocationID] = in_color[gl_InvocationID];

    gl_TessLevelOuter[0] = 4.0;
    gl_TessLevelOuter[1] = 4.0;
    gl_TessLevelOuter[2] = 4.0;

    gl_TessLevelInner[0] = 3.0;
}
