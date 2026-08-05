/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef PFN_ALL
#define PFN_ALL(name)
#endif

#ifndef PFN_GIPA
#define PFN_GIPA(name) PFN_ALL(name)
#endif

#ifndef PFN_GLOBAL
#define PFN_GLOBAL(name) PFN_ALL(name)
#endif

#ifndef PFN_INSTANCE
#define PFN_INSTANCE(name) PFN_ALL(name)
#endif

#ifndef PFN_DEVICE
#define PFN_DEVICE(name) PFN_ALL(name)
#endif

PFN_GIPA(GetInstanceProcAddr)

PFN_GLOBAL(CreateInstance)
PFN_GLOBAL(EnumerateInstanceExtensionProperties)
PFN_GLOBAL(EnumerateInstanceLayerProperties)
PFN_GLOBAL(EnumerateInstanceVersion)

PFN_INSTANCE(CreateDevice)
PFN_INSTANCE(DestroyInstance)
PFN_INSTANCE(EnumerateDeviceExtensionProperties)
PFN_INSTANCE(EnumeratePhysicalDeviceGroups)
PFN_INSTANCE(EnumeratePhysicalDevices)
PFN_INSTANCE(GetPhysicalDeviceExternalBufferProperties)
PFN_INSTANCE(GetPhysicalDeviceExternalFenceProperties)
PFN_INSTANCE(GetPhysicalDeviceExternalSemaphoreProperties)
PFN_INSTANCE(GetPhysicalDeviceFeatures2)
PFN_INSTANCE(GetPhysicalDeviceFormatProperties2)
PFN_INSTANCE(GetPhysicalDeviceImageFormatProperties2)
PFN_INSTANCE(GetPhysicalDeviceMemoryProperties2)
PFN_INSTANCE(GetPhysicalDeviceProperties2)
PFN_INSTANCE(GetPhysicalDeviceQueueFamilyProperties2)
PFN_INSTANCE(GetPhysicalDeviceSparseImageFormatProperties2)

PFN_DEVICE(AllocateCommandBuffers)
PFN_DEVICE(AllocateDescriptorSets)
PFN_DEVICE(AllocateMemory)
PFN_DEVICE(BeginCommandBuffer)
PFN_DEVICE(CmdBeginQuery)
PFN_DEVICE(CmdBindPipeline)
PFN_DEVICE(CmdClearAttachments)
PFN_DEVICE(CmdClearColorImage)
PFN_DEVICE(CmdClearDepthStencilImage)
PFN_DEVICE(CmdCopyQueryPoolResults)
PFN_DEVICE(CmdDispatch)
PFN_DEVICE(CmdDispatchIndirect)
PFN_DEVICE(CmdDraw)
PFN_DEVICE(CmdDrawIndexed)
PFN_DEVICE(CmdDrawIndexedIndirect)
PFN_DEVICE(CmdDrawIndirect)
PFN_DEVICE(CmdEndQuery)
PFN_DEVICE(CmdExecuteCommands)
PFN_DEVICE(CmdFillBuffer)
PFN_DEVICE(CmdResetQueryPool)
PFN_DEVICE(CmdSetBlendConstants)
PFN_DEVICE(CmdSetDepthBias)
PFN_DEVICE(CmdSetDepthBounds)
PFN_DEVICE(CmdSetLineWidth)
PFN_DEVICE(CmdSetStencilCompareMask)
PFN_DEVICE(CmdSetStencilReference)
PFN_DEVICE(CmdSetStencilWriteMask)
PFN_DEVICE(CmdUpdateBuffer)
PFN_DEVICE(CreateBuffer)
PFN_DEVICE(CreateBufferView)
PFN_DEVICE(CreateCommandPool)
PFN_DEVICE(CreateComputePipelines)
PFN_DEVICE(CreateDescriptorPool)
PFN_DEVICE(CreateDescriptorSetLayout)
PFN_DEVICE(CreateEvent)
PFN_DEVICE(CreateFence)
PFN_DEVICE(CreateGraphicsPipelines)
PFN_DEVICE(CreateImage)
PFN_DEVICE(CreateImageView)
PFN_DEVICE(CreatePipelineCache)
PFN_DEVICE(CreatePipelineLayout)
PFN_DEVICE(CreateQueryPool)
PFN_DEVICE(CreateSampler)
PFN_DEVICE(CreateSemaphore)
PFN_DEVICE(DestroyBuffer)
PFN_DEVICE(DestroyBufferView)
PFN_DEVICE(DestroyCommandPool)
PFN_DEVICE(DestroyDescriptorPool)
PFN_DEVICE(DestroyDescriptorSetLayout)
PFN_DEVICE(DestroyDevice)
PFN_DEVICE(DestroyEvent)
PFN_DEVICE(DestroyFence)
PFN_DEVICE(DestroyImage)
PFN_DEVICE(DestroyImageView)
PFN_DEVICE(DestroyPipeline)
PFN_DEVICE(DestroyPipelineCache)
PFN_DEVICE(DestroyPipelineLayout)
PFN_DEVICE(DestroyQueryPool)
PFN_DEVICE(DestroySampler)
PFN_DEVICE(DestroySemaphore)
PFN_DEVICE(DestroyShaderModule)
PFN_DEVICE(DeviceWaitIdle)
PFN_DEVICE(EndCommandBuffer)
PFN_DEVICE(FlushMappedMemoryRanges)
PFN_DEVICE(FreeCommandBuffers)
PFN_DEVICE(FreeDescriptorSets)
PFN_DEVICE(FreeMemory)
PFN_DEVICE(GetDeviceMemoryCommitment)
PFN_DEVICE(GetDeviceProcAddr)
PFN_DEVICE(GetEventStatus)
PFN_DEVICE(GetFenceStatus)
PFN_DEVICE(GetPipelineCacheData)
PFN_DEVICE(GetQueryPoolResults)
PFN_DEVICE(InvalidateMappedMemoryRanges)
PFN_DEVICE(MergePipelineCaches)
PFN_DEVICE(QueueBindSparse)
PFN_DEVICE(QueueWaitIdle)
PFN_DEVICE(ResetCommandBuffer)
PFN_DEVICE(ResetCommandPool)
PFN_DEVICE(ResetDescriptorPool)
PFN_DEVICE(ResetEvent)
PFN_DEVICE(ResetFences)
PFN_DEVICE(SetEvent)
PFN_DEVICE(UpdateDescriptorSets)
PFN_DEVICE(WaitForFences)

/* Vulkan 1.1 */
PFN_DEVICE(BindBufferMemory2)
PFN_DEVICE(BindImageMemory2)
PFN_DEVICE(CmdDispatchBase)
PFN_DEVICE(CmdSetDeviceMask)
PFN_DEVICE(CreateDescriptorUpdateTemplate)
PFN_DEVICE(CreateSamplerYcbcrConversion)
PFN_DEVICE(DestroyDescriptorUpdateTemplate)
PFN_DEVICE(DestroySamplerYcbcrConversion)
PFN_DEVICE(GetBufferMemoryRequirements2)
PFN_DEVICE(GetDescriptorSetLayoutSupport)
PFN_DEVICE(GetDeviceGroupPeerMemoryFeatures)
PFN_DEVICE(GetDeviceQueue2)
PFN_DEVICE(GetImageMemoryRequirements2)
PFN_DEVICE(GetImageSparseMemoryRequirements2)
PFN_DEVICE(TrimCommandPool)
PFN_DEVICE(UpdateDescriptorSetWithTemplate)

/* Vulkan 1.2 */
PFN_DEVICE(CmdDrawIndexedIndirectCount)
PFN_DEVICE(CmdDrawIndirectCount)
PFN_DEVICE(GetBufferDeviceAddress)
PFN_DEVICE(GetBufferOpaqueCaptureAddress)
PFN_DEVICE(GetDeviceMemoryOpaqueCaptureAddress)
PFN_DEVICE(GetSemaphoreCounterValue)
PFN_DEVICE(ResetQueryPool)
PFN_DEVICE(SignalSemaphore)
PFN_DEVICE(WaitSemaphores)

/* Vulkan 1.3 */
PFN_INSTANCE(GetPhysicalDeviceToolProperties)
PFN_DEVICE(CmdBeginRendering)
PFN_DEVICE(CmdBindVertexBuffers2)
PFN_DEVICE(CmdBlitImage2)
PFN_DEVICE(CmdCopyBuffer2)
PFN_DEVICE(CmdCopyBufferToImage2)
PFN_DEVICE(CmdCopyImage2)
PFN_DEVICE(CmdCopyImageToBuffer2)
PFN_DEVICE(CmdEndRendering)
PFN_DEVICE(CmdPipelineBarrier2)
PFN_DEVICE(CmdResetEvent2)
PFN_DEVICE(CmdResolveImage2)
PFN_DEVICE(CmdSetCullMode)
PFN_DEVICE(CmdSetDepthBiasEnable)
PFN_DEVICE(CmdSetDepthBoundsTestEnable)
PFN_DEVICE(CmdSetDepthCompareOp)
PFN_DEVICE(CmdSetDepthTestEnable)
PFN_DEVICE(CmdSetDepthWriteEnable)
PFN_DEVICE(CmdSetEvent2)
PFN_DEVICE(CmdSetFrontFace)
PFN_DEVICE(CmdSetPrimitiveRestartEnable)
PFN_DEVICE(CmdSetPrimitiveTopology)
PFN_DEVICE(CmdSetRasterizerDiscardEnable)
PFN_DEVICE(CmdSetScissorWithCount)
PFN_DEVICE(CmdSetStencilOp)
PFN_DEVICE(CmdSetStencilTestEnable)
PFN_DEVICE(CmdSetViewportWithCount)
PFN_DEVICE(CmdWaitEvents2)
PFN_DEVICE(CmdWriteTimestamp2)
PFN_DEVICE(CreatePrivateDataSlot)
PFN_DEVICE(DestroyPrivateDataSlot)
PFN_DEVICE(GetDeviceBufferMemoryRequirements)
PFN_DEVICE(GetDeviceImageMemoryRequirements)
PFN_DEVICE(GetDeviceImageSparseMemoryRequirements)
PFN_DEVICE(GetPrivateData)
PFN_DEVICE(QueueSubmit2)
PFN_DEVICE(SetPrivateData)

/* Vulkan 1.4 */
PFN_DEVICE(CmdBindDescriptorSets2)
PFN_DEVICE(CmdBindIndexBuffer2)
PFN_DEVICE(CmdPushConstants2)
PFN_DEVICE(CmdPushDescriptorSet2)
PFN_DEVICE(CmdPushDescriptorSetWithTemplate2)
PFN_DEVICE(CmdSetLineStipple)
PFN_DEVICE(CmdSetRenderingAttachmentLocations)
PFN_DEVICE(CmdSetRenderingInputAttachmentIndices)
PFN_DEVICE(CopyImageToImage)
PFN_DEVICE(CopyImageToMemory)
PFN_DEVICE(CopyMemoryToImage)
PFN_DEVICE(GetDeviceImageSubresourceLayout)
PFN_DEVICE(GetImageSubresourceLayout2)
PFN_DEVICE(GetRenderingAreaGranularity)
PFN_DEVICE(MapMemory2)
PFN_DEVICE(TransitionImageLayout)
PFN_DEVICE(UnmapMemory2)

/* VK_KHR_calibrated_timestamps */
PFN_INSTANCE(GetPhysicalDeviceCalibrateableTimeDomainsKHR)
PFN_DEVICE(GetCalibratedTimestampsKHR)

/* VK_KHR_display */
PFN_INSTANCE(CreateDisplayModeKHR)
PFN_INSTANCE(CreateDisplayPlaneSurfaceKHR)
PFN_INSTANCE(GetDisplayModePropertiesKHR)
PFN_INSTANCE(GetDisplayPlaneCapabilitiesKHR)
PFN_INSTANCE(GetDisplayPlaneSupportedDisplaysKHR)
PFN_INSTANCE(GetPhysicalDeviceDisplayPlanePropertiesKHR)
PFN_INSTANCE(GetPhysicalDeviceDisplayPropertiesKHR)

/* VK_KHR_external_memory_fd */
PFN_DEVICE(GetMemoryFdKHR)
PFN_DEVICE(GetMemoryFdPropertiesKHR)

/* VK_KHR_external_semaphore_fd */
PFN_DEVICE(GetSemaphoreFdKHR)
PFN_DEVICE(ImportSemaphoreFdKHR)

/* VK_KHR_get_surface_capabilities2 */
PFN_INSTANCE(GetPhysicalDeviceSurfaceCapabilities2KHR)
PFN_INSTANCE(GetPhysicalDeviceSurfaceFormats2KHR)

/* VK_KHR_present_wait2 */
PFN_DEVICE(WaitForPresent2KHR)

/* VK_KHR_shared_presentable_image */
PFN_DEVICE(GetSwapchainStatusKHR)

/* VK_KHR_surface */
PFN_INSTANCE(DestroySurfaceKHR)
PFN_INSTANCE(GetPhysicalDeviceSurfacePresentModesKHR)
PFN_INSTANCE(GetPhysicalDeviceSurfaceSupportKHR)

/* VK_KHR_swapchain */
PFN_INSTANCE(GetPhysicalDevicePresentRectanglesKHR)
PFN_DEVICE(AcquireNextImage2KHR)
PFN_DEVICE(CreateSwapchainKHR)
PFN_DEVICE(DestroySwapchainKHR)
PFN_DEVICE(GetDeviceGroupPresentCapabilitiesKHR)
PFN_DEVICE(GetDeviceGroupSurfacePresentModesKHR)
PFN_DEVICE(GetSwapchainImagesKHR)
PFN_DEVICE(QueuePresentKHR)

/* VK_KHR_swapchain_maintenance1 */
PFN_DEVICE(ReleaseSwapchainImagesKHR)

/* VK_EXT_debug_utils */
PFN_INSTANCE(CreateDebugUtilsMessengerEXT)
PFN_INSTANCE(DestroyDebugUtilsMessengerEXT)
PFN_INSTANCE(SubmitDebugUtilsMessageEXT)
PFN_DEVICE(CmdBeginDebugUtilsLabelEXT)
PFN_DEVICE(CmdEndDebugUtilsLabelEXT)
PFN_DEVICE(CmdInsertDebugUtilsLabelEXT)
PFN_DEVICE(QueueBeginDebugUtilsLabelEXT)
PFN_DEVICE(QueueEndDebugUtilsLabelEXT)
PFN_DEVICE(QueueInsertDebugUtilsLabelEXT)
PFN_DEVICE(SetDebugUtilsObjectNameEXT)
PFN_DEVICE(SetDebugUtilsObjectTagEXT)

/* VK_EXT_hdr_metadata */
PFN_DEVICE(SetHdrMetadataEXT)

/* VK_EXT_present_timing */
PFN_DEVICE(GetPastPresentationTimingEXT)
PFN_DEVICE(GetSwapchainTimeDomainPropertiesEXT)
PFN_DEVICE(GetSwapchainTimingPropertiesEXT)
PFN_DEVICE(SetSwapchainPresentTimingQueueSizeEXT)

/* VK_ANDROID_external_memory_android_hardware_buffer */
PFN_DEVICE(GetAndroidHardwareBufferPropertiesANDROID)
PFN_DEVICE(GetMemoryAndroidHardwareBufferANDROID)

#undef PFN_ALL
#undef PFN_GPIA
#undef PFN_GLOBAL
#undef PFN_INSTANCE
#undef PFN_DEVICE
