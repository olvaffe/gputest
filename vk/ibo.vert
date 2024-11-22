/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(set = 0, binding = 0, r32ui) readonly uniform uimageBuffer ibo;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
};

/* simulate storage-texel-buffer.vert from deqp */
void main()
{
    uint raw = imageLoad(ibo, gl_VertexIndex / 2).x;
    uint packed = (gl_VertexIndex % 2 == 0) ? (raw & 0xffff) : (raw >> 16);
    vec2 xy = vec2((packed & 0xff), packed >> 8) / 255.0 * 1.98 - 0.99;

    gl_Position = vec4(xy, 0.0, 1.0);
    gl_PointSize = 1.0;
}
