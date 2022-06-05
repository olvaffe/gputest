/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 420 core

layout(location = 0) in vec2 in_position;
layout(location = 0) out vec2 out_texcoord;

void main()
{
    gl_Position = vec4(in_position, 0.0, 1.0);
    out_texcoord = (in_position + 1.0) / 2.0;
}
