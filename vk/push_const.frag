/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform CONSTS {
    vec4 color;
} consts;

layout(set = 0, binding = 0) uniform UBO {
    vec4 color;
} ubo;

void main()
{
    out_color = consts.color + ubo.color;
}
