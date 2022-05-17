/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 420 core

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 in_texcoord;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(tex, in_texcoord);
}
