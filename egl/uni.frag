#version 310 es

/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

precision mediump float;

layout(location = 0) out vec4 out_color;

uniform float cond;
uniform float val1[2];
uniform float val2[2];

void main()
{
    float r = (cond > 0.5 ? val1 : val2)[1];
    float g = (cond > 0.5 ? val2 : val1)[1];
    out_color = vec4(r, g, 0.0, 1.0);
}
