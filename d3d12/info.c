/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "d3d12util.h"

static void
info_general(struct d3d12 *d3d12)
{
    d3d12_log("ID3D12Device%u:", d3d12->dev_version);

    const UINT node_count = ID3D12Device_GetNodeCount(d3d12->dev);
    const LUID luid = ID3D12Device_GetAdapterLuid(d3d12->dev);

    d3d12_log("  Node Count: %u", node_count);
    d3d12_log("  Adapter LUID: %08x:%08x", (unsigned)luid.HighPart, (unsigned)luid.LowPart);

    void *ext = NULL;
    d3d12->result = ID3D12Device_QueryInterface(d3d12->dev, &IID_ID3D12DeviceExt, &ext);
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("  Implementation: VKD3D-Proton (ID3D12DeviceExt supported)");
        IUnknown_Release((IUnknown *)ext);
    } else {
        d3d12_log("  Implementation: Unknown / Native D3D12");
    }
}

static void
info_heap_properties(struct d3d12 *d3d12)
{
    static const struct {
        D3D12_HEAP_TYPE type;
        const char *name;
    } heap_types[] = {
        { D3D12_HEAP_TYPE_DEFAULT, "DEFAULT" },
        { D3D12_HEAP_TYPE_UPLOAD, "UPLOAD" },
        { D3D12_HEAP_TYPE_READBACK, "READBACK" },
        { D3D12_HEAP_TYPE_GPU_UPLOAD, "GPU_UPLOAD" },
    };

    d3d12_log("Custom Heap Properties:");
    for (size_t i = 0; i < ARRAY_SIZE(heap_types); i++) {
        const D3D12_HEAP_PROPERTIES props =
            ID3D12Device_GetCustomHeapProperties(d3d12->dev, 0, heap_types[i].type);

        const char *cpu_page;
        switch (props.CPUPageProperty) {
        case D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE:
            cpu_page = "NOT_AVAILABLE";
            break;
        case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE:
            cpu_page = "WRITE_COMBINE";
            break;
        case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK:
            cpu_page = "WRITE_BACK";
            break;
        default:
            cpu_page = "UNKNOWN";
            break;
        }

        const char *mem_pool;
        switch (props.MemoryPoolPreference) {
        case D3D12_MEMORY_POOL_L0:
            mem_pool = "L0";
            break;
        case D3D12_MEMORY_POOL_L1:
            mem_pool = "L1";
            break;
        default:
            mem_pool = "UNKNOWN";
            break;
        }

        d3d12_log("  %s: CPUPageProperty=%s, MemoryPoolPreference=%s", heap_types[i].name,
                  cpu_page, mem_pool);
    }
}

static void
info_descriptor_sizes(struct d3d12 *d3d12)
{
    const UINT cbv_srv_uav = ID3D12Device_GetDescriptorHandleIncrementSize(
        d3d12->dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const UINT sampler = ID3D12Device_GetDescriptorHandleIncrementSize(
        d3d12->dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    const UINT rtv =
        ID3D12Device_GetDescriptorHandleIncrementSize(d3d12->dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const UINT dsv =
        ID3D12Device_GetDescriptorHandleIncrementSize(d3d12->dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    d3d12_log("Descriptor Handle Increment Sizes:");
    d3d12_log("  CBV_SRV_UAV: %u bytes", cbv_srv_uav);
    d3d12_log("  SAMPLER:     %u bytes", sampler);
    d3d12_log("  RTV:         %u bytes", rtv);
    d3d12_log("  DSV:         %u bytes", dsv);
}

static void
info_feat_architecture(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_ARCHITECTURE1 arch = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_ARCHITECTURE1,
                                                     &arch, sizeof(arch));
    if (FAILED(d3d12->result)) {
        D3D12_FEATURE_DATA_ARCHITECTURE arch0 = { 0 };
        d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_ARCHITECTURE,
                                                         &arch0, sizeof(arch0));
        if (SUCCEEDED(d3d12->result)) {
            arch.TileBasedRenderer = arch0.TileBasedRenderer;
            arch.UMA = arch0.UMA;
            arch.CacheCoherentUMA = arch0.CacheCoherentUMA;
        }
    }

    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Architecture:");
        d3d12_log("  TileBasedRenderer:             %d", arch.TileBasedRenderer);
        d3d12_log("  UMA:                           %d", arch.UMA);
        d3d12_log("  CacheCoherentUMA:              %d", arch.CacheCoherentUMA);
        d3d12_log("  IsolatedMMU:                   %d", arch.IsolatedMMU);
    }
}

static void
info_feat_feature_levels(struct d3d12 *d3d12)
{
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
    };
    D3D12_FEATURE_DATA_FEATURE_LEVELS data = {
        .NumFeatureLevels = ARRAY_SIZE(levels),
        .pFeatureLevelsRequested = levels,
    };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_FEATURE_LEVELS,
                                                     &data, sizeof(data));
    d3d12_check(d3d12, "CheckFeatureSupport FEATURE_LEVELS");

    const char *str;
    switch (data.MaxSupportedFeatureLevel) {
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
    case D3D_FEATURE_LEVEL_12_2:
        str = "12_2";
        break;
    default:
        str = "unknown";
        break;
    }

    d3d12_log("Max Feature Level: %s (0x%x)", str, data.MaxSupportedFeatureLevel);
}

static void
info_feat_gpu_virtual_address_support(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT data = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &data, sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("GPU Virtual Address Support:");
        d3d12_log("  MaxGPUVirtualAddressBitsPerResource: %u",
                  data.MaxGPUVirtualAddressBitsPerResource);
        d3d12_log("  MaxGPUVirtualAddressBitsPerProcess:  %u",
                  data.MaxGPUVirtualAddressBitsPerProcess);
    }
}

static void
info_feat_shader_model(struct d3d12 *d3d12)
{
    const D3D_SHADER_MODEL models[] = {
        D3D_SHADER_MODEL_6_7, D3D_SHADER_MODEL_6_6, D3D_SHADER_MODEL_6_5,
        D3D_SHADER_MODEL_6_4, D3D_SHADER_MODEL_6_3, D3D_SHADER_MODEL_6_2,
        D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0, D3D_SHADER_MODEL_5_1,
    };
    for (size_t i = 0; i < ARRAY_SIZE(models); i++) {
        D3D12_FEATURE_DATA_SHADER_MODEL data = { .HighestShaderModel = models[i] };
        d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_SHADER_MODEL,
                                                         &data, sizeof(data));
        if (SUCCEEDED(d3d12->result)) {
            d3d12_log("Highest Shader Model: 0x%x", data.HighestShaderModel);
            break;
        }
    }
}

static void
info_feat_root_signature(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE data = { .HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_ROOT_SIGNATURE,
                                                     &data, sizeof(data));
    if (FAILED(d3d12->result)) {
        data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    const char *str = data.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_1 ? "1.1" : "1.0";
    d3d12_log("Highest Root Signature Version: %s", str);
}

static void
info_feat_shader_cache(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_SHADER_CACHE data = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_SHADER_CACHE,
                                                     &data, sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Shader Cache Support Flags: 0x%x", data.SupportFlags);
    }
}

static void
info_feat_existing_heaps(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_EXISTING_HEAPS data = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_EXISTING_HEAPS,
                                                     &data, sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Existing Heaps Supported: %d", data.Supported);
    }
}

static void
info_feat_serialization(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_SERIALIZATION data = { .NodeIndex = 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_SERIALIZATION,
                                                     &data, sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Heap Serialization Tier: %d", data.HeapSerializationTier);
    }
}

static void
info_feat_cross_node(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_CROSS_NODE data = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_CROSS_NODE, &data,
                                                     sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Cross Node:");
        d3d12_log("  SharingTier:               %d", data.SharingTier);
        d3d12_log("  AtomicShaderInstructions:  %d", data.AtomicShaderInstructions);
    }
}

static void
info_feat_predication(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_PREDICATION data = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_PREDICATION, &data,
                                                     sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Predication Supported: %d", data.Supported);
    }
}

static void
info_feat_hardware_copy(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_HARDWARE_COPY data = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_HARDWARE_COPY,
                                                     &data, sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Hardware Copy Supported: %d", data.Supported);
    }
}

static void
info_feat_tight_alignment(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_TIGHT_ALIGNMENT data = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_TIGHT_ALIGNMENT, &data, sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Tight Alignment Tier: %d", data.SupportTier);
    }
}

static void
info_feat_application_specific_driver_state(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_APPLICATION_SPECIFIC_DRIVER_STATE data = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_APPLICATION_SPECIFIC_DRIVER_STATE, &data, sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Application Specific Driver State Supported: %d", data.Supported);
    }
}

static void
info_feat_bytecode_bypass_hash(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_BYTECODE_BYPASS_HASH_SUPPORTED data = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_BYTECODE_BYPASS_HASH_SUPPORTED, &data, sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Bytecode Bypass Hash Supported: %d", data.Supported);
    }
}

static void
info_feat_protected_resource_session_support(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_PROTECTED_RESOURCE_SESSION_SUPPORT data = { .NodeIndex = 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_PROTECTED_RESOURCE_SESSION_SUPPORT, &data, sizeof(data));
    if (SUCCEEDED(d3d12->result)) {
        d3d12_log("Protected Resource Session Support: 0x%x", data.Support);
    }
}

static void
info_feat_options(struct d3d12 *d3d12)
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS opts = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS,
                                                     &opts, sizeof(opts));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS");

    D3D12_FEATURE_DATA_D3D12_OPTIONS1 opts1 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS1,
                                                     &opts1, sizeof(opts1));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS1");

    D3D12_FEATURE_DATA_D3D12_OPTIONS2 opts2 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS2,
                                                     &opts2, sizeof(opts2));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS2");

    D3D12_FEATURE_DATA_D3D12_OPTIONS3 opts3 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS3,
                                                     &opts3, sizeof(opts3));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS3");

    D3D12_FEATURE_DATA_D3D12_OPTIONS4 opts4 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS4,
                                                     &opts4, sizeof(opts4));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS4");

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS5,
                                                     &opts5, sizeof(opts5));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS5");

    D3D12_FEATURE_DATA_D3D12_OPTIONS6 opts6 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS6,
                                                     &opts6, sizeof(opts6));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS6");

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 opts7 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS7,
                                                     &opts7, sizeof(opts7));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS7");

    D3D12_FEATURE_DATA_D3D12_OPTIONS8 opts8 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS8,
                                                     &opts8, sizeof(opts8));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS8");

    D3D12_FEATURE_DATA_D3D12_OPTIONS9 opts9 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS9,
                                                     &opts9, sizeof(opts9));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS9");

    D3D12_FEATURE_DATA_D3D12_OPTIONS10 opts10 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS10,
                                                     &opts10, sizeof(opts10));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS10");

    D3D12_FEATURE_DATA_D3D12_OPTIONS11 opts11 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS11,
                                                     &opts11, sizeof(opts11));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS11");

    D3D12_FEATURE_DATA_D3D12_OPTIONS12 opts12 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS12,
                                                     &opts12, sizeof(opts12));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS12");

    D3D12_FEATURE_DATA_D3D12_OPTIONS13 opts13 = { 0 };
    d3d12->result = ID3D12Device_CheckFeatureSupport(d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS13,
                                                     &opts13, sizeof(opts13));
    d3d12_check(d3d12, "CheckFeatureSupport D3D12_OPTIONS13");

    D3D12_FEATURE_DATA_D3D12_OPTIONS14 opts14 = { 0 };
    const bool has_opts14 = SUCCEEDED(ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS14, &opts14, sizeof(opts14)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS15 opts15 = { 0 };
    const bool has_opts15 = SUCCEEDED(ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS15, &opts15, sizeof(opts15)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS16 opts16 = { 0 };
    const bool has_opts16 = SUCCEEDED(ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS16, &opts16, sizeof(opts16)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS17 opts17 = { 0 };
    const bool has_opts17 = SUCCEEDED(ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS17, &opts17, sizeof(opts17)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS18 opts18 = { 0 };
    const bool has_opts18 = SUCCEEDED(ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS18, &opts18, sizeof(opts18)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS19 opts19 = { 0 };
    const bool has_opts19 = SUCCEEDED(ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS19, &opts19, sizeof(opts19)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS20 opts20 = { 0 };
    const bool has_opts20 = SUCCEEDED(ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS20, &opts20, sizeof(opts20)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS21 opts21 = { 0 };
    const bool has_opts21 = SUCCEEDED(ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS21, &opts21, sizeof(opts21)));

    D3D12_FEATURE_DATA_D3D12_OPTIONS22 opts22 = { 0 };
    const bool has_opts22 = SUCCEEDED(ID3D12Device_CheckFeatureSupport(
        d3d12->dev, D3D12_FEATURE_D3D12_OPTIONS22, &opts22, sizeof(opts22)));

    d3d12_log("D3D12 Options:");
    d3d12_log("  DoublePrecisionFloatShaderOps: %d", opts.DoublePrecisionFloatShaderOps);
    d3d12_log("  OutputMergerLogicOp:           %d", opts.OutputMergerLogicOp);
    d3d12_log("  MinPrecisionSupport:           0x%x", opts.MinPrecisionSupport);
    d3d12_log("  TiledResourcesTier:            %d", opts.TiledResourcesTier);
    d3d12_log("  ResourceBindingTier:           %d", opts.ResourceBindingTier);
    d3d12_log("  PSSpecifiedStencilRefSupported: %d", opts.PSSpecifiedStencilRefSupported);
    d3d12_log("  TypedUAVLoadAdditionalFormats: %d", opts.TypedUAVLoadAdditionalFormats);
    d3d12_log("  ROVsSupported:                 %d", opts.ROVsSupported);
    d3d12_log("  ConservativeRasterizationTier: %d", opts.ConservativeRasterizationTier);
    d3d12_log("  MaxGPUVirtualAddressBits:      %u", opts.MaxGPUVirtualAddressBitsPerResource);
    d3d12_log("  StandardSwizzle64KBSupported:  %d", opts.StandardSwizzle64KBSupported);
    d3d12_log("  CrossNodeSharingTier:          %d", opts.CrossNodeSharingTier);
    d3d12_log("  ResourceHeapTier:              %d", opts.ResourceHeapTier);

    d3d12_log("D3D12 Options1:");
    d3d12_log("  WaveOps:                       %d", opts1.WaveOps);
    d3d12_log("  WaveLaneCountMin:              %u", opts1.WaveLaneCountMin);
    d3d12_log("  WaveLaneCountMax:              %u", opts1.WaveLaneCountMax);
    d3d12_log("  TotalLaneCount:                %u", opts1.TotalLaneCount);
    d3d12_log("  ExpandedComputeResourceStates: %d", opts1.ExpandedComputeResourceStates);
    d3d12_log("  Int64ShaderOps:                %d", opts1.Int64ShaderOps);

    d3d12_log("D3D12 Options2:");
    d3d12_log("  DepthBoundsTestSupported:      %d", opts2.DepthBoundsTestSupported);
    d3d12_log("  ProgrammableSamplePositionsTier: %d", opts2.ProgrammableSamplePositionsTier);

    d3d12_log("D3D12 Options3:");
    d3d12_log("  CopyQueueTimestampQueriesSupported: %d",
              opts3.CopyQueueTimestampQueriesSupported);
    d3d12_log("  CastingFullyTypedFormatSupported: %d", opts3.CastingFullyTypedFormatSupported);
    d3d12_log("  WriteBufferImmediateSupportFlags: 0x%x", opts3.WriteBufferImmediateSupportFlags);
    d3d12_log("  ViewInstancingTier:            %d", opts3.ViewInstancingTier);
    d3d12_log("  BarycentricsSupported:         %d", opts3.BarycentricsSupported);

    d3d12_log("D3D12 Options4:");
    d3d12_log("  MSAA64KBAlignedTextureSupported: %d", opts4.MSAA64KBAlignedTextureSupported);
    d3d12_log("  SharedResourceCompatibilityTier: %d", opts4.SharedResourceCompatibilityTier);
    d3d12_log("  Native16BitShaderOpsSupported: %d", opts4.Native16BitShaderOpsSupported);

    d3d12_log("D3D12 Options5:");
    d3d12_log("  SRVOnlyTiledResourceTier3:     %d", opts5.SRVOnlyTiledResourceTier3);
    d3d12_log("  RenderPassesTier:              %d", opts5.RenderPassesTier);
    d3d12_log("  RaytracingTier:                %d", opts5.RaytracingTier);

    d3d12_log("D3D12 Options6:");
    d3d12_log("  AdditionalShadingRatesSupported: %d", opts6.AdditionalShadingRatesSupported);
    d3d12_log("  PerPrimitiveShadingRateSupportedWithViewportIndexing: %d",
              opts6.PerPrimitiveShadingRateSupportedWithViewportIndexing);
    d3d12_log("  VariableShadingRateTier:       %d", opts6.VariableShadingRateTier);
    d3d12_log("  ShadingRateImageTileSize:      %u", opts6.ShadingRateImageTileSize);
    d3d12_log("  BackgroundProcessingSupported: %d", opts6.BackgroundProcessingSupported);

    d3d12_log("D3D12 Options7:");
    d3d12_log("  MeshShaderTier:                %d", opts7.MeshShaderTier);
    d3d12_log("  SamplerFeedbackTier:           %d", opts7.SamplerFeedbackTier);

    d3d12_log("D3D12 Options8:");
    d3d12_log("  UnalignedBlockTexturesSupported: %d", opts8.UnalignedBlockTexturesSupported);

    d3d12_log("D3D12 Options9:");
    d3d12_log("  MeshShaderPipelineStatsSupported: %d", opts9.MeshShaderPipelineStatsSupported);
    d3d12_log("  MeshShaderSupportsFullRangeRenderTargetArrayIndex: %d",
              opts9.MeshShaderSupportsFullRangeRenderTargetArrayIndex);
    d3d12_log("  AtomicInt64OnTypedResourceSupported: %d",
              opts9.AtomicInt64OnTypedResourceSupported);
    d3d12_log("  AtomicInt64OnGroupSharedSupported: %d", opts9.AtomicInt64OnGroupSharedSupported);
    d3d12_log("  DerivativesInMeshAndAmplificationShadersSupported: %d",
              opts9.DerivativesInMeshAndAmplificationShadersSupported);
    d3d12_log("  WaveMMATier:                   %d", opts9.WaveMMATier);

    d3d12_log("D3D12 Options10:");
    d3d12_log("  VariableRateShadingSumCombinerSupported: %d",
              opts10.VariableRateShadingSumCombinerSupported);
    d3d12_log("  MeshShaderPerPrimitiveShadingRateSupported: %d",
              opts10.MeshShaderPerPrimitiveShadingRateSupported);

    d3d12_log("D3D12 Options11:");
    d3d12_log("  AtomicInt64OnDescriptorHeapResourceSupported: %d",
              opts11.AtomicInt64OnDescriptorHeapResourceSupported);

    d3d12_log("D3D12 Options12:");
    d3d12_log("  MSPrimitivesPipelineStatisticIncludesCulledPrimitives: %d",
              opts12.MSPrimitivesPipelineStatisticIncludesCulledPrimitives);
    d3d12_log("  EnhancedBarriersSupported:     %d", opts12.EnhancedBarriersSupported);
    d3d12_log("  RelaxedFormatCastingSupported: %d", opts12.RelaxedFormatCastingSupported);

    d3d12_log("D3D12 Options13:");
    d3d12_log("  UnrestrictedBufferTextureCopyPitchSupported: %d",
              opts13.UnrestrictedBufferTextureCopyPitchSupported);
    d3d12_log("  UnrestrictedVertexElementAlignmentSupported: %d",
              opts13.UnrestrictedVertexElementAlignmentSupported);
    d3d12_log("  InvertedViewportHeightFlipsYSupported: %d",
              opts13.InvertedViewportHeightFlipsYSupported);
    d3d12_log("  InvertedViewportDepthFlipsZSupported: %d",
              opts13.InvertedViewportDepthFlipsZSupported);
    d3d12_log("  TextureCopyBetweenDimensionsSupported: %d",
              opts13.TextureCopyBetweenDimensionsSupported);
    d3d12_log("  AlphaBlendFactorSupported:     %d", opts13.AlphaBlendFactorSupported);

    if (has_opts14) {
        d3d12_log("D3D12 Options14:");
        d3d12_log("  AdvancedTextureOpsSupported:   %d", opts14.AdvancedTextureOpsSupported);
        d3d12_log("  WriteableMSAATexturesSupported: %d", opts14.WriteableMSAATexturesSupported);
        d3d12_log("  IndependentFrontAndBackStencilRefMaskSupported: %d",
                  opts14.IndependentFrontAndBackStencilRefMaskSupported);
    }

    if (has_opts15) {
        d3d12_log("D3D12 Options15:");
        d3d12_log("  TriangleFanSupported:          %d", opts15.TriangleFanSupported);
        d3d12_log("  DynamicIndexBufferStripCutSupported: %d",
                  opts15.DynamicIndexBufferStripCutSupported);
    }

    if (has_opts16) {
        d3d12_log("D3D12 Options16:");
        d3d12_log("  DynamicDepthBiasSupported:     %d", opts16.DynamicDepthBiasSupported);
        d3d12_log("  GPUUploadHeapSupported:        %d", opts16.GPUUploadHeapSupported);
    }

    if (has_opts17) {
        d3d12_log("D3D12 Options17:");
        d3d12_log("  NonNormalizedCoordinateSamplersSupported: %d",
                  opts17.NonNormalizedCoordinateSamplersSupported);
        d3d12_log("  ManualWriteTrackingResourceSupported: %d",
                  opts17.ManualWriteTrackingResourceSupported);
    }

    if (has_opts18) {
        d3d12_log("D3D12 Options18:");
        d3d12_log("  RenderPassesValid:             %d", opts18.RenderPassesValid);
    }

    if (has_opts19) {
        d3d12_log("D3D12 Options19:");
        d3d12_log("  MismatchingOutputDimensionsSupported: %d",
                  opts19.MismatchingOutputDimensionsSupported);
        d3d12_log("  SupportedSampleCountsWithNoOutputs:   %u",
                  opts19.SupportedSampleCountsWithNoOutputs);
        d3d12_log("  PointSamplingAddressesNeverRoundUp:   %d",
                  opts19.PointSamplingAddressesNeverRoundUp);
        d3d12_log("  RasterizerDesc2Supported:             %d", opts19.RasterizerDesc2Supported);
        d3d12_log("  NarrowQuadrilateralLinesSupported:    %d",
                  opts19.NarrowQuadrilateralLinesSupported);
        d3d12_log("  AnisoFilterWithPointMipSupported:     %d",
                  opts19.AnisoFilterWithPointMipSupported);
        d3d12_log("  MaxSamplerDescriptorHeapSize:         %u",
                  opts19.MaxSamplerDescriptorHeapSize);
        d3d12_log("  MaxSamplerDescriptorHeapSizeWithStaticSamplers: %u",
                  opts19.MaxSamplerDescriptorHeapSizeWithStaticSamplers);
        d3d12_log("  MaxViewDescriptorHeapSize:           %u", opts19.MaxViewDescriptorHeapSize);
        d3d12_log("  ComputeOnlyCustomHeapSupported:       %d",
                  opts19.ComputeOnlyCustomHeapSupported);
    }

    if (has_opts20) {
        d3d12_log("D3D12 Options20:");
        d3d12_log("  ComputeOnlyWriteWatchSupported: %d", opts20.ComputeOnlyWriteWatchSupported);
        d3d12_log("  RecreateAtTier:                 %d", opts20.RecreateAtTier);
    }

    if (has_opts21) {
        d3d12_log("D3D12 Options21:");
        d3d12_log("  WorkGraphsTier:                %d", opts21.WorkGraphsTier);
        d3d12_log("  ExecuteIndirectTier:           %d", opts21.ExecuteIndirectTier);
        d3d12_log("  SampleCmpGradientAndBiasSupported: %d",
                  opts21.SampleCmpGradientAndBiasSupported);
        d3d12_log("  ExtendedCommandInfoSupported:  %d", opts21.ExtendedCommandInfoSupported);
    }

    if (has_opts22) {
        d3d12_log("D3D12 Options22:");
        d3d12_log("  ShaderExecutionReorderingActuallyReorders: %d",
                  opts22.ShaderExecutionReorderingActuallyReorders);
        d3d12_log("  CreateByteOffsetViewsSupported:            %d",
                  opts22.CreateByteOffsetViewsSupported);
        d3d12_log("  Max1DDispatchSize:                         %u", opts22.Max1DDispatchSize);
        d3d12_log("  Max1DDispatchMeshSize:                     %u",
                  opts22.Max1DDispatchMeshSize);
    }
}

int
main(void)
{
    struct d3d12 d3d12;

    d3d12_init(&d3d12, NULL);

    info_general(&d3d12);

    info_heap_properties(&d3d12);
    info_descriptor_sizes(&d3d12);

    info_feat_architecture(&d3d12);
    info_feat_feature_levels(&d3d12);
    info_feat_gpu_virtual_address_support(&d3d12);
    info_feat_shader_model(&d3d12);
    info_feat_root_signature(&d3d12);
    info_feat_shader_cache(&d3d12);
    info_feat_existing_heaps(&d3d12);
    info_feat_serialization(&d3d12);
    info_feat_cross_node(&d3d12);
    info_feat_predication(&d3d12);
    info_feat_hardware_copy(&d3d12);
    info_feat_tight_alignment(&d3d12);
    info_feat_application_specific_driver_state(&d3d12);
    info_feat_bytecode_bypass_hash(&d3d12);
    info_feat_protected_resource_session_support(&d3d12);

    info_feat_options(&d3d12);

    d3d12_cleanup(&d3d12);

    return 0;
}
