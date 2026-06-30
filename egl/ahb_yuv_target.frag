#version 310 es

/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#extension GL_EXT_YUV_target : require

precision mediump float;

layout(yuv) out vec4 out_color;

uniform vec4 color;

void main()
{
    out_color = color;
}
