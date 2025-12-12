#version 310 es

/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#extension GL_OES_shader_io_blocks: enable

layout(location = 0) in vec2 in_position;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(in_position, 0.0, 1.0);
}
