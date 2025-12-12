#version 310 es

/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

precision mediump float;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 encoded = vec3(1.0, 255.0, 65025.0) * gl_FragCoord.z;
    encoded = fract(encoded);
    encoded.xy -= encoded.yz / 255.0;

    out_color = vec4(encoded, 0.3);
}
