/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(location = 0) out uvec4 out_color;

void main()
{
    out_color = uvec4(0x80, 0x90, 0xa0, 0xb0);
}
