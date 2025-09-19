/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

struct dma_heap_test {
    VkDeviceSize size;
    VkExternalMemoryHandleTypeFlagBits handle_type;
    char *heap_path;

    struct vk vk;
    VkBuffer buf;
    VkMemoryRequirements buf_reqs;
    int buf_fd;
    void *buf_ptr;
    VkDeviceMemory mem;
};

static void
dma_heap_test_init_memory(struct dma_heap_test *test)
{
    struct vk *vk = &test->vk;

    VkMemoryFdPropertiesKHR fd_props = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
    };
    vk->result =
        vk->GetMemoryFdPropertiesKHR(vk->dev, test->handle_type, test->buf_fd, &fd_props);
    vk_check(vk, "invalid dma-buf");

    const uint32_t mt_mask = test->buf_reqs.memoryTypeBits & fd_props.memoryTypeBits;
    if (!mt_mask)
        vk_die("no valid mt");

    const int buf_fd = dup(test->buf_fd);
    if (buf_fd < 0)
        vk_die("failed to dup dma-buf");

    const VkImportMemoryFdInfoKHR import_info = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        .handleType = test->handle_type,
        .fd = buf_fd,
    };
    const VkMemoryDedicatedAllocateInfo dedicated_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .pNext = &import_info,
        .buffer = test->buf,
    };
    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &dedicated_info,
        .allocationSize = test->buf_reqs.size,
        .memoryTypeIndex = ffs(mt_mask) - 1,
    };
    vk->result = vk->AllocateMemory(vk->dev, &alloc_info, NULL, &test->mem);
    vk_check(vk, "failed to import dma-buf");

    vk->result = vk->BindBufferMemory(vk->dev, test->buf, test->mem, 0);
    vk_check(vk, "failed to bind buffer memory");
}

static void
dma_heap_test_init_dma_buf(struct dma_heap_test *test)
{
    int heap_fd;

    heap_fd = open(test->heap_path, O_RDONLY | O_CLOEXEC);
    if (heap_fd < 0)
        vk_die("failed to open %s", test->heap_path);

    struct dma_heap_allocation_data args = {
        .len = test->buf_reqs.size,
        .fd_flags = O_RDWR | O_CLOEXEC,
    };
    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &args))
        vk_die("failed to alloc dma-buf");

    close(heap_fd);

    test->buf_fd = args.fd;

    test->buf_ptr = mmap(NULL, test->buf_reqs.size, PROT_READ, MAP_SHARED, test->buf_fd, 0);
    if (test->buf_ptr == MAP_FAILED)
        vk_die("failed to mmap dma-buf");
}

static void
dma_heap_test_init_buffer(struct dma_heap_test *test)
{
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    struct vk *vk = &test->vk;

    const VkPhysicalDeviceExternalBufferInfo external_info = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,
        .usage = usage,
        .handleType = test->handle_type,
    };
    VkExternalBufferProperties external_props = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES,
    };
    vk->GetPhysicalDeviceExternalBufferProperties(vk->physical_dev, &external_info,
                                                  &external_props);
    if (!(external_props.externalMemoryProperties.externalMemoryFeatures &
          VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
        vk_die("no import support");

    const VkExternalMemoryBufferCreateInfo external_create_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
        .handleTypes = test->handle_type,
    };
    const VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = &external_create_info,
        .size = test->size,
        .usage = usage,
    };
    vk->result = vk->CreateBuffer(vk->dev, &create_info, NULL, &test->buf);
    vk_check(vk, "failed to create buffer");

    vk->GetBufferMemoryRequirements(vk->dev, test->buf, &test->buf_reqs);
}

static void
dma_heap_test_init(struct dma_heap_test *test)
{
    struct vk *vk = &test->vk;

    const char *const dev_exts[] = {
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
    };
    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_3,
        .dev_exts = dev_exts,
        .dev_ext_count = ARRAY_SIZE(dev_exts),
    };

    vk_init(vk, &params);
    dma_heap_test_init_buffer(test);
    dma_heap_test_init_dma_buf(test);
    dma_heap_test_init_memory(test);
}

static void
dma_heap_test_cleanup(struct dma_heap_test *test)
{
    struct vk *vk = &test->vk;

    vk->FreeMemory(vk->dev, test->mem, NULL);
    vk->DestroyBuffer(vk->dev, test->buf, NULL);

    munmap(test->buf_ptr, test->buf_reqs.size);
    close(test->buf_fd);

    vk_cleanup(vk);
}

static void
dma_heap_test_draw(struct dma_heap_test *test)
{
    struct vk *vk = &test->vk;

    const VkBufferMemoryBarrier barriers[] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_NONE,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
            .dstQueueFamilyIndex = vk->queue_family_index,
            .buffer = test->buf,
            .size = VK_WHOLE_SIZE,
        },
        [1] = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_NONE,
            .srcQueueFamilyIndex = vk->queue_family_index,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT,
            .buffer = test->buf,
            .size = VK_WHOLE_SIZE,
        },
    };

    for (uint32_t val = 0; val < 10; val++) {
        VkCommandBuffer cmd = vk_begin_cmd(vk, false);
        vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_NONE, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                               NULL, 1, &barriers[0], 0, NULL);
        vk->CmdFillBuffer(cmd, test->buf, 0, VK_WHOLE_SIZE, val);
        vk->CmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_NONE, 0, 0,
                               NULL, 1, &barriers[1], 0, NULL);
        vk_end_cmd(vk);
        vk_wait(vk);

        struct dma_buf_sync args = {
            .flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ,
        };
        if (ioctl(test->buf_fd, DMA_BUF_IOCTL_SYNC, &args))
            vk_die("failed to start cpu access");

        for (VkDeviceSize i = 0; i < test->size / sizeof(uint32_t); i++) {
            const uint32_t real = ((const uint32_t *)test->buf_ptr)[i];
            if (real != val)
                vk_die("index %d is 0x%x, not 0x%x", (int)i, real, val);
        }

        args.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
        if (ioctl(test->buf_fd, DMA_BUF_IOCTL_SYNC, &args))
            vk_die("failed to end cpu access");
    }
}

int
main(void)
{
    struct dma_heap_test test = {
        .size = 64,
        .handle_type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
        .heap_path = "/dev/dma_heap/system",

        .buf_fd = -1,
    };

    dma_heap_test_init(&test);
    dma_heap_test_draw(&test);
    dma_heap_test_cleanup(&test);

    return 0;
}
