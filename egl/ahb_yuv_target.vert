#version 310 es

/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#extension GL_OES_shader_io_blocks: enable

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    const vec4 vertices[3] = vec4[](
        vec4(-1.0, -1.0, 0.0, 1.0),
        vec4( 1.0, -1.0, 0.0, 1.0),
        vec4( 0.0,  1.0, 0.0, 1.0)
    );
    gl_Position = vertices[gl_VertexID % 3];
}
