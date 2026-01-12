#version 460 core

/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 out_color;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(in_position, 0.0, 1.0);
    out_color = in_color;
}
