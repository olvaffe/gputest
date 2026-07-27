/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "d3d11util.h"

static const uint8_t tex_test_vs[] = {
#include "tex_test.vs.inc"
};

static const uint8_t tex_test_ps[] = {
#include "tex_test.ps.inc"
};

static const unsigned char tex_test_ppm[] = {
#include "tex_test.ppm.inc"
};

static const struct {
    float pos[2];
    float uv[2];
} tex_test_vertices[] = {
    [0] = {
        .pos = { -1.0f, 1.0f, },
        .uv = { 0.0f, 0.0f, },
    },
    [1] = {
        .pos = { 1.0f, 1.0f, },
        .uv = { 1.0f, 0.0f, },
    },
    [2] = {
        .pos = { -1.0f, -1.0f, },
        .uv = { 0.0f, 1.0f, },
    },
    [3] = {
        .pos = { 1.0f, -1.0f, },
        .uv = { 1.0f, 1.0f, },
    },
};

struct tex_test {
    uint32_t width;
    uint32_t height;
    DXGI_FORMAT format;

    struct d3d11 d3d11;

    ID3D11Buffer *vb;

    ID3D11Texture2D *tex;
    ID3D11ShaderResourceView *srv;
    ID3D11SamplerState *sampler;

    ID3D11Texture2D *rt;
    ID3D11RenderTargetView *rtv;

    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11InputLayout *input_layout;
};

static void
tex_test_init_pipeline(struct tex_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    d3d11->result = ID3D11Device_CreateVertexShader(d3d11->dev, tex_test_vs, sizeof(tex_test_vs),
                                                    NULL, &test->vs);
    d3d11_check(d3d11, "CreateVertexShader");

    d3d11->result = ID3D11Device_CreatePixelShader(d3d11->dev, tex_test_ps, sizeof(tex_test_ps),
                                                   NULL, &test->ps);
    d3d11_check(d3d11, "CreatePixelShader");

    const D3D11_INPUT_ELEMENT_DESC input_elements[] = {
        [0] = {
            .SemanticName = "POSITION",
            .Format = DXGI_FORMAT_R32G32_FLOAT,
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
        },
        [1] = {
            .SemanticName = "TEXCOORD",
            .Format = DXGI_FORMAT_R32G32_FLOAT,
            .AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT,
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
        },
    };

    d3d11->result =
        ID3D11Device_CreateInputLayout(d3d11->dev, input_elements, ARRAY_SIZE(input_elements),
                                       tex_test_vs, sizeof(tex_test_vs), &test->input_layout);
    d3d11_check(d3d11, "CreateInputLayout");
}

static void
tex_test_init_rt(struct tex_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    const D3D11_TEXTURE2D_DESC desc = {
        .Width = test->width,
        .Height = test->height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = test->format,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0,
        },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET,
    };
    d3d11->result = ID3D11Device_CreateTexture2D(d3d11->dev, &desc, NULL, &test->rt);
    d3d11_check(d3d11, "CreateTexture2D (RT)");

    d3d11->result = ID3D11Device_CreateRenderTargetView(d3d11->dev, (ID3D11Resource *)test->rt,
                                                        NULL, &test->rtv);
    d3d11_check(d3d11, "CreateRenderTargetView");
}

static void
tex_test_init_tex(struct tex_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    test->tex = d3d11_create_tex_from_ppm(d3d11, tex_test_ppm, sizeof(tex_test_ppm));

    d3d11->result = ID3D11Device_CreateShaderResourceView(d3d11->dev, (ID3D11Resource *)test->tex,
                                                          NULL, &test->srv);
    d3d11_check(d3d11, "CreateShaderResourceView");

    const D3D11_SAMPLER_DESC sampler_desc = {
        .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
        .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
        .MinLOD = 0.0f,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    d3d11->result = ID3D11Device_CreateSamplerState(d3d11->dev, &sampler_desc, &test->sampler);
    d3d11_check(d3d11, "CreateSamplerState");
}

static void
tex_test_init_vb(struct tex_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    const D3D11_BUFFER_DESC desc = {
        .ByteWidth = sizeof(tex_test_vertices),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
    };
    const D3D11_SUBRESOURCE_DATA sub_data = {
        .pSysMem = tex_test_vertices,
    };

    d3d11->result = ID3D11Device_CreateBuffer(d3d11->dev, &desc, &sub_data, &test->vb);
    d3d11_check(d3d11, "CreateBuffer (VB)");
}

static void
tex_test_init(struct tex_test *test)
{
    d3d11_init(&test->d3d11, NULL);

    tex_test_init_vb(test);
    tex_test_init_tex(test);
    tex_test_init_rt(test);
    tex_test_init_pipeline(test);
}

static void
tex_test_cleanup(struct tex_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    ID3D11InputLayout_Release(test->input_layout);
    ID3D11PixelShader_Release(test->ps);
    ID3D11VertexShader_Release(test->vs);

    ID3D11RenderTargetView_Release(test->rtv);
    ID3D11Texture2D_Release(test->rt);

    ID3D11SamplerState_Release(test->sampler);
    ID3D11ShaderResourceView_Release(test->srv);
    ID3D11Texture2D_Release(test->tex);

    ID3D11Buffer_Release(test->vb);

    d3d11_cleanup(d3d11);
}

static void
tex_test_draw(struct tex_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    const UINT stride = sizeof(tex_test_vertices[0]);
    const UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(d3d11->ctx, 0, 1, &test->vb, &stride, &offset);
    ID3D11DeviceContext_IASetInputLayout(d3d11->ctx, test->input_layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d11->ctx,
                                               D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    ID3D11DeviceContext_VSSetShader(d3d11->ctx, test->vs, NULL, 0);

    const D3D11_VIEWPORT vp = {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = (float)test->width,
        .Height = (float)test->height,
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
    ID3D11DeviceContext_RSSetViewports(d3d11->ctx, 1, &vp);

    ID3D11DeviceContext_PSSetShader(d3d11->ctx, test->ps, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(d3d11->ctx, 0, 1, &test->srv);
    ID3D11DeviceContext_PSSetSamplers(d3d11->ctx, 0, 1, &test->sampler);

    ID3D11DeviceContext_OMSetRenderTargets(d3d11->ctx, 1, &test->rtv, NULL);

    ID3D11DeviceContext_Draw(d3d11->ctx, ARRAY_SIZE(tex_test_vertices), 0);

    d3d11_dump_image(d3d11, test->tex, "tex.ppm");
    d3d11_dump_image(d3d11, test->rt, "rt.ppm");
}

int
main(void)
{
    struct tex_test test = {
        .width = 300,
        .height = 300,
        .format = DXGI_FORMAT_R8G8B8A8_UNORM,
    };

    tex_test_init(&test);
    tex_test_draw(&test);
    tex_test_cleanup(&test);

    return 0;
}
