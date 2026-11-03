/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "mtlutil.h"

static void
info_general(struct mtl *mtl)
{
    const NS::SharedPtr<MTL::Device> &dev = mtl->dev;

    NS::SharedPtr<NS::Array> all_devs = NS::TransferPtr(MTL::CopyAllDevices());
    const NS::UInteger dev_count = all_devs ? all_devs->count() : 0;

    const char *loc_str;
    switch (dev->location()) {
    case MTL::DeviceLocationBuiltIn:
        loc_str = "BuiltIn";
        break;
    case MTL::DeviceLocationSlot:
        loc_str = "Slot";
        break;
    case MTL::DeviceLocationExternal:
        loc_str = "External";
        break;
    case MTL::DeviceLocationUnspecified:
        loc_str = "Unspecified";
        break;
    default:
        loc_str = "Unknown";
        break;
    }

    MTL::Architecture *arch = dev->architecture();
    const char *arch_name = (arch && arch->name()) ? arch->name()->utf8String() : "Unknown";

    mtl_log("MTL::Device (1 of %lu):", (unsigned long)dev_count);
    mtl_log("  Name:                            %s", dev->name()->utf8String());
    mtl_log("  Registry ID:                     0x%" PRIx64, dev->registryID());
    mtl_log("  Architecture:                    %s", arch_name);
    mtl_log("  Location:                        %s (%lu)", loc_str,
            (unsigned long)dev->location());
    mtl_log("  Location Number:                 %lu", (unsigned long)dev->locationNumber());
    mtl_log("  Low Power:                       %d", dev->isLowPower());
    mtl_log("  Headless:                        %d", dev->isHeadless());
    mtl_log("  Removable:                       %d", dev->isRemovable());
    mtl_log("  Unified Memory:                  %d", dev->hasUnifiedMemory());
    mtl_log("  Peer Group ID:                   0x%" PRIx64, dev->peerGroupID());
    mtl_log("  Peer Index:                      %u", dev->peerIndex());
    mtl_log("  Peer Count:                      %u", dev->peerCount());
}

static void
info_memory_and_limits(struct mtl *mtl)
{
    const NS::SharedPtr<MTL::Device> &dev = mtl->dev;
    const MTL::Size max_threads = dev->maxThreadsPerThreadgroup();

    mtl_log("Memory and Limits:");
    mtl_log("  Recommended Max Working Set:     %" PRIu64 " MB (%" PRIu64 " bytes)",
            dev->recommendedMaxWorkingSetSize() / (1024 * 1024),
            dev->recommendedMaxWorkingSetSize());
    mtl_log("  Current Allocated Size:          %lu bytes",
            (unsigned long)dev->currentAllocatedSize());
    mtl_log("  Max Transfer Rate:               %" PRIu64 " bytes/s", dev->maxTransferRate());
    mtl_log("  Max Buffer Length:               %lu MB (%lu bytes)",
            (unsigned long)(dev->maxBufferLength() / (1024 * 1024)),
            (unsigned long)dev->maxBufferLength());
    mtl_log("  Max Threadgroup Memory Length:   %lu bytes",
            (unsigned long)dev->maxThreadgroupMemoryLength());
    mtl_log("  Max Threads Per Threadgroup:     %lu x %lu x %lu",
            (unsigned long)max_threads.width, (unsigned long)max_threads.height,
            (unsigned long)max_threads.depth);
    mtl_log("  Max Argument Buffer Samplers:    %lu",
            (unsigned long)dev->maxArgumentBufferSamplerCount());
    mtl_log("  Maximize Concurrent Compilation: %d", dev->shouldMaximizeConcurrentCompilation());
    mtl_log("  Max Concurrent Compilation Tasks:%lu",
            (unsigned long)dev->maximumConcurrentCompilationTaskCount());
}

static void
info_gpu_families(struct mtl *mtl)
{
    const NS::SharedPtr<MTL::Device> &dev = mtl->dev;

    static const struct {
        MTL::GPUFamily family;
        const char *name;
    } families[] = {
        { MTL::GPUFamilyApple1, "Apple1" },
        { MTL::GPUFamilyApple2, "Apple2" },
        { MTL::GPUFamilyApple3, "Apple3" },
        { MTL::GPUFamilyApple4, "Apple4" },
        { MTL::GPUFamilyApple5, "Apple5" },
        { MTL::GPUFamilyApple6, "Apple6" },
        { MTL::GPUFamilyApple7, "Apple7" },
        { MTL::GPUFamilyApple8, "Apple8" },
        { MTL::GPUFamilyApple9, "Apple9" },
        { MTL::GPUFamilyApple10, "Apple10" },
        { MTL::GPUFamilyApple11, "Apple11" },
        { MTL::GPUFamilyMac1, "Mac1" },
        { MTL::GPUFamilyMac2, "Mac2" },
        { MTL::GPUFamilyCommon1, "Common1" },
        { MTL::GPUFamilyCommon2, "Common2" },
        { MTL::GPUFamilyCommon3, "Common3" },
        { MTL::GPUFamilyMacCatalyst1, "MacCatalyst1" },
        { MTL::GPUFamilyMacCatalyst2, "MacCatalyst2" },
        { MTL::GPUFamilyMetal3, "Metal3" },
        { MTL::GPUFamilyMetal4, "Metal4" },
    };

    mtl_log("GPU Families:");
    for (size_t i = 0; i < ARRAY_SIZE(families); i++) {
        mtl_log("  %-32s %d", families[i].name, dev->supportsFamily(families[i].family));
    }
}

static void
info_feature_sets(struct mtl *mtl)
{
    const NS::SharedPtr<MTL::Device> &dev = mtl->dev;

    static const struct {
        MTL::FeatureSet set;
        const char *name;
    } sets[] = {
        { MTL::FeatureSet_macOS_GPUFamily1_v1, "macOS_GPUFamily1_v1" },
        { MTL::FeatureSet_macOS_GPUFamily1_v2, "macOS_GPUFamily1_v2" },
        { MTL::FeatureSet_macOS_ReadWriteTextureTier2, "macOS_ReadWriteTextureTier2" },
        { MTL::FeatureSet_macOS_GPUFamily1_v3, "macOS_GPUFamily1_v3" },
        { MTL::FeatureSet_macOS_GPUFamily1_v4, "macOS_GPUFamily1_v4" },
        { MTL::FeatureSet_macOS_GPUFamily2_v1, "macOS_GPUFamily2_v1" },
    };

    mtl_log("Feature Sets (Legacy):");
    for (size_t i = 0; i < ARRAY_SIZE(sets); i++) {
        mtl_log("  %-32s %d", sets[i].name, dev->supportsFeatureSet(sets[i].set));
    }
}

static void
info_tiers_and_features(struct mtl *mtl)
{
    const NS::SharedPtr<MTL::Device> &dev = mtl->dev;

    const char *arg_buf_str;
    switch (dev->argumentBuffersSupport()) {
    case MTL::ArgumentBuffersTier1:
        arg_buf_str = "Tier1";
        break;
    case MTL::ArgumentBuffersTier2:
        arg_buf_str = "Tier2";
        break;
    default:
        arg_buf_str = "Unknown";
        break;
    }

    const char *rw_tex_str;
    switch (dev->readWriteTextureSupport()) {
    case MTL::ReadWriteTextureTierNone:
        rw_tex_str = "None";
        break;
    case MTL::ReadWriteTextureTier1:
        rw_tex_str = "Tier1";
        break;
    case MTL::ReadWriteTextureTier2:
        rw_tex_str = "Tier2";
        break;
    default:
        rw_tex_str = "Unknown";
        break;
    }

    mtl_log("Tiers and Capabilities:");
    mtl_log("  ArgumentBuffersSupport:          %s (%lu)", arg_buf_str,
            (unsigned long)dev->argumentBuffersSupport());
    mtl_log("  ReadWriteTextureSupport:         %s (%lu)", rw_tex_str,
            (unsigned long)dev->readWriteTextureSupport());
    mtl_log("  RasterOrderGroupsSupported:      %d", dev->areRasterOrderGroupsSupported());
    mtl_log("  BarycentricCoordsSupported:      %d", dev->areBarycentricCoordsSupported());
    mtl_log("  ShaderBarycentricCoordinates:    %d", dev->supportsShaderBarycentricCoordinates());
    mtl_log("  PullModelInterpolation:          %d", dev->supportsPullModelInterpolation());
    mtl_log("  32BitFloatFiltering:             %d", dev->supports32BitFloatFiltering());
    mtl_log("  32BitMSAA:                       %d", dev->supports32BitMSAA());
    mtl_log("  QueryTextureLOD:                 %d", dev->supportsQueryTextureLOD());
    mtl_log("  BCTextureCompression:            %d", dev->supportsBCTextureCompression());
    mtl_log("  Depth24Stencil8PixelFormat:      %d",
            dev->isDepth24Stencil8PixelFormatSupported());
    mtl_log("  DynamicLibraries:                %d", dev->supportsDynamicLibraries());
    mtl_log("  RenderDynamicLibraries:          %d", dev->supportsRenderDynamicLibraries());
    mtl_log("  FunctionPointers:                %d", dev->supportsFunctionPointers());
    mtl_log("  FunctionPointersFromRender:      %d", dev->supportsFunctionPointersFromRender());
    mtl_log("  Raytracing:                      %d", dev->supportsRaytracing());
    mtl_log("  RaytracingFromRender:            %d", dev->supportsRaytracingFromRender());
    mtl_log("  PrimitiveMotionBlur:             %d", dev->supportsPrimitiveMotionBlur());
    mtl_log("  PlacementSparse:                 %d", dev->supportsPlacementSparse());
}

static void
info_multisample(struct mtl *mtl)
{
    const NS::SharedPtr<MTL::Device> &dev = mtl->dev;

    mtl_log("Multisampling:");
    mtl_log("  ProgrammableSamplePositions:     %d",
            dev->areProgrammableSamplePositionsSupported());

    static const NS::UInteger sample_counts[] = { 1, 2, 4, 8, 16, 32, 64 };
    for (size_t i = 0; i < ARRAY_SIZE(sample_counts); i++) {
        const NS::UInteger count = sample_counts[i];
        const bool supported = dev->supportsTextureSampleCount(count);
        mtl_log("  Sample Count %-2lu Supported:       %d", (unsigned long)count, supported);
        if (supported && dev->areProgrammableSamplePositionsSupported()) {
            MTL::SamplePosition positions[64];
            dev->getDefaultSamplePositions(positions, count);
            for (NS::UInteger s = 0; s < count; s++) {
                mtl_log("    Sample[%lu]:                     (%.4f, %.4f)", (unsigned long)s,
                        positions[s].x, positions[s].y);
            }
        }
    }
}

static void
info_amplification_and_vrr(struct mtl *mtl)
{
    const NS::SharedPtr<MTL::Device> &dev = mtl->dev;

    mtl_log("Vertex Amplification:");
    static const NS::UInteger amp_counts[] = { 1, 2, 4, 8, 16, 32 };
    for (size_t i = 0; i < ARRAY_SIZE(amp_counts); i++) {
        const NS::UInteger count = amp_counts[i];
        mtl_log("  Count %-2lu Supported:              %d", (unsigned long)count,
                dev->supportsVertexAmplificationCount(count));
    }

    mtl_log("Rasterization Rate Map:");
    static const NS::UInteger layer_counts[] = { 1, 2, 4, 8, 16, 32, 64 };
    for (size_t i = 0; i < ARRAY_SIZE(layer_counts); i++) {
        const NS::UInteger layers = layer_counts[i];
        mtl_log("  Layer Count %-2lu Supported:        %d", (unsigned long)layers,
                dev->supportsRasterizationRateMap(layers));
    }
}

static void
info_sparse_and_heaps(struct mtl *mtl)
{
    const NS::SharedPtr<MTL::Device> &dev = mtl->dev;

    static const struct {
        MTL::SparsePageSize page_size;
        const char *name;
    } page_sizes[] = {
        { MTL::SparsePageSize16, "PageSize16" },
        { MTL::SparsePageSize64, "PageSize64" },
        { MTL::SparsePageSize256, "PageSize256" },
    };

    mtl_log("Sparse Tiles and Heaps:");
    mtl_log("  Sparse Tile Size:                %lu bytes",
            (unsigned long)dev->sparseTileSizeInBytes());
    for (size_t i = 0; i < ARRAY_SIZE(page_sizes); i++) {
        mtl_log("  Sparse Tile Size (%-11s):  %lu bytes", page_sizes[i].name,
                (unsigned long)dev->sparseTileSizeInBytes(page_sizes[i].page_size));
    }

    static const struct {
        MTL::ResourceOptions options;
        const char *name;
    } buf_modes[] = {
        { MTL::ResourceStorageModeShared, "StorageModeShared" },
        { MTL::ResourceStorageModePrivate, "StorageModePrivate" },
    };
    for (size_t i = 0; i < ARRAY_SIZE(buf_modes); i++) {
        const MTL::SizeAndAlign sa = dev->heapBufferSizeAndAlign(1, buf_modes[i].options);
        mtl_log("  Heap Buffer 1B (%-18s): size=%lu, align=%lu", buf_modes[i].name,
                (unsigned long)sa.size, (unsigned long)sa.align);
    }
}

static void
info_timestamps_and_counters(struct mtl *mtl)
{
    const NS::SharedPtr<MTL::Device> &dev = mtl->dev;

    MTL::Timestamp cpu_ts = 0;
    MTL::Timestamp gpu_ts = 0;
    dev->sampleTimestamps(&cpu_ts, &gpu_ts);

    mtl_log("Timestamps and Counters:");
    mtl_log("  Timestamp Frequency:             %" PRIu64 " Hz", dev->queryTimestampFrequency());
    mtl_log("  Sampled CPU Timestamp:           %" PRIu64, (uint64_t)cpu_ts);
    mtl_log("  Sampled GPU Timestamp:           %" PRIu64, (uint64_t)gpu_ts);
    mtl_log("  MTL4 CounterHeap Timestamp Size: %lu bytes",
            (unsigned long)dev->sizeOfCounterHeapEntry(MTL4::CounterHeapTypeTimestamp));

    static const struct {
        MTL::CounterSamplingPoint point;
        const char *name;
    } sampling_points[] = {
        { MTL::CounterSamplingPointAtStageBoundary, "AtStageBoundary" },
        { MTL::CounterSamplingPointAtDrawBoundary, "AtDrawBoundary" },
        { MTL::CounterSamplingPointAtDispatchBoundary, "AtDispatchBoundary" },
        { MTL::CounterSamplingPointAtTileDispatchBoundary, "AtTileDispatchBoundary" },
        { MTL::CounterSamplingPointAtBlitBoundary, "AtBlitBoundary" },
    };

    mtl_log("  Counter Sampling Points:");
    for (size_t i = 0; i < ARRAY_SIZE(sampling_points); i++) {
        mtl_log("    %-30s %d", sampling_points[i].name,
                dev->supportsCounterSampling(sampling_points[i].point));
    }

    NS::Array *counter_sets = dev->counterSets();
    const NS::UInteger set_count = counter_sets ? counter_sets->count() : 0;
    mtl_log("  Counter Sets (%lu):", (unsigned long)set_count);
    for (NS::UInteger i = 0; i < set_count; i++) {
        MTL::CounterSet *set = counter_sets->object<MTL::CounterSet>(i);
        NS::Array *counters = set->counters();
        const NS::UInteger counter_count = counters ? counters->count() : 0;
        mtl_log("    [%lu] %s (%lu counters):", (unsigned long)i,
                set->name() ? set->name()->utf8String() : "Unknown",
                (unsigned long)counter_count);
        for (NS::UInteger j = 0; j < counter_count; j++) {
            MTL::Counter *counter = counters->object<MTL::Counter>(j);
            mtl_log("      [%lu] %s", (unsigned long)j,
                    counter->name() ? counter->name()->utf8String() : "Unknown");
        }
    }
}

int
main(void)
{
    struct mtl mtl;

    mtl_init(&mtl, NULL);

    info_general(&mtl);
    info_memory_and_limits(&mtl);
    info_gpu_families(&mtl);
    info_feature_sets(&mtl);
    info_tiers_and_features(&mtl);
    info_multisample(&mtl);
    info_amplification_and_vrr(&mtl);
    info_sparse_and_heaps(&mtl);
    info_timestamps_and_counters(&mtl);

    mtl_cleanup(&mtl);

    return 0;
}
