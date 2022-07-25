/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec2 out_position;
layout(location = 1) out vec3 out_color;

void main()
{
    out_position = in_position;
    out_color = in_color;
}
