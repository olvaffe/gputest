/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

/* XXX is this valid? */
layout(binding = 0) uniform sampler1DArray tex1d;
layout(binding = 0) uniform sampler2DArray tex2d;
layout(binding = 0) uniform samplerCubeArray texcube;
layout(binding = 0) uniform sampler3D tex3d;

layout(location = 0) in vec2 in_texcoord;
layout(location = 0) out vec4 out_color;

void main()
{
    //out_color = texture(tex1d, vec2(in_texcoord.x, 0.0));
    out_color = texture(tex2d, vec3(in_texcoord, 0.0));
    //out_color = texture(texcube, vec4(in_texcoord, 0.0, 0.0));
    //out_color = texture(tex3d, vec3(in_texcoord, 0.0));
}
