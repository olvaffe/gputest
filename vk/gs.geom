/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(points) in;
layout(triangle_strip, max_vertices = 60) out;

layout(location = 0) in vec2 in_position[];
layout(location = 1) in vec3 in_color[];
layout(location = 2) in float in_radius[];
layout(location = 0) out vec3 out_color;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    const uint tri_count = 20;
    const float rad = 3.1415926 * 2.0 / tri_count;

    out_color = in_color[0];

    for (uint i = 0; i < 20; i++) {
        const vec2 origin = in_position[0];
        const vec2 offset1 = vec2(cos(rad * i), sin(rad * i)) * in_radius[0];
        const vec2 offset2 =
            vec2(cos(rad * (i + 1)), sin(rad * (i + 1))) * in_radius[0];

        gl_Position = vec4(origin, 0.0, 1.0);
        EmitVertex();

        gl_Position = vec4(origin + offset1, 0.0, 1.0);
        EmitVertex();

        gl_Position = vec4(origin + offset2, 0.0, 1.0);
        EmitVertex();

        EndPrimitive();
    }
}
