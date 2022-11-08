#version 460 core

/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

layout(location = 0) in vec2 in_position;
layout(location = 0) out vec2 out_texcoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(in_position, 0.0, 1.0);
    out_texcoord = (in_position + 1.0) / 2.0;
}
