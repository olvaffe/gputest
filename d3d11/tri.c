/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "d3d11util.h"

static const uint8_t tri_test_vs[] = {
#include "tri_test.vs.h"
};

static const uint8_t tri_test_ps[] = {
#include "tri_test.ps.h"
};

static const struct {
    float pos[3];
    float color[3];
} tri_test_vertices[] = {
    [0] = {
        .pos = { 0.0f, 1.0f, 0.0f },
        .color = { 1.0f, 0.0f, 0.0f },
    },
    [1] = {
        .pos = { 1.0f, -1.0f, 0.0f },
        .color = { 0.0f, 1.0f, 0.0f },
    },
    [2] = {
        .pos = { -1.0f, -1.0f, 0.0f },
        .color = { 0.0f, 0.0f, 1.0f },
    },
};

struct tri_test {
    uint32_t width;
    uint32_t height;
    DXGI_FORMAT format;
    float clear_color[4];

    struct d3d11 d3d11;

    ID3D11Buffer *vb;
    ID3D11Texture2D *rt;
    ID3D11RenderTargetView *rtv;

    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11InputLayout *input_layout;
    ID3D11RasterizerState *rs;
    ID3D11DepthStencilState *ds;
};

static void
tri_test_init_pipeline(struct tri_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    d3d11->result = ID3D11Device_CreateVertexShader(d3d11->dev, tri_test_vs, sizeof(tri_test_vs),
                                                    NULL, &test->vs);
    d3d11_check(d3d11, "CreateVertexShader");

    d3d11->result = ID3D11Device_CreatePixelShader(d3d11->dev, tri_test_ps, sizeof(tri_test_ps),
                                                   NULL, &test->ps);
    d3d11_check(d3d11, "CreatePixelShader");

    const D3D11_INPUT_ELEMENT_DESC input_elements[] = {
        {
            .SemanticName = "POSITION",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = 0,
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        },
        {
            .SemanticName = "COLOR",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = 12,
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0,
        },
    };

    d3d11->result =
        ID3D11Device_CreateInputLayout(d3d11->dev, input_elements, ARRAY_SIZE(input_elements),
                                       tri_test_vs, sizeof(tri_test_vs), &test->input_layout);
    d3d11_check(d3d11, "CreateInputLayout");

    const D3D11_RASTERIZER_DESC rs_desc = {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .DepthClipEnable = TRUE,
    };
    d3d11->result = ID3D11Device_CreateRasterizerState(d3d11->dev, &rs_desc, &test->rs);
    d3d11_check(d3d11, "CreateRasterizerState");

    const D3D11_DEPTH_STENCIL_DESC ds_desc = {
        .DepthEnable = FALSE,
        .StencilEnable = FALSE,
    };
    d3d11->result = ID3D11Device_CreateDepthStencilState(d3d11->dev, &ds_desc, &test->ds);
    d3d11_check(d3d11, "CreateDepthStencilState");
}

static void
tri_test_init_rt(struct tri_test *test)
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
tri_test_init_vb(struct tri_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    const D3D11_BUFFER_DESC desc = {
        .ByteWidth = sizeof(tri_test_vertices),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
    };
    const D3D11_SUBRESOURCE_DATA sub_data = {
        .pSysMem = tri_test_vertices,
    };

    d3d11->result = ID3D11Device_CreateBuffer(d3d11->dev, &desc, &sub_data, &test->vb);
    d3d11_check(d3d11, "CreateBuffer (VB)");
}

static void
tri_test_init(struct tri_test *test)
{
    d3d11_init(&test->d3d11, NULL);

    tri_test_init_vb(test);
    tri_test_init_rt(test);
    tri_test_init_pipeline(test);
}

static void
tri_test_cleanup(struct tri_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    ID3D11DepthStencilState_Release(test->ds);
    ID3D11RasterizerState_Release(test->rs);
    ID3D11InputLayout_Release(test->input_layout);
    ID3D11PixelShader_Release(test->ps);
    ID3D11VertexShader_Release(test->vs);

    ID3D11RenderTargetView_Release(test->rtv);
    ID3D11Texture2D_Release(test->rt);

    ID3D11Buffer_Release(test->vb);

    d3d11_cleanup(d3d11);
}

static void
tri_test_draw(struct tri_test *test)
{
    struct d3d11 *d3d11 = &test->d3d11;

    const D3D11_VIEWPORT vp = {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = (float)test->width,
        .Height = (float)test->height,
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };

    ID3D11DeviceContext_ClearRenderTargetView(d3d11->ctx, test->rtv, test->clear_color);

    const UINT stride = sizeof(tri_test_vertices[0]);
    const UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(d3d11->ctx, 0, 1, &test->vb, &stride, &offset);
    ID3D11DeviceContext_IASetInputLayout(d3d11->ctx, test->input_layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(d3d11->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11DeviceContext_VSSetShader(d3d11->ctx, test->vs, NULL, 0);

    ID3D11DeviceContext_RSSetState(d3d11->ctx, test->rs);
    ID3D11DeviceContext_RSSetViewports(d3d11->ctx, 1, &vp);

    ID3D11DeviceContext_PSSetShader(d3d11->ctx, test->ps, NULL, 0);

    ID3D11DeviceContext_OMSetRenderTargets(d3d11->ctx, 1, &test->rtv, NULL);
    ID3D11DeviceContext_OMSetDepthStencilState(d3d11->ctx, test->ds, 0);

    ID3D11DeviceContext_Draw(d3d11->ctx, ARRAY_SIZE(tri_test_vertices), 0);

    d3d11_dump_image(d3d11, test->rt, "rt.ppm");
}

int
main(void)
{
    struct tri_test test = {
        .width = 300,
        .height = 300,
        .format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .clear_color = { 0.0f, 0.2f, 0.4f, 1.0f },
    };

    tri_test_init(&test);
    tri_test_draw(&test);
    tri_test_cleanup(&test);

    return 0;
}
