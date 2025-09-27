/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "dmautil.h"
#include "vkutil.h"

struct residency_test {
    size_t size;

    uint32_t page_size;
    struct vk vk;
};

struct proc_vmstat {
    unsigned long nr_free_pages;
};

struct proc_statm {
    unsigned long size;
    unsigned long resident;
    unsigned long shared;
    unsigned long text;
    unsigned long data;
};

static void
residency_test_read_vmstat(struct residency_test *test, struct proc_vmstat *vmstat)
{
    const char path[] = "/proc/vmstat";
    char buf[256];

    const int fd = open(path, O_RDONLY);
    if (fd < 0)
        vk_die("failed to open %s", path);

    const ssize_t size = read(fd, buf, sizeof(buf));
    if (size < -1)
        vk_die("failed to read %s", path);

    close(fd);

    if (sscanf(buf, "nr_free_pages %lu\n", &vmstat->nr_free_pages) != 1)
        vk_die("failed to parse %s", path);
}

static void
residency_test_read_statm(struct residency_test *test, struct proc_statm *statm)
{
    const char path[] = "/proc/self/statm";
    char buf[256];

    const int fd = open(path, O_RDONLY);
    if (fd < 0)
        vk_die("failed to open %s", path);

    const ssize_t size = read(fd, buf, sizeof(buf));
    if (size < -1 || size == 256)
        vk_die("failed to read %s", path);

    close(fd);

    if (sscanf(buf, "%lu %lu %lu %lu 0 %lu 0\n", &statm->size, &statm->resident, &statm->shared,
               &statm->text, &statm->data) != 5)
        vk_die("failed to parse %s", path);
}

static void
residency_test_log_statm(struct residency_test *test, const char *reason)
{
    struct proc_vmstat vmstat;
    residency_test_read_vmstat(test, &vmstat);

    struct proc_statm statm;
    residency_test_read_statm(test, &statm);

    vk_log("%s: free %lu MiB, size %lu MiB, resident %lu MiB, shared %lu MiB", reason,
           vmstat.nr_free_pages * test->page_size / 1024 / 1024,
           statm.size * test->page_size / 1024 / 1024,
           statm.resident * test->page_size / 1024 / 1024,
           statm.shared * test->page_size / 1024 / 1024);
}

static void
residency_test_init(struct residency_test *test)
{
    struct vk *vk = &test->vk;

    test->page_size = sysconf(_SC_PAGESIZE);

    vk_init(vk, NULL);
}

static void
residency_test_cleanup(struct residency_test *test)
{
    struct vk *vk = &test->vk;

    vk_cleanup(vk);
}

static void
residency_test_run_vulkan(struct residency_test *test, uint32_t mt)
{
    struct vk *vk = &test->vk;

    const VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = test->size,
        .memoryTypeIndex = mt,
    };

    residency_test_log_statm(test, "  before alloc");

    VkDeviceMemory mem;
    if (vk->AllocateMemory(vk->dev, &alloc_info, NULL, &mem) != VK_SUCCESS) {
        vk_log("  failed to allocate for mt %d", mt);
        return;
    }
    residency_test_log_statm(test, "  after alloc");

    const VkMemoryPropertyFlags mt_flags = vk->mem_props.memoryTypes[mt].propertyFlags;
    if (mt_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        void *ptr;
        vk->result = vk->MapMemory(vk->dev, mem, 0, VK_WHOLE_SIZE, 0, &ptr);
        vk_check(vk, "failed to map memory");
        residency_test_log_statm(test, "  after map");

        memset(ptr, 0x77, test->size);
        residency_test_log_statm(test, "  after memset");

        if (!((uintptr_t)ptr & (test->page_size - 1))) {
            if (!madvise(ptr, test->size, MADV_PAGEOUT))
                residency_test_log_statm(test, "  after MADV_PAGEOUT");
        }
    }

    vk->FreeMemory(vk->dev, mem, NULL);
    residency_test_log_statm(test, "  after free");
}

static void
residency_test_run_dma_heap(struct residency_test *test)
{
    struct dma_heap heap;

    dma_heap_init(&heap, "system");

    residency_test_log_statm(test, "  before alloc");
    struct dma_buf *buf = dma_heap_alloc(&heap, test->size);
    residency_test_log_statm(test, "  after alloc");

    dma_heap_cleanup(&heap);

    dma_buf_map(buf);
    residency_test_log_statm(test, "  after map");

    memset(buf->map, 0x77, test->size);
    residency_test_log_statm(test, "  after memset");

    if (!madvise(buf->map, test->size, MADV_PAGEOUT))
        residency_test_log_statm(test, "  after MADV_PAGEOUT");

    dma_buf_unmap(buf);
    dma_buf_destroy(buf);
    residency_test_log_statm(test, "  after free");
}

static void
residency_test_run_malloc(struct residency_test *test)
{
    residency_test_log_statm(test, "  before alloc");

    void *ptr = aligned_alloc(test->page_size, test->size);
    if (!ptr)
        vk_die("failed to malloc");
    residency_test_log_statm(test, "  after alloc");

    memset(ptr, 0x77, test->size);
    residency_test_log_statm(test, "  after memset");

    if (!madvise(ptr, test->size, MADV_PAGEOUT))
        residency_test_log_statm(test, "  after MADV_PAGEOUT");

    free(ptr);
    residency_test_log_statm(test, "  after free");
}

static void
residency_test_run(struct residency_test *test)
{
    struct vk *vk = &test->vk;

    vk_log("alloc size %zu MiB", test->size / 1024 / 1024);

    vk_log("malloc:");
    residency_test_run_malloc(test);

    vk_log("system dma-heap:");
    residency_test_run_dma_heap(test);

    for (uint32_t i = 0; i < vk->mem_props.memoryTypeCount; i++) {
        vk_log("vulkan mt %d:", i);
        residency_test_run_vulkan(test, i);
    }
}

int
main(void)
{
    struct residency_test test = {
        .size = 4ull * 1024 * 1024 * 1024,
    };

    residency_test_init(&test);
    residency_test_run(&test);
    residency_test_cleanup(&test);

    return 0;
}
