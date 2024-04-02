/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(triangles, equal_spacing, ccw) in;

layout(location = 0) in vec2 in_position[];
layout(location = 1) in vec3 in_color[];
layout(location = 0) out vec3 out_color;

out gl_PerVertex {
    vec4 gl_Position;
};

vec2 interpolatePosition()
{
    return in_position[0] * gl_TessCoord.x +
           in_position[1] * gl_TessCoord.y +
           in_position[2] * gl_TessCoord.z;
}

vec3 interpolateColor()
{
    return in_color[0] * gl_TessCoord.x +
           in_color[1] * gl_TessCoord.y +
           in_color[2] * gl_TessCoord.z;
}

void main()
{
    gl_Position = vec4(interpolatePosition(), 0.0, 1.0);
    out_color = interpolateColor();
}
