/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform CONSTS {
    vec4 color;
} consts;

void main()
{
    out_color = consts.color;
}
