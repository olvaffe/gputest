/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(binding = 0) uniform sampler1D tex1d;
layout(binding = 0) uniform sampler2D tex2d;
layout(binding = 0) uniform sampler3D tex3d;
layout(binding = 0) uniform samplerCube texcube;
layout(binding = 0) uniform sampler1DArray tex1da;
layout(binding = 0) uniform sampler2DArray tex2da;
layout(binding = 0) uniform samplerCubeArray texcubea;

layout(location = 0) in vec2 in_texcoord;
layout(location = 0) out vec4 out_color;

const uint VK_IMAGE_VIEW_TYPE_1D = 0;
const uint VK_IMAGE_VIEW_TYPE_2D = 1;
const uint VK_IMAGE_VIEW_TYPE_3D = 2;
const uint VK_IMAGE_VIEW_TYPE_CUBE = 3;
const uint VK_IMAGE_VIEW_TYPE_1D_ARRAY = 4;
const uint VK_IMAGE_VIEW_TYPE_2D_ARRAY = 5;
const uint VK_IMAGE_VIEW_TYPE_CUBE_ARRAY = 6;
layout(push_constant) uniform PUSH {
    uint view_type;
    float slice;
} push;

void main()
{
    if (push.view_type == VK_IMAGE_VIEW_TYPE_1D)
        out_color = texture(tex1d, in_texcoord.x);
    else if (push.view_type == VK_IMAGE_VIEW_TYPE_2D)
        out_color = texture(tex2d, in_texcoord);
    else if (push.view_type == VK_IMAGE_VIEW_TYPE_3D)
        out_color = texture(tex3d, vec3(in_texcoord, push.slice));
    else if (push.view_type == VK_IMAGE_VIEW_TYPE_CUBE)
        out_color = texture(texcube, vec3(in_texcoord, 1.0));
    else if (push.view_type == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
        out_color = texture(tex1da, vec2(in_texcoord.x, push.slice));
    else if (push.view_type == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
        out_color = texture(tex2da, vec3(in_texcoord, push.slice));
    else
        out_color = texture(texcubea, vec4(in_texcoord, 1.0, push.slice));
}
