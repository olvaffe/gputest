/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef CLUTIL_H
#define CLUTIL_H

#include "util.h"

#include <dlfcn.h>

#define CL_TARGET_OPENCL_VERSION 300
#define CL_NO_PROTOTYPES
#include <CL/cl.h>
#include <CL/cl_ext.h>
#include <CL/cl_function_types.h>

#define LIBOPENCL_NAME "libOpenCL.so.1"

struct cl_device {
    cl_device_id id;

    cl_device_type type;
    cl_uint vendor_id;
    cl_uint max_compute_units;
    cl_uint max_work_item_dimensions;
    size_t *max_work_item_sizes;
    size_t max_work_group_size;
    cl_uint preferred_vector_width_short;
    cl_uint preferred_vector_width_char;
    cl_uint preferred_vector_width_int;
    cl_uint preferred_vector_width_float;
    cl_uint preferred_vector_width_long;
    cl_uint preferred_vector_width_double;
    cl_uint preferred_vector_width_half;
    cl_uint native_vector_width_char;
    cl_uint native_vector_width_short;
    cl_uint native_vector_width_int;
    cl_uint native_vector_width_long;
    cl_uint native_vector_width_float;
    cl_uint native_vector_width_double;
    cl_uint native_vector_width_half;
    cl_uint max_clock_frequency;
    cl_uint address_bits;
    cl_ulong max_mem_alloc_size;
    cl_bool image_support;
    cl_uint max_read_image_args;
    cl_uint max_write_image_args;
    cl_uint max_read_write_image_args;
    cl_name_version *ils;
    uint32_t il_count;
    size_t image2d_max_width;
    size_t image2d_max_height;
    size_t image3d_max_width;
    size_t image3d_max_height;
    size_t image3d_max_depth;
    size_t image_max_buffer_size;
    size_t image_max_array_size;
    cl_uint max_samplers;
    cl_uint image_pitch_alignment;
    cl_uint image_base_address_alignment;
    cl_uint max_pipe_args;
    cl_uint pipe_max_active_reservations;
    cl_uint pipe_max_packet_size;
    size_t max_parameter_size;
    cl_uint mem_base_addr_align;
    cl_device_fp_config single_fp_config;
    cl_device_fp_config double_fp_config;
    cl_device_mem_cache_type global_mem_cache_type;
    cl_uint global_mem_cacheline_size;
    cl_ulong global_mem_cache_size;
    cl_ulong global_mem_size;
    cl_ulong max_constant_buffer_size;
    cl_uint max_constant_args;
    size_t max_global_variable_size;
    size_t global_variable_preferred_total_size;
    cl_device_local_mem_type local_mem_type;
    cl_ulong local_mem_size;
    cl_bool error_correction_support;
    size_t profiling_timer_resolution;
    cl_bool endian_little;
    cl_bool available;
    cl_bool compiler_available;
    cl_bool linker_available;
    cl_device_exec_capabilities execution_capabilities;
    cl_command_queue_properties queue_on_host_properties;
    cl_command_queue_properties queue_on_device_properties;
    cl_uint queue_on_device_preferred_size;
    cl_uint queue_on_device_max_size;
    cl_uint max_on_device_queues;
    cl_uint max_on_device_events;
    cl_name_version *built_in_kernels;
    uint32_t built_in_kernel_count;
    cl_platform_id platform;
    char *name;
    char *vendor;
    char *driver_version;
    char *profile;
    cl_version version;
    char *version_str;
    cl_name_version *opencl_c_versions;
    uint32_t opencl_c_version_count;
    cl_name_version *opencl_c_features;
    uint32_t opencl_c_feature_count;
    cl_name_version *extensions;
    uint32_t extension_count;
    size_t printf_buffer_size;
    cl_bool preferred_interop_user_sync;
    cl_device_id parent_device;
    cl_uint partition_max_sub_devices;
    cl_device_partition_property *partition_properties;
    uint32_t partition_property_count;
    cl_device_affinity_domain partition_affinity_domain;
    cl_device_partition_property *partition_type;
    uint32_t partition_type_count;
    cl_uint reference_count;
    cl_device_svm_capabilities svm_capabilities;
    cl_uint preferred_platform_atomic_alignment;
    cl_uint preferred_global_atomic_alignment;
    cl_uint preferred_local_atomic_alignment;
    cl_uint max_num_sub_groups;
    cl_bool sub_group_independent_forward_progress;
    cl_device_atomic_capabilities atomic_memory_capabilities;
    cl_device_atomic_capabilities atomic_fence_capabilities;
    cl_bool non_uniform_work_group_support;
    cl_bool work_group_collective_functions_support;
    cl_bool generic_address_space_support;
    cl_device_device_enqueue_capabilities device_enqueue_capabilities;
    cl_bool pipe_support;
    size_t preferred_work_group_size_multiple;
    char *latest_conformance_version_passed;
};

struct cl_platform {
    cl_platform_id id;

    char *profile;
    char *version_str;
    cl_version version;
    char *name;
    char *vendor;
    cl_name_version *extensions;
    uint32_t extension_count;
    cl_ulong host_timer_resolution;

    struct cl_device *devices;
    uint32_t device_count;
};

struct cl_init_params {
    int unused;
};

struct cl {
    struct cl_init_params params;

    struct {
        void *handle;
#define PFN(name) cl##name##_fn name;
#include "clutil_entrypoints.inc"
    };

    int err;

    struct cl_platform *platforms;
    uint32_t platform_count;
};

static inline void PRINTFLIKE(1, 2) cl_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_logv("CL", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN cl_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    u_diev("CL", format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(2, 3) cl_check(struct cl *cl, const char *format, ...)
{
    if (cl->err == CL_SUCCESS)
        return;

    va_list ap;
    va_start(ap, format);
    u_diev("CL", format, ap);
    va_end(ap);
}

static inline const char *
cl_device_type_to_str(cl_device_type val)
{
    /* clang-format off */
    switch (val) {
#define CASE(v) case CL_DEVICE_TYPE_ ##v: return #v
    CASE(CPU);
    CASE(GPU);
    CASE(ACCELERATOR);
    CASE(CUSTOM);
    default: return "UNKNOWN";
#undef CASE
    }
    /* clang-format on */
}

static inline const char *
cl_device_fp_config_to_str(cl_device_fp_config val, char *str, size_t size)
{
    /* clang-format off */
    static const struct u_bitmask_desc descs[] = {
#define DESC(v) { .bitmask = CL_FP_ ##v, .str = #v }
        DESC(DENORM),
        DESC(INF_NAN),
        DESC(ROUND_TO_NEAREST),
        DESC(ROUND_TO_ZERO),
        DESC(ROUND_TO_INF),
        DESC(FMA),
        DESC(SOFT_FLOAT),
        DESC(CORRECTLY_ROUNDED_DIVIDE_SQRT),
#undef DESC
    };
    /* clang-format on */

    return u_bitmask_to_str(val, descs, ARRAY_SIZE(descs), str, size);
}

static inline const char *
cl_device_mem_cache_type_to_str(cl_device_mem_cache_type val)
{
    /* clang-format off */
    switch (val) {
#define CASE(v) case CL_ ##v: return #v
    CASE(NONE);
    CASE(READ_ONLY_CACHE);
    CASE(READ_WRITE_CACHE);
    default: return "UNKNOWN";
#undef CASE
    }
    /* clang-format on */
}

static inline const char *
cl_device_local_mem_type_to_str(cl_device_local_mem_type val)
{
    /* clang-format off */
    switch (val) {
#define CASE(v) case CL_ ##v: return #v
    CASE(LOCAL);
    CASE(GLOBAL);
    default: return "UNKNOWN";
#undef CASE
    }
    /* clang-format on */
}

static inline const char *
cl_device_exec_capabilities_to_str(cl_device_exec_capabilities val, char *str, size_t size)
{
    /* clang-format off */
    static const struct u_bitmask_desc descs[] = {
#define DESC(v) { .bitmask = CL_EXEC_ ##v, .str = #v }
        DESC(KERNEL),
        DESC(NATIVE_KERNEL),
#undef DESC
    };
    /* clang-format on */

    return u_bitmask_to_str(val, descs, ARRAY_SIZE(descs), str, size);
}

static inline const char *
cl_command_queue_properties_to_str(cl_command_queue_properties val, char *str, size_t size)
{
    /* clang-format off */
    static const struct u_bitmask_desc descs[] = {
#define DESC(v) { .bitmask = CL_QUEUE_ ##v, .str = #v }
        DESC(OUT_OF_ORDER_EXEC_MODE_ENABLE),
        DESC(PROFILING_ENABLE),
        DESC(ON_DEVICE),
        DESC(ON_DEVICE_DEFAULT),
#undef DESC
    };
    /* clang-format on */

    return u_bitmask_to_str(val, descs, ARRAY_SIZE(descs), str, size);
}

static inline const char *
cl_device_svm_capabilities_to_str(cl_device_svm_capabilities val, char *str, size_t size)
{
    /* clang-format off */
    static const struct u_bitmask_desc descs[] = {
#define DESC(v) { .bitmask = CL_DEVICE_SVM_ ##v, .str = #v }
        DESC(COARSE_GRAIN_BUFFER),
        DESC(FINE_GRAIN_BUFFER),
        DESC(FINE_GRAIN_SYSTEM),
        DESC(ATOMICS),
#undef DESC
    };
    /* clang-format on */

    return u_bitmask_to_str(val, descs, ARRAY_SIZE(descs), str, size);
}

static inline const char *
cl_device_atomic_capabilities_to_str(cl_device_atomic_capabilities val, char *str, size_t size)
{
    /* clang-format off */
    static const struct u_bitmask_desc descs[] = {
#define DESC(v) { .bitmask = CL_DEVICE_ATOMIC_ ##v, .str = #v }
        DESC(ORDER_RELAXED),
        DESC(ORDER_ACQ_REL),
        DESC(ORDER_SEQ_CST),
        DESC(SCOPE_WORK_ITEM),
        DESC(SCOPE_WORK_GROUP),
        DESC(SCOPE_DEVICE),
        DESC(SCOPE_ALL_DEVICES),
#undef DESC
    };
    /* clang-format on */

    return u_bitmask_to_str(val, descs, ARRAY_SIZE(descs), str, size);
}

static inline void
cl_init_library(struct cl *cl)
{
    cl->handle = dlopen(LIBOPENCL_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!cl->handle)
        cl_die("failed to load %s: %s", LIBOPENCL_NAME, dlerror());

#define PFN(name)                                                                                \
    cl->name = (cl##name##_fn)dlsym(cl->handle, "cl" #name);                                     \
    if (!cl->name)                                                                               \
        cl_die("no cl" #name);
#include "clutil_entrypoints.inc"
}

static inline int
cl_sort_name_versions(const void *a, const void *b)
{
    const cl_name_version *ext1 = a;
    const cl_name_version *ext2 = b;
    return strcmp(ext1->name, ext2->name);
}

static inline void
cl_get_platform_info(
    struct cl *cl, cl_platform_id plat, cl_platform_info param, void *buf, size_t size)
{
    size_t real_size;
    cl->err = cl->GetPlatformInfo(plat, param, size, buf, &real_size);
    cl_check(cl, "failed to get platform info");
    if (size != real_size)
        cl_die("bad platform info size");
}

static inline void *
cl_get_platform_info_alloc(struct cl *cl,
                           cl_platform_id plat,
                           cl_platform_info param,
                           size_t *size)
{
    size_t real_size;
    cl->err = cl->GetPlatformInfo(plat, param, 0, NULL, &real_size);
    cl_check(cl, "failed to get platform info size");

    void *buf = malloc(real_size);
    if (!buf)
        cl_die("failed to alloc platform info buf");
    cl_get_platform_info(cl, plat, param, buf, real_size);

    if (size)
        *size = real_size;
    return buf;
}

static inline void
cl_init_platforms(struct cl *cl)
{
    uint32_t count;
    cl->err = cl->GetPlatformIDs(0, NULL, &count);
    cl_check(cl, "failed to get platform count");

    cl_platform_id *ids = malloc(sizeof(*ids) * count);
    if (!ids)
        cl_die("failed to alloc platform ids");

    cl->err = cl->GetPlatformIDs(count, ids, &count);
    cl_check(cl, "failed to get platform ids");

    cl->platforms = malloc(sizeof(*cl->platforms) * count);
    if (!cl->platforms)
        cl_die("failed to alloc platforms");
    cl->platform_count = count;

    for (uint32_t i = 0; i < count; i++) {
        struct cl_platform *plat = &cl->platforms[i];
        size_t size;

        plat->id = ids[i];

        plat->profile = cl_get_platform_info_alloc(cl, plat->id, CL_PLATFORM_PROFILE, NULL);
        plat->version_str = cl_get_platform_info_alloc(cl, plat->id, CL_PLATFORM_VERSION, NULL);
        cl_get_platform_info(cl, plat->id, CL_PLATFORM_NUMERIC_VERSION, &plat->version,
                             sizeof(plat->version));
        plat->name = cl_get_platform_info_alloc(cl, plat->id, CL_PLATFORM_NAME, NULL);
        plat->vendor = cl_get_platform_info_alloc(cl, plat->id, CL_PLATFORM_VENDOR, NULL);

        plat->extensions =
            cl_get_platform_info_alloc(cl, plat->id, CL_PLATFORM_EXTENSIONS_WITH_VERSION, &size);
        plat->extension_count = size / sizeof(*plat->extensions);
        qsort(plat->extensions, plat->extension_count, sizeof(*plat->extensions),
              cl_sort_name_versions);

        cl_get_platform_info(cl, plat->id, CL_PLATFORM_HOST_TIMER_RESOLUTION,
                             &plat->host_timer_resolution, sizeof(plat->host_timer_resolution));
    }
}

static inline void
cl_get_device_info(struct cl *cl, cl_device_id plat, cl_device_info param, void *buf, size_t size)
{
    size_t real_size;
    cl->err = cl->GetDeviceInfo(plat, param, size, buf, &real_size);
    cl_check(cl, "failed to get device info");
    if (size != real_size)
        cl_die("bad device info size");
}

static inline void *
cl_get_device_info_alloc(struct cl *cl, cl_device_id plat, cl_device_info param, size_t *size)
{
    size_t real_size;
    cl->err = cl->GetDeviceInfo(plat, param, 0, NULL, &real_size);
    cl_check(cl, "failed to get device info size");

    void *buf = malloc(real_size);
    if (!buf)
        cl_die("failed to alloc device info buf");
    cl_get_device_info(cl, plat, param, buf, real_size);

    if (size)
        *size = real_size;
    return buf;
}

static inline void
cl_init_devices(struct cl *cl, uint32_t idx)
{
    struct cl_platform *plat = &cl->platforms[idx];

    uint32_t count;
    cl->err = cl->GetDeviceIDs(plat->id, CL_DEVICE_TYPE_ALL, 0, NULL, &count);
    cl_check(cl, "failed to get device count");

    cl_device_id *ids = malloc(sizeof(*ids) * count);
    if (!ids)
        cl_die("failed to alloc device ids");

    cl->err = cl->GetDeviceIDs(plat->id, CL_DEVICE_TYPE_ALL, count, ids, &count);
    cl_check(cl, "failed to get device ids");

    plat->devices = malloc(sizeof(*plat->devices) * count);
    if (!plat->devices)
        cl_die("failed to alloc devices");
    plat->device_count = count;

    for (uint32_t i = 0; i < count; i++) {
        struct cl_device *dev = &plat->devices[i];
        size_t size;

        dev->id = ids[i];

        cl_get_device_info(cl, dev->id, CL_DEVICE_TYPE, &dev->type, sizeof(dev->type));
        cl_get_device_info(cl, dev->id, CL_DEVICE_VENDOR_ID, &dev->vendor_id,
                           sizeof(dev->vendor_id));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_COMPUTE_UNITS, &dev->max_compute_units,
                           sizeof(dev->max_compute_units));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
                           &dev->max_work_item_dimensions, sizeof(dev->max_work_item_dimensions));

        dev->max_work_item_sizes =
            cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_MAX_WORK_ITEM_SIZES, NULL);

        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_WORK_GROUP_SIZE, &dev->max_work_group_size,
                           sizeof(dev->max_work_group_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,
                           &dev->preferred_vector_width_char,
                           sizeof(dev->preferred_vector_width_char));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,
                           &dev->preferred_vector_width_short,
                           sizeof(dev->preferred_vector_width_short));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,
                           &dev->preferred_vector_width_int,
                           sizeof(dev->preferred_vector_width_int));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,
                           &dev->preferred_vector_width_long,
                           sizeof(dev->preferred_vector_width_long));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,
                           &dev->preferred_vector_width_float,
                           sizeof(dev->preferred_vector_width_float));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,
                           &dev->preferred_vector_width_double,
                           sizeof(dev->preferred_vector_width_double));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF,
                           &dev->preferred_vector_width_half,
                           sizeof(dev->preferred_vector_width_half));
        cl_get_device_info(cl, dev->id, CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR,
                           &dev->native_vector_width_char, sizeof(dev->native_vector_width_char));
        cl_get_device_info(cl, dev->id, CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT,
                           &dev->native_vector_width_short,
                           sizeof(dev->native_vector_width_short));
        cl_get_device_info(cl, dev->id, CL_DEVICE_NATIVE_VECTOR_WIDTH_INT,
                           &dev->native_vector_width_int, sizeof(dev->native_vector_width_int));
        cl_get_device_info(cl, dev->id, CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG,
                           &dev->native_vector_width_long, sizeof(dev->native_vector_width_long));
        cl_get_device_info(cl, dev->id, CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT,
                           &dev->native_vector_width_float,
                           sizeof(dev->native_vector_width_float));
        cl_get_device_info(cl, dev->id, CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE,
                           &dev->native_vector_width_double,
                           sizeof(dev->native_vector_width_double));
        cl_get_device_info(cl, dev->id, CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF,
                           &dev->native_vector_width_half, sizeof(dev->native_vector_width_half));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_CLOCK_FREQUENCY, &dev->max_clock_frequency,
                           sizeof(dev->max_clock_frequency));
        cl_get_device_info(cl, dev->id, CL_DEVICE_ADDRESS_BITS, &dev->address_bits,
                           sizeof(dev->address_bits));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, &dev->max_mem_alloc_size,
                           sizeof(dev->max_mem_alloc_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE_SUPPORT, &dev->image_support,
                           sizeof(dev->image_support));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_READ_IMAGE_ARGS, &dev->max_read_image_args,
                           sizeof(dev->max_read_image_args));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_WRITE_IMAGE_ARGS,
                           &dev->max_write_image_args, sizeof(dev->max_write_image_args));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS,
                           &dev->max_read_write_image_args,
                           sizeof(dev->max_read_write_image_args));

        dev->ils = cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_ILS_WITH_VERSION, &size);
        dev->il_count = size / sizeof(*dev->ils);

        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE2D_MAX_WIDTH, &dev->image2d_max_width,
                           sizeof(dev->image2d_max_width));
        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE2D_MAX_HEIGHT, &dev->image2d_max_height,
                           sizeof(dev->image2d_max_height));
        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE3D_MAX_WIDTH, &dev->image3d_max_width,
                           sizeof(dev->image3d_max_width));
        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE3D_MAX_HEIGHT, &dev->image3d_max_height,
                           sizeof(dev->image3d_max_height));
        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE3D_MAX_DEPTH, &dev->image3d_max_depth,
                           sizeof(dev->image3d_max_depth));
        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE_MAX_BUFFER_SIZE,
                           &dev->image_max_buffer_size, sizeof(dev->image_max_buffer_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE_MAX_ARRAY_SIZE,
                           &dev->image_max_array_size, sizeof(dev->image_max_array_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_SAMPLERS, &dev->max_samplers,
                           sizeof(dev->max_samplers));
        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE_PITCH_ALIGNMENT,
                           &dev->image_pitch_alignment, sizeof(dev->image_pitch_alignment));
        cl_get_device_info(cl, dev->id, CL_DEVICE_IMAGE_BASE_ADDRESS_ALIGNMENT,
                           &dev->image_base_address_alignment,
                           sizeof(dev->image_base_address_alignment));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_PIPE_ARGS, &dev->max_pipe_args,
                           sizeof(dev->max_pipe_args));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PIPE_MAX_ACTIVE_RESERVATIONS,
                           &dev->pipe_max_active_reservations,
                           sizeof(dev->pipe_max_active_reservations));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PIPE_MAX_PACKET_SIZE,
                           &dev->pipe_max_packet_size, sizeof(dev->pipe_max_packet_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_PARAMETER_SIZE, &dev->max_parameter_size,
                           sizeof(dev->max_parameter_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MEM_BASE_ADDR_ALIGN, &dev->mem_base_addr_align,
                           sizeof(dev->mem_base_addr_align));
        cl_get_device_info(cl, dev->id, CL_DEVICE_SINGLE_FP_CONFIG, &dev->single_fp_config,
                           sizeof(dev->single_fp_config));
        cl_get_device_info(cl, dev->id, CL_DEVICE_DOUBLE_FP_CONFIG, &dev->double_fp_config,
                           sizeof(dev->double_fp_config));
        cl_get_device_info(cl, dev->id, CL_DEVICE_GLOBAL_MEM_CACHE_TYPE,
                           &dev->global_mem_cache_type, sizeof(dev->global_mem_cache_type));
        cl_get_device_info(cl, dev->id, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,
                           &dev->global_mem_cacheline_size,
                           sizeof(dev->global_mem_cacheline_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE,
                           &dev->global_mem_cache_size, sizeof(dev->global_mem_cache_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_GLOBAL_MEM_SIZE, &dev->global_mem_size,
                           sizeof(dev->global_mem_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,
                           &dev->max_constant_buffer_size, sizeof(dev->max_constant_buffer_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_CONSTANT_ARGS, &dev->max_constant_args,
                           sizeof(dev->max_constant_args));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_GLOBAL_VARIABLE_SIZE,
                           &dev->max_global_variable_size, sizeof(dev->max_global_variable_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_GLOBAL_VARIABLE_PREFERRED_TOTAL_SIZE,
                           &dev->global_variable_preferred_total_size,
                           sizeof(dev->global_variable_preferred_total_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_LOCAL_MEM_TYPE, &dev->local_mem_type,
                           sizeof(dev->local_mem_type));
        cl_get_device_info(cl, dev->id, CL_DEVICE_LOCAL_MEM_SIZE, &dev->local_mem_size,
                           sizeof(dev->local_mem_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_ERROR_CORRECTION_SUPPORT,
                           &dev->error_correction_support, sizeof(dev->error_correction_support));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PROFILING_TIMER_RESOLUTION,
                           &dev->profiling_timer_resolution,
                           sizeof(dev->profiling_timer_resolution));
        cl_get_device_info(cl, dev->id, CL_DEVICE_ENDIAN_LITTLE, &dev->endian_little,
                           sizeof(dev->endian_little));
        cl_get_device_info(cl, dev->id, CL_DEVICE_AVAILABLE, &dev->available,
                           sizeof(dev->available));
        cl_get_device_info(cl, dev->id, CL_DEVICE_COMPILER_AVAILABLE, &dev->compiler_available,
                           sizeof(dev->compiler_available));
        cl_get_device_info(cl, dev->id, CL_DEVICE_LINKER_AVAILABLE, &dev->linker_available,
                           sizeof(dev->linker_available));
        cl_get_device_info(cl, dev->id, CL_DEVICE_EXECUTION_CAPABILITIES,
                           &dev->execution_capabilities, sizeof(dev->execution_capabilities));
        cl_get_device_info(cl, dev->id, CL_DEVICE_QUEUE_ON_HOST_PROPERTIES,
                           &dev->queue_on_host_properties, sizeof(dev->queue_on_host_properties));
        cl_get_device_info(cl, dev->id, CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES,
                           &dev->queue_on_device_properties,
                           sizeof(dev->queue_on_device_properties));
        cl_get_device_info(cl, dev->id, CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,
                           &dev->queue_on_device_preferred_size,
                           sizeof(dev->queue_on_device_preferred_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE,
                           &dev->queue_on_device_max_size, sizeof(dev->queue_on_device_max_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_ON_DEVICE_QUEUES,
                           &dev->max_on_device_queues, sizeof(dev->max_on_device_queues));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_ON_DEVICE_EVENTS,
                           &dev->max_on_device_events, sizeof(dev->max_on_device_events));

        dev->built_in_kernels =
            cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION, &size);
        dev->built_in_kernel_count = size / sizeof(*dev->built_in_kernels);

        cl_get_device_info(cl, dev->id, CL_DEVICE_PLATFORM, &dev->platform,
                           sizeof(dev->platform));

        dev->name = cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_NAME, NULL);
        dev->vendor = cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_VENDOR, NULL);
        dev->driver_version = cl_get_device_info_alloc(cl, dev->id, CL_DRIVER_VERSION, NULL);
        dev->profile = cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_PROFILE, NULL);

        cl_get_device_info(cl, dev->id, CL_DEVICE_NUMERIC_VERSION, &dev->version,
                           sizeof(dev->version));
        dev->version_str = cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_VERSION, NULL);

        dev->opencl_c_versions =
            cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_OPENCL_C_ALL_VERSIONS, &size);
        dev->opencl_c_version_count = size / sizeof(*dev->opencl_c_versions);

        dev->opencl_c_features =
            cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_OPENCL_C_FEATURES, &size);
        dev->opencl_c_feature_count = size / sizeof(*dev->opencl_c_features);
        qsort(dev->opencl_c_features, dev->opencl_c_feature_count,
              sizeof(*dev->opencl_c_features), cl_sort_name_versions);

        dev->extensions =
            cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_EXTENSIONS_WITH_VERSION, &size);
        dev->extension_count = size / sizeof(*dev->extensions);
        qsort(dev->extensions, dev->extension_count, sizeof(*dev->extensions),
              cl_sort_name_versions);

        cl_get_device_info(cl, dev->id, CL_DEVICE_PRINTF_BUFFER_SIZE, &dev->printf_buffer_size,
                           sizeof(dev->printf_buffer_size));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_INTEROP_USER_SYNC,
                           &dev->preferred_interop_user_sync,
                           sizeof(dev->preferred_interop_user_sync));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PARENT_DEVICE, &dev->parent_device,
                           sizeof(dev->parent_device));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PARTITION_MAX_SUB_DEVICES,
                           &dev->partition_max_sub_devices,
                           sizeof(dev->partition_max_sub_devices));

        dev->partition_properties =
            cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_PARTITION_PROPERTIES, &size);
        dev->partition_property_count = size / sizeof(*dev->partition_properties);

        cl_get_device_info(cl, dev->id, CL_DEVICE_PARTITION_AFFINITY_DOMAIN,
                           &dev->partition_affinity_domain,
                           sizeof(dev->partition_affinity_domain));

        dev->partition_type =
            cl_get_device_info_alloc(cl, dev->id, CL_DEVICE_PARTITION_TYPE, &size);
        dev->partition_type_count = size / sizeof(*dev->partition_type);

        cl_get_device_info(cl, dev->id, CL_DEVICE_REFERENCE_COUNT, &dev->reference_count,
                           sizeof(dev->reference_count));
        cl_get_device_info(cl, dev->id, CL_DEVICE_SVM_CAPABILITIES, &dev->svm_capabilities,
                           sizeof(dev->svm_capabilities));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_PLATFORM_ATOMIC_ALIGNMENT,
                           &dev->preferred_platform_atomic_alignment,
                           sizeof(dev->preferred_platform_atomic_alignment));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_GLOBAL_ATOMIC_ALIGNMENT,
                           &dev->preferred_global_atomic_alignment,
                           sizeof(dev->preferred_global_atomic_alignment));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_LOCAL_ATOMIC_ALIGNMENT,
                           &dev->preferred_local_atomic_alignment,
                           sizeof(dev->preferred_local_atomic_alignment));
        cl_get_device_info(cl, dev->id, CL_DEVICE_MAX_NUM_SUB_GROUPS, &dev->max_num_sub_groups,
                           sizeof(dev->max_num_sub_groups));
        cl_get_device_info(cl, dev->id, CL_DEVICE_SUB_GROUP_INDEPENDENT_FORWARD_PROGRESS,
                           &dev->sub_group_independent_forward_progress,
                           sizeof(dev->sub_group_independent_forward_progress));
        cl_get_device_info(cl, dev->id, CL_DEVICE_ATOMIC_MEMORY_CAPABILITIES,
                           &dev->atomic_memory_capabilities,
                           sizeof(dev->atomic_memory_capabilities));
        cl_get_device_info(cl, dev->id, CL_DEVICE_ATOMIC_FENCE_CAPABILITIES,
                           &dev->atomic_fence_capabilities,
                           sizeof(dev->atomic_fence_capabilities));
        cl_get_device_info(cl, dev->id, CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT,
                           &dev->non_uniform_work_group_support,
                           sizeof(dev->non_uniform_work_group_support));
        cl_get_device_info(cl, dev->id, CL_DEVICE_WORK_GROUP_COLLECTIVE_FUNCTIONS_SUPPORT,
                           &dev->work_group_collective_functions_support,
                           sizeof(dev->work_group_collective_functions_support));
        cl_get_device_info(cl, dev->id, CL_DEVICE_GENERIC_ADDRESS_SPACE_SUPPORT,
                           &dev->generic_address_space_support,
                           sizeof(dev->generic_address_space_support));
        cl_get_device_info(cl, dev->id, CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES,
                           &dev->device_enqueue_capabilities,
                           sizeof(dev->device_enqueue_capabilities));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PIPE_SUPPORT, &dev->pipe_support,
                           sizeof(dev->pipe_support));
        cl_get_device_info(cl, dev->id, CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                           &dev->preferred_work_group_size_multiple,
                           sizeof(dev->preferred_work_group_size_multiple));

        dev->latest_conformance_version_passed = cl_get_device_info_alloc(
            cl, dev->id, CL_DEVICE_LATEST_CONFORMANCE_VERSION_PASSED, NULL);
    }
}

static inline void
cl_init(struct cl *cl, const struct cl_init_params *params)
{
    memset(cl, 0, sizeof(*cl));
    if (params)
        cl->params = *params;

    cl_init_library(cl);
    cl_init_platforms(cl);

    for (uint32_t i = 0; i < cl->platform_count; i++)
        cl_init_devices(cl, i);
}

static inline void
cl_cleanup(struct cl *cl)
{
    for (uint32_t i = 0; i < cl->platform_count; i++) {
        struct cl_platform *plat = &cl->platforms[i];

        free(plat->profile);
        free(plat->version_str);
        free(plat->name);
        free(plat->vendor);
        free(plat->extensions);

        for (uint32_t j = 0; j < plat->device_count; j++) {
            struct cl_device *dev = &plat->devices[j];

            free(dev->max_work_item_sizes);
            free(dev->ils);
            free(dev->built_in_kernels);
            free(dev->name);
            free(dev->vendor);
            free(dev->driver_version);
            free(dev->profile);
            free(dev->version_str);
            free(dev->opencl_c_versions);
            free(dev->opencl_c_features);
            free(dev->extensions);
            free(dev->partition_properties);
            free(dev->partition_type);
            free(dev->latest_conformance_version_passed);
        }
        free(plat->devices);
    }
    free(cl->platforms);

    dlclose(cl->handle);
}

#endif /* CLUTIL_H */
