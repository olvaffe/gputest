#version 320 es

/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

layout(location = 0) in vec3 in_position;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(in_position, 1.0);
}
