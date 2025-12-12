#version 310 es

/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#extension GL_OES_shader_io_blocks: enable

uniform vec4 bones[32 * 3];
uniform mat4 mvp;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_bone_index;
layout(location = 2) in vec4 in_bone_weight;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    ivec4 bone_idx = ivec4(in_bone_index);
    mat4 bone0 = mat4(
        bones[bone_idx.x * 3 + 0],
        bones[bone_idx.x * 3 + 1],
        bones[bone_idx.x * 3 + 2],
        vec4(0.0, 0.0, 0.0, 1.0)
    );
    mat4 bone1 = mat4(
        bones[bone_idx.y * 3 + 0],
        bones[bone_idx.y * 3 + 1],
        bones[bone_idx.y * 3 + 2],
        vec4(0.0, 0.0, 0.0, 1.0)
    );
    mat4 bone2 = mat4(
        bones[bone_idx.z * 3 + 0],
        bones[bone_idx.z * 3 + 1],
        bones[bone_idx.z * 3 + 2],
        vec4(0.0, 0.0, 0.0, 1.0)
    );
    mat4 bone3 = mat4(
        bones[bone_idx.z * 3 + 0],
        bones[bone_idx.z * 3 + 1],
        bones[bone_idx.w * 3 + 2],
        vec4(0.0, 0.0, 0.0, 1.0)
    );
    mat4 bone =
        bone0 * in_bone_weight.x +
        bone1 * in_bone_weight.y +
        bone2 * in_bone_weight.z +
        bone3 * in_bone_weight.w;

    vec3 pos = (vec4(in_position, 1.0) * bone).xyz;

    gl_Position = mvp * vec4(pos, 1.0);
}
