#version 460 core

/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

layout(location = 0) in vec3 in_color;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(in_color, 1.0);
}
