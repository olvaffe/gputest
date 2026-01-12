/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct protected_test {
    VkDeviceSize buf_size;

    struct vk vk;

    struct vk_buffer *src_buf;
    struct vk_buffer *dst_buf;
};

static void
protected_test_init_buffer(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    uint32_t protected_mt = VK_MAX_MEMORY_TYPES;
    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        const VkMemoryType *mt = &vk->mem_props.memoryTypes[i];
        if (mt->propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) {
            protected_mt = i;
            break;
        }
    }
    if (protected_mt == VK_MAX_MEMORY_TYPES)
        vk_die("no protected mt");

    test->src_buf = vk_create_buffer(vk, 0, test->buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memset(test->src_buf->mem_ptr, 0x80, test->buf_size);

    test->dst_buf = vk_create_buffer_with_mt(vk, VK_BUFFER_CREATE_PROTECTED_BIT, test->buf_size,
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT, protected_mt);
}

static void
protected_test_init(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    const struct vk_init_params params = {
        .protected_memory = true,
    };
    vk_init(vk, &params);

    protected_test_init_buffer(test);
}

static void
protected_test_cleanup(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_buffer(vk, test->src_buf);
    vk_destroy_buffer(vk, test->dst_buf);

    vk_cleanup(vk);
}

static void
protected_test_draw(struct protected_test *test)
{
    struct vk *vk = &test->vk;

    VkCommandBuffer cmd = vk_begin_cmd(vk, true);

    const VkBufferCopy copy = {
        .size = test->buf_size,
    };
    vk->CmdCopyBuffer(cmd, test->src_buf->buf, test->dst_buf->buf, 1, &copy);

    vk_end_cmd(vk);
    vk_wait(vk);
}

int
main(void)
{
    struct protected_test test = {
        .buf_size = 32 * 1024,
    };

    protected_test_init(&test);
    protected_test_draw(&test);
    protected_test_cleanup(&test);

    return 0;
}
