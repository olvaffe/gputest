/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "d3d11util.h"
#include "dxgiutil.h"

#include <wchar.h>

static void
info_factory(struct dxgi *dxgi)
{
    dxgi_log("IDXGIFactory%u:", dxgi->factory_version);

    if (dxgi->factory_version >= 5) {
        IDXGIFactory5 *factory5 = (IDXGIFactory5 *)dxgi->factory;

        BOOL allow_tearing = FALSE;
        dxgi->result = IDXGIFactory5_CheckFeatureSupport(
            factory5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
        if (SUCCEEDED(dxgi->result)) {
            dxgi_log("  PresentAllowTearing:   %d", allow_tearing);
        }
    }
}

static void
info_adapter(struct dxgi *dxgi)
{
    dxgi_log("IDXGIAdapter%u:", dxgi->adapter_version);

    DXGI_ADAPTER_DESC3 desc;
    dxgi->result = IDXGIAdapter4_GetDesc3(dxgi->adapter, &desc);
    dxgi_check(dxgi, "GetDesc3");

    char description[128] = { 0 };
    wcstombs(description, desc.Description, sizeof(description) - 1);

    dxgi_log("  Description:           %s", description);
    dxgi_log("  Vendor ID:             0x%04x", desc.VendorId);
    dxgi_log("  Device ID:             0x%04x", desc.DeviceId);
    dxgi_log("  SubSys ID:             0x%04x", desc.SubSysId);
    dxgi_log("  Revision:              0x%04x", desc.Revision);
    dxgi_log("  Dedicated Video Mem:   %zu MB", desc.DedicatedVideoMemory / (1024 * 1024));
    dxgi_log("  Dedicated System Mem:  %zu MB", desc.DedicatedSystemMemory / (1024 * 1024));
    dxgi_log("  Shared System Mem:     %zu MB", desc.SharedSystemMemory / (1024 * 1024));
    dxgi_log("  Adapter LUID:          %08x:%08x", (unsigned)desc.AdapterLuid.HighPart,
             (unsigned)desc.AdapterLuid.LowPart);
    dxgi_log("  Adapter Flags:         0x%x (%s%s%s)", desc.Flags,
             (desc.Flags & DXGI_ADAPTER_FLAG_REMOTE) ? "REMOTE " : "",
             (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? "SOFTWARE " : "",
             desc.Flags == DXGI_ADAPTER_FLAG3_NONE ? "NONE" : "");
    dxgi_log("  Graphics Preemption:   %u", desc.GraphicsPreemptionGranularity);
    dxgi_log("  Compute Preemption:    %u", desc.ComputePreemptionGranularity);

    DXGI_QUERY_VIDEO_MEMORY_INFO mem_info;
    if (SUCCEEDED(IDXGIAdapter4_QueryVideoMemoryInfo(
            dxgi->adapter, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &mem_info))) {
        dxgi_log("  Local Video Memory Info:");
        dxgi_log("    Budget:              %zu MB", mem_info.Budget / (1024 * 1024));
        dxgi_log("    Current Usage:       %zu MB", mem_info.CurrentUsage / (1024 * 1024));
        dxgi_log("    Available Reservation:%zu MB",
                 mem_info.AvailableForReservation / (1024 * 1024));
        dxgi_log("    Current Reservation: %zu MB", mem_info.CurrentReservation / (1024 * 1024));
    }
    if (SUCCEEDED(IDXGIAdapter4_QueryVideoMemoryInfo(
            dxgi->adapter, 0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &mem_info))) {
        dxgi_log("  Non-Local Video Memory Info:");
        dxgi_log("    Budget:              %zu MB", mem_info.Budget / (1024 * 1024));
        dxgi_log("    Current Usage:       %zu MB", mem_info.CurrentUsage / (1024 * 1024));
    }
}

static void
info_outputs(struct dxgi *dxgi)
{
    IDXGIOutput *output;
    for (UINT i = 0; SUCCEEDED(IDXGIAdapter4_EnumOutputs(dxgi->adapter, i, &output)); i++) {
        DXGI_OUTPUT_DESC desc;
        dxgi->result = IDXGIOutput_GetDesc(output, &desc);
        dxgi_check(dxgi, "IDXGIOutput_GetDesc");

        char device_name[128] = { 0 };
        wcstombs(device_name, desc.DeviceName, sizeof(device_name) - 1);

        dxgi_log("Output %u: %s", i, device_name);
        dxgi_log("  Attached to Desktop: %s", desc.AttachedToDesktop ? "yes" : "no");
        dxgi_log("  Rotation:            %u", desc.Rotation);
        dxgi_log("  Desktop Coordinates: [%d, %d, %d, %d] (%dx%d)", desc.DesktopCoordinates.left,
                 desc.DesktopCoordinates.top, desc.DesktopCoordinates.right,
                 desc.DesktopCoordinates.bottom,
                 desc.DesktopCoordinates.right - desc.DesktopCoordinates.left,
                 desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top);

        IDXGIOutput_Release(output);
    }
}

static void
info_device(struct d3d11 *d3d11)
{
    d3d11_log("ID3D11Device%d:", d3d11->dev_version);

    const D3D_FEATURE_LEVEL level = ID3D11Device5_GetFeatureLevel(d3d11->dev);
    const char *str;
    switch (level) {
    case D3D_FEATURE_LEVEL_9_1:
        str = "9_1";
        break;
    case D3D_FEATURE_LEVEL_9_2:
        str = "9_2";
        break;
    case D3D_FEATURE_LEVEL_9_3:
        str = "9_3";
        break;
    case D3D_FEATURE_LEVEL_10_0:
        str = "10_0";
        break;
    case D3D_FEATURE_LEVEL_10_1:
        str = "10_1";
        break;
    case D3D_FEATURE_LEVEL_11_0:
        str = "11_0";
        break;
    case D3D_FEATURE_LEVEL_11_1:
        str = "11_1";
        break;
    case D3D_FEATURE_LEVEL_12_0:
        str = "12_0";
        break;
    case D3D_FEATURE_LEVEL_12_1:
        str = "12_1";
        break;
    default:
        str = "unknown";
        break;
    }

    d3d11_log("  Feature Level: %s (0x%x)", str, (unsigned)level);
}

static void
info_context(struct d3d11 *d3d11)
{
    d3d11_log("ID3D11DeviceContext%d:", d3d11->ctx_version);
}

static void
info_feat_threading(struct d3d11 *d3d11)
{
    D3D11_FEATURE_DATA_THREADING data = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_THREADING, &data,
                                                      sizeof(data));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("Threading Support:");
        d3d11_log("  DriverConcurrentCreates: %d", data.DriverConcurrentCreates);
        d3d11_log("  DriverCommandLists:      %d", data.DriverCommandLists);
    }
}

static void
info_feat_doubles(struct d3d11 *d3d11)
{
    D3D11_FEATURE_DATA_DOUBLES data = { 0 };
    d3d11->result =
        ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_DOUBLES, &data, sizeof(data));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("Double Precision Ops: %d", data.DoublePrecisionFloatShaderOps);
    }
}

static void
info_feat_architecture(struct d3d11 *d3d11)
{
    D3D11_FEATURE_DATA_ARCHITECTURE_INFO data = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_ARCHITECTURE_INFO,
                                                      &data, sizeof(data));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("Architecture:");
        d3d11_log("  TileBasedDeferredRenderer: %d", data.TileBasedDeferredRenderer);
    }
}

static void
info_feat_shader_min_precision(struct d3d11 *d3d11)
{
    D3D11_FEATURE_DATA_SHADER_MIN_PRECISION_SUPPORT data = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(
        d3d11->dev, D3D11_FEATURE_SHADER_MIN_PRECISION_SUPPORT, &data, sizeof(data));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("Shader Min Precision:");
        d3d11_log("  PixelShaderMinPrecision:            0x%x", data.PixelShaderMinPrecision);
        d3d11_log("  AllOtherShaderStagesMinPrecision:   0x%x",
                  data.AllOtherShaderStagesMinPrecision);
    }
}

static void
info_feat_marker_support(struct d3d11 *d3d11)
{
    D3D11_FEATURE_DATA_MARKER_SUPPORT data = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_MARKER_SUPPORT,
                                                      &data, sizeof(data));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("Marker Support:        %d", data.Profile);
    }
}

static void
info_feat_gpu_virtual_address_support(struct d3d11 *d3d11)
{
    D3D11_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT data = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(
        d3d11->dev, D3D11_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &data, sizeof(data));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("GPU Virtual Address Support:");
        d3d11_log("  MaxGPUVirtualAddressBitsPerResource: %u",
                  data.MaxGPUVirtualAddressBitsPerResource);
        d3d11_log("  MaxGPUVirtualAddressBitsPerProcess:  %u",
                  data.MaxGPUVirtualAddressBitsPerProcess);
    }
}

static void
info_feat_shader_cache(struct d3d11 *d3d11)
{
    D3D11_FEATURE_DATA_SHADER_CACHE data = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_SHADER_CACHE,
                                                      &data, sizeof(data));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("Shader Cache Support Flags: 0x%x", data.SupportFlags);
    }
}

static void
info_feat_options(struct d3d11 *d3d11)
{
    D3D11_FEATURE_DATA_D3D11_OPTIONS opts = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_D3D11_OPTIONS,
                                                      &opts, sizeof(opts));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("D3D11 Options:");
        d3d11_log("  OutputMergerLogicOp:                   %d", opts.OutputMergerLogicOp);
        d3d11_log("  UAVOnlyRenderingForcedSampleCount:     %d",
                  opts.UAVOnlyRenderingForcedSampleCount);
        d3d11_log("  DiscardAPIsSeenByDriver:               %d", opts.DiscardAPIsSeenByDriver);
        d3d11_log("  FlagsForUpdateAndCopySeenByDriver:     %d",
                  opts.FlagsForUpdateAndCopySeenByDriver);
        d3d11_log("  ClearView:                             %d", opts.ClearView);
        d3d11_log("  CopyWithOverlap:                       %d", opts.CopyWithOverlap);
        d3d11_log("  ConstantBufferPartialUpdate:           %d",
                  opts.ConstantBufferPartialUpdate);
        d3d11_log("  ConstantBufferOffsetting:              %d", opts.ConstantBufferOffsetting);
        d3d11_log("  MapNoOverwriteOnDynamicConstantBuffer: %d",
                  opts.MapNoOverwriteOnDynamicConstantBuffer);
        d3d11_log("  MapNoOverwriteOnDynamicBufferSRV:      %d",
                  opts.MapNoOverwriteOnDynamicBufferSRV);
        d3d11_log("  MultisampleRTVWithForcedSampleCountOne:%d",
                  opts.MultisampleRTVWithForcedSampleCountOne);
        d3d11_log("  SAD4ShaderInstructions:                %d", opts.SAD4ShaderInstructions);
        d3d11_log("  ExtendedDoublesShaderInstructions:     %d",
                  opts.ExtendedDoublesShaderInstructions);
        d3d11_log("  ExtendedResourceSharing:               %d", opts.ExtendedResourceSharing);
    }

    D3D11_FEATURE_DATA_D3D11_OPTIONS1 opts1 = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_D3D11_OPTIONS1,
                                                      &opts1, sizeof(opts1));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("D3D11 Options1:");
        d3d11_log("  TiledResourcesTier:                    %d", opts1.TiledResourcesTier);
        d3d11_log("  MinMaxFiltering:                       %d", opts1.MinMaxFiltering);
        d3d11_log("  ClearViewAlsoSupportsDepthOnlyFormats: %d",
                  opts1.ClearViewAlsoSupportsDepthOnlyFormats);
        d3d11_log("  MapOnDefaultBuffers:                   %d", opts1.MapOnDefaultBuffers);
    }

    D3D11_FEATURE_DATA_D3D11_OPTIONS2 opts2 = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_D3D11_OPTIONS2,
                                                      &opts2, sizeof(opts2));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("D3D11 Options2:");
        d3d11_log("  PSSpecifiedStencilRefSupported:        %d",
                  opts2.PSSpecifiedStencilRefSupported);
        d3d11_log("  TypedUAVLoadAdditionalFormats:         %d",
                  opts2.TypedUAVLoadAdditionalFormats);
        d3d11_log("  ROVsSupported:                         %d", opts2.ROVsSupported);
        d3d11_log("  ConservativeRasterizationTier:         %d",
                  opts2.ConservativeRasterizationTier);
        d3d11_log("  TiledResourcesTier:                    %d", opts2.TiledResourcesTier);
        d3d11_log("  MapOnDefaultTextures:                  %d", opts2.MapOnDefaultTextures);
        d3d11_log("  StandardSwizzle:                       %d", opts2.StandardSwizzle);
        d3d11_log("  UnifiedMemoryArchitecture:             %d", opts2.UnifiedMemoryArchitecture);
    }

    D3D11_FEATURE_DATA_D3D11_OPTIONS3 opts3 = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_D3D11_OPTIONS3,
                                                      &opts3, sizeof(opts3));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("D3D11 Options3:");
        d3d11_log("  VPAndRTArrayIndexFromAnyShaderFeedingRasterizer: %d",
                  opts3.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer);
    }

    D3D11_FEATURE_DATA_D3D11_OPTIONS4 opts4 = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_D3D11_OPTIONS4,
                                                      &opts4, sizeof(opts4));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("D3D11 Options4:");
        d3d11_log("  ExtendedNV12SharedTextureSupported:    %d",
                  opts4.ExtendedNV12SharedTextureSupported);
    }

    D3D11_FEATURE_DATA_D3D11_OPTIONS5 opts5 = { 0 };
    d3d11->result = ID3D11Device5_CheckFeatureSupport(d3d11->dev, D3D11_FEATURE_D3D11_OPTIONS5,
                                                      &opts5, sizeof(opts5));
    if (SUCCEEDED(d3d11->result)) {
        d3d11_log("D3D11 Options5:");
        d3d11_log("  SharedResourceTier:                    %d", opts5.SharedResourceTier);
    }
}

int
main(void)
{
    struct dxgi dxgi;
    dxgi_init(&dxgi, NULL);

    info_factory(&dxgi);
    info_adapter(&dxgi);
    info_outputs(&dxgi);

    struct d3d11_init_params d3d11_params = {
        .adapter = (IDXGIAdapter *)dxgi.adapter,
    };
    struct d3d11 d3d11;
    d3d11_init(&d3d11, &d3d11_params);

    info_device(&d3d11);
    info_context(&d3d11);

    info_feat_threading(&d3d11);
    info_feat_doubles(&d3d11);
    info_feat_architecture(&d3d11);
    info_feat_shader_min_precision(&d3d11);
    info_feat_marker_support(&d3d11);
    info_feat_gpu_virtual_address_support(&d3d11);
    info_feat_shader_cache(&d3d11);

    info_feat_options(&d3d11);

    d3d11_cleanup(&d3d11);
    dxgi_cleanup(&dxgi);

    return 0;
}
