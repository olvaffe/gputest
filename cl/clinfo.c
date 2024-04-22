/*
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "clutil.h"

static void
info_dump_device(struct cl *cl, uint32_t plat_idx, uint32_t dev_idx)
{
    const struct cl_platform *plat = &cl->platforms[plat_idx];
    const struct cl_device *dev = &plat->devices[dev_idx];
    char str[256];

    cl_log("platform #%d device #%d:", plat_idx, dev_idx);

    cl_log("  type: %s", cl_device_type_to_str(dev->type, str, sizeof(str)));
    cl_log("  vendor_id: 0x%x", dev->vendor_id);
    cl_log("  max_compute_units: %u", dev->max_compute_units);
    cl_log("  max_work_item_dimensions: %u", dev->max_work_item_dimensions);

    cl_log("  max_work_item_sizes:");
    for (uint32_t i = 0; i < dev->max_work_item_dimensions; i++)
        cl_log("    %zu", dev->max_work_item_sizes[i]);

    cl_log("  max_work_group_size: %zu", dev->max_work_group_size);
    cl_log("  preferred_vector_width_char: %u", dev->preferred_vector_width_char);
    cl_log("  preferred_vector_width_short: %u", dev->preferred_vector_width_short);
    cl_log("  preferred_vector_width_int: %u", dev->preferred_vector_width_int);
    cl_log("  preferred_vector_width_long: %u", dev->preferred_vector_width_long);
    cl_log("  preferred_vector_width_float: %u", dev->preferred_vector_width_float);
    cl_log("  preferred_vector_width_double: %u", dev->preferred_vector_width_double);
    cl_log("  preferred_vector_width_half: %u", dev->preferred_vector_width_half);
    cl_log("  native_vector_width_char: %u", dev->native_vector_width_char);
    cl_log("  native_vector_width_short: %u", dev->native_vector_width_short);
    cl_log("  native_vector_width_int: %u", dev->native_vector_width_int);
    cl_log("  native_vector_width_long: %u", dev->native_vector_width_long);
    cl_log("  native_vector_width_float: %u", dev->native_vector_width_float);
    cl_log("  native_vector_width_double: %u", dev->native_vector_width_double);
    cl_log("  native_vector_width_half: %u", dev->native_vector_width_half);
    cl_log("  max_clock_frequency: %u", dev->max_clock_frequency);
    cl_log("  address_bits: %u", dev->address_bits);
    cl_log("  max_mem_alloc_size: %lu", dev->max_mem_alloc_size);
    cl_log("  image_support: %u", dev->image_support);
    cl_log("  max_read_image_args: %u", dev->max_read_image_args);
    cl_log("  max_write_image_args: %u", dev->max_write_image_args);
    cl_log("  max_read_write_image_args: %u", dev->max_read_write_image_args);

    cl_log("  ILs:");
    for (uint32_t i = 0; i < dev->il_count; i++) {
        const cl_name_version *v = &dev->ils[i];
        cl_log("    %s: %d.%d.%d", v->name, CL_VERSION_MAJOR(v->version),
               CL_VERSION_MINOR(v->version), CL_VERSION_PATCH(v->version));
    }

    cl_log("  image2d_max_width: %zu", dev->image2d_max_width);
    cl_log("  image2d_max_height: %zu", dev->image2d_max_height);
    cl_log("  image3d_max_width: %zu", dev->image3d_max_width);
    cl_log("  image3d_max_height: %zu", dev->image3d_max_height);
    cl_log("  image3d_max_depth: %zu", dev->image3d_max_depth);
    cl_log("  image_max_buffer_size: %zu", dev->image_max_buffer_size);
    cl_log("  image_max_array_size: %zu", dev->image_max_array_size);
    cl_log("  max_samplers: %u", dev->max_samplers);
    cl_log("  image_pitch_alignment: %u", dev->image_pitch_alignment);
    cl_log("  image_base_address_alignment: %u", dev->image_base_address_alignment);
    cl_log("  max_pipe_args: %u", dev->max_pipe_args);
    cl_log("  pipe_max_active_reservations: %u", dev->pipe_max_active_reservations);
    cl_log("  pipe_max_packet_size: %u", dev->pipe_max_packet_size);
    cl_log("  max_parameter_size: %zu", dev->max_parameter_size);
    cl_log("  mem_base_addr_align: %u", dev->mem_base_addr_align);
    cl_log("  single_fp_config: %s",
           cl_device_fp_config_to_str(dev->single_fp_config, str, sizeof(str)));
    cl_log("  double_fp_config: %s",
           cl_device_fp_config_to_str(dev->double_fp_config, str, sizeof(str)));
    cl_log("  half_fp_config: %s",
           cl_device_fp_config_to_str(dev->half_fp_config, str, sizeof(str)));
    cl_log("  global_mem_cache_type: %s",
           cl_device_mem_cache_type_to_str(dev->global_mem_cache_type));
    cl_log("  global_mem_cacheline_size: %u", dev->global_mem_cacheline_size);
    cl_log("  global_mem_cache_size: %lu", dev->global_mem_cache_size);
    cl_log("  global_mem_size: %lu", dev->global_mem_size);
    cl_log("  max_constant_buffer_size: %lu", dev->max_constant_buffer_size);
    cl_log("  max_constant_args: %u", dev->max_constant_args);
    cl_log("  max_global_variable_size: %zu", dev->max_global_variable_size);
    cl_log("  global_variable_preferred_total_size: %zu",
           dev->global_variable_preferred_total_size);
    cl_log("  local_mem_type: %s", cl_device_local_mem_type_to_str(dev->local_mem_type));
    cl_log("  local_mem_size: %lu", dev->local_mem_size);
    cl_log("  error_correction_support: %d", dev->error_correction_support);
    cl_log("  profiling_timer_resolution: %zu", dev->profiling_timer_resolution);
    cl_log("  endian_little: %d", dev->endian_little);
    cl_log("  available: %d", dev->available);
    cl_log("  compiler_available: %d", dev->compiler_available);
    cl_log("  linker_available: %d", dev->linker_available);
    cl_log("  execution_capabilities: %s",
           cl_device_exec_capabilities_to_str(dev->execution_capabilities, str, sizeof(str)));
    cl_log("  queue_on_host_properties: %s",
           cl_command_queue_properties_to_str(dev->queue_on_host_properties, str, sizeof(str)));
    cl_log("  queue_on_device_properties: %s",
           cl_command_queue_properties_to_str(dev->queue_on_device_properties, str, sizeof(str)));
    cl_log("  queue_on_device_preferred_size: %u", dev->queue_on_device_preferred_size);
    cl_log("  queue_on_device_max_size: %u", dev->queue_on_device_max_size);
    cl_log("  max_on_device_queues: %u", dev->max_on_device_queues);
    cl_log("  max_on_device_events: %u", dev->max_on_device_events);

    cl_log("  built-in kernels:");
    for (uint32_t i = 0; i < dev->built_in_kernel_count; i++) {
        const cl_name_version *v = &dev->built_in_kernels[i];
        cl_log("    %s: %d.%d.%d", v->name, CL_VERSION_MAJOR(v->version),
               CL_VERSION_MINOR(v->version), CL_VERSION_PATCH(v->version));
    }

    cl_log("  platform: %p", dev->platform);
    cl_log("  name: %s", dev->name);
    cl_log("  vendor: %s", dev->vendor);
    cl_log("  driver_version: %s", dev->driver_version);
    cl_log("  profile: %s", dev->profile);

    cl_log("  version: %d.%d.%d (%s)", CL_VERSION_MAJOR(dev->version),
           CL_VERSION_MINOR(dev->version), CL_VERSION_PATCH(dev->version), dev->version_str);

    cl_log("  OpenCL C versions:");
    for (uint32_t i = 0; i < dev->opencl_c_version_count; i++) {
        const cl_name_version *v = &dev->opencl_c_versions[i];
        cl_log("    %s: %d.%d.%d", v->name, CL_VERSION_MAJOR(v->version),
               CL_VERSION_MINOR(v->version), CL_VERSION_PATCH(v->version));
    }
    cl_log("  OpenCL C features:");
    for (uint32_t i = 0; i < dev->opencl_c_feature_count; i++) {
        const cl_name_version *v = &dev->opencl_c_features[i];
        cl_log("    %s: %d.%d.%d", v->name, CL_VERSION_MAJOR(v->version),
               CL_VERSION_MINOR(v->version), CL_VERSION_PATCH(v->version));
    }

    cl_log("  extensions:");
    for (uint32_t i = 0; i < dev->extension_count; i++) {
        const cl_name_version *v = &dev->extensions[i];
        cl_log("    %s: %d.%d.%d", v->name, CL_VERSION_MAJOR(v->version),
               CL_VERSION_MINOR(v->version), CL_VERSION_PATCH(v->version));
    }
    cl_log("  printf_buffer_size: %zu", dev->printf_buffer_size);
    cl_log("  preferred_interop_user_sync: %d", dev->preferred_interop_user_sync);
    cl_log("  parent_device: %p", dev->parent_device);
    cl_log("  partition_max_sub_devices: %u", dev->partition_max_sub_devices);

    cl_log("  partition_properties:");
    for (uint32_t i = 0; i < dev->partition_property_count; i++)
        cl_log("    0x%lx", dev->partition_properties[i]);

    cl_log("  partition_affinity_domain: 0x%lx", dev->partition_affinity_domain);

    cl_log("  partition_type:");
    for (uint32_t i = 0; i < dev->partition_type_count; i++)
        cl_log("    0x%lx", dev->partition_type[i]);

    cl_log("  reference_count: %u", dev->reference_count);
    cl_log("  svm_capabilities: %s",
           cl_device_svm_capabilities_to_str(dev->svm_capabilities, str, sizeof(str)));
    cl_log("  preferred_platform_atomic_alignment: %u", dev->preferred_platform_atomic_alignment);
    cl_log("  preferred_global_atomic_alignment: %u", dev->preferred_global_atomic_alignment);
    cl_log("  preferred_local_atomic_alignment: %u", dev->preferred_local_atomic_alignment);
    cl_log("  max_num_sub_groups: %u", dev->max_num_sub_groups);
    cl_log("  sub_group_independent_forward_progress: %d",
           dev->sub_group_independent_forward_progress);
    cl_log(
        "  atomic_memory_capabilities: %s",
        cl_device_atomic_capabilities_to_str(dev->atomic_memory_capabilities, str, sizeof(str)));
    cl_log(
        "  atomic_fence_capabilities: %s",
        cl_device_atomic_capabilities_to_str(dev->atomic_fence_capabilities, str, sizeof(str)));
    cl_log("  non_uniform_work_group_support: %d", dev->non_uniform_work_group_support);
    cl_log("  work_group_collective_functions_support: %d",
           dev->work_group_collective_functions_support);
    cl_log("  generic_address_space_support: %d", dev->generic_address_space_support);
    cl_log("  device_enqueue_capabilities: 0x%lx", dev->device_enqueue_capabilities);
    cl_log("  pipe_support: %d", dev->pipe_support);
    cl_log("  preferred_work_group_size_multiple: %zu", dev->preferred_work_group_size_multiple);
    cl_log("  latest_conformance_version_passed: %s", dev->latest_conformance_version_passed);
}

static void
info_dump_platform(struct cl *cl, uint32_t idx)
{
    const struct cl_platform *plat = &cl->platforms[idx];

    cl_log("platform #%d:", idx);

    cl_log("  profile: %s", plat->profile);
    cl_log("  version: %d.%d.%d (%s)", CL_VERSION_MAJOR(plat->version),
           CL_VERSION_MINOR(plat->version), CL_VERSION_PATCH(plat->version), plat->version_str);
    cl_log("  name: %s", plat->name);
    cl_log("  vendor: %s", plat->vendor);

    cl_log("  extensions:");
    for (uint32_t i = 0; i < plat->extension_count; i++) {
        const cl_name_version *ext = &plat->extensions[i];
        cl_log("    %s: %d.%d.%d", ext->name, CL_VERSION_MAJOR(ext->version),
               CL_VERSION_MINOR(ext->version), CL_VERSION_PATCH(ext->version));
    }

    cl_log("  host timer resolution: %lu", plat->host_timer_resolution);
}

int
main(void)
{
    struct cl cl;
    cl_init(&cl, NULL);

    for (uint32_t i = 0; i < cl.platform_count; i++) {
        const struct cl_platform *plat = &cl.platforms[i];

        info_dump_platform(&cl, i);
        for (uint32_t j = 0; j < plat->device_count; j++)
            info_dump_device(&cl, i, j);
    }

    cl_cleanup(&cl);

    return 0;
}
