/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkutil.h"

struct timestamp_test {
    uint32_t sleep;
    bool EXT_calibrated_timestamps;
    bool loop;

    struct vk vk;
    struct vk_event *event;
    struct vk_query *query;
};

static void
timestamp_test_init(struct timestamp_test *test)
{
    struct vk *vk = &test->vk;

    const struct vk_init_params params = {
        .api_version = VK_API_VERSION_1_2,
        .dev_exts = (const char *[]){ VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME },
        .dev_ext_count = test->EXT_calibrated_timestamps,
    };
    vk_init(vk, &params);

    if (test->EXT_calibrated_timestamps) {
        VkTimeDomainEXT domains[16];
        uint32_t count = ARRAY_SIZE(domains);
        vk->result =
            vk->GetPhysicalDeviceCalibrateableTimeDomainsEXT(vk->physical_dev, &count, domains);
        vk_check(vk, "failed to get time domains");

        bool has_device_domain = false;
        for (uint32_t i = 0; i < count; i++) {
            if (domains[i] == VK_TIME_DOMAIN_DEVICE_EXT) {
                has_device_domain = true;
                break;
            }
        }
        if (!has_device_domain)
            vk_die("no device time domain");
    }

    test->event = vk_create_event(vk);
    test->query = vk_create_query(vk, VK_QUERY_TYPE_TIMESTAMP, 2);
}

static void
timestamp_test_cleanup(struct timestamp_test *test)
{
    struct vk *vk = &test->vk;

    vk_destroy_event(vk, test->event);
    vk_destroy_query(vk, test->query);
    vk_cleanup(vk);
}

static void
timestamp_test_dump_delta(struct timestamp_test *test, const char *name, const uint64_t ts[2])
{
    struct vk *vk = &test->vk;

    const uint64_t delta = ts[1] - ts[0];
    const uint64_t delta_ns = delta * (uint64_t)vk->props.properties.limits.timestampPeriod;
    vk_log("%s: ts = (%" PRIu64 ", %" PRIu64 "), period = %f, ms = %d", name, ts[0], ts[1],
           vk->props.properties.limits.timestampPeriod, (int)(delta_ns / 1000000));
}

static void
timestamp_test_get_query_result(struct timestamp_test *test, uint64_t *ts, uint32_t count)
{
    struct vk *vk = &test->vk;

    vk->result =
        vk->GetQueryPoolResults(vk->dev, test->query->pool, 0, count, sizeof(ts[0]) * count, ts,
                                sizeof(ts[0]), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    vk_check(vk, "failed to get query results");
}

static void
timestamp_test_draw_same_cmd(struct timestamp_test *test)
{
    struct vk *vk = &test->vk;

    vk->ResetQueryPool(vk->dev, test->query->pool, 0, 2);
    vk->ResetEvent(vk->dev, test->event->event);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);

    vk->CmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, test->query->pool, 0);
    vk->CmdWaitEvents(cmd, 1, &test->event->event, VK_PIPELINE_STAGE_HOST_BIT,
                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, NULL, 0, NULL, 0, NULL);
    vk->CmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, test->query->pool, 1);

    vk_end_cmd(vk);

    vk_sleep(test->sleep);
    vk->SetEvent(vk->dev, test->event->event);

    vk_wait(vk);

    uint64_t ts[2];
    timestamp_test_get_query_result(test, ts, 2);
    timestamp_test_dump_delta(test, __func__, ts);
}

static void
timestamp_test_draw_two_cmds(struct timestamp_test *test)
{
    struct vk *vk = &test->vk;

    vk->ResetQueryPool(vk->dev, test->query->pool, 0, 2);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, test->query->pool, 0);
    vk_end_cmd(vk);
    vk_wait(vk);

    vk_sleep(test->sleep);

    cmd = vk_begin_cmd(vk, false);
    vk->CmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, test->query->pool, 1);
    vk_end_cmd(vk);
    vk_wait(vk);

    uint64_t ts[2];
    timestamp_test_get_query_result(test, ts, 2);
    timestamp_test_dump_delta(test, __func__, ts);
}

static void
timestamp_test_draw_calibrated(struct timestamp_test *test)
{
    struct vk *vk = &test->vk;
    const VkCalibratedTimestampInfoEXT info = {
        .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT,
        .timeDomain = VK_TIME_DOMAIN_DEVICE_EXT,
    };
    uint64_t ts[2];
    uint64_t deviation;

    vk->result = vk->GetCalibratedTimestampsEXT(vk->dev, 1, &info, &ts[0], &deviation);
    vk_sleep(test->sleep);
    vk->result = vk->GetCalibratedTimestampsEXT(vk->dev, 1, &info, &ts[1], &deviation);

    timestamp_test_dump_delta(test, __func__, ts);
}

static void
timestamp_test_draw_mixed(struct timestamp_test *test)
{
    struct vk *vk = &test->vk;
    const VkCalibratedTimestampInfoEXT info = {
        .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT,
        .timeDomain = VK_TIME_DOMAIN_DEVICE_EXT,
    };
    uint64_t ts[3];
    uint64_t deviation;

    vk->ResetQueryPool(vk->dev, test->query->pool, 0, 2);

    vk->result = vk->GetCalibratedTimestampsEXT(vk->dev, 1, &info, &ts[0], &deviation);

    vk_sleep(test->sleep / 2);

    VkCommandBuffer cmd = vk_begin_cmd(vk, false);
    vk->CmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, test->query->pool, 0);
    vk_end_cmd(vk);
    vk_wait(vk);
    timestamp_test_get_query_result(test, &ts[1], 1);

    vk_sleep(test->sleep / 2);

    vk->result = vk->GetCalibratedTimestampsEXT(vk->dev, 1, &info, &ts[2], &deviation);

    timestamp_test_dump_delta(test, __func__, &ts[0]);
    timestamp_test_dump_delta(test, __func__, &ts[1]);
}

static void
timestamp_test_draw_loop(struct timestamp_test *test)
{
    struct vk *vk = &test->vk;
    const VkCalibratedTimestampInfoEXT info = {
        .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT,
        .timeDomain = VK_TIME_DOMAIN_DEVICE_EXT,
    };
    uint64_t ts;
    uint64_t deviation;

    while (true) {
        vk->result = vk->GetCalibratedTimestampsEXT(vk->dev, 1, &info, &ts, &deviation);
        const uint64_t ns = (uint64_t)(ts * vk->props.properties.limits.timestampPeriod);
        const uint64_t ms = ns / 1000000;
        vk_log("%" PRIu64 ".%03" PRIu64, ms / 1000, ms % 1000);
        vk_sleep(1000);
    }
}

static void
timestamp_test_draw(struct timestamp_test *test)
{
    timestamp_test_draw_same_cmd(test);
    timestamp_test_draw_two_cmds(test);

    if (test->EXT_calibrated_timestamps) {
        timestamp_test_draw_calibrated(test);
        timestamp_test_draw_mixed(test);
        if (test->loop)
            timestamp_test_draw_loop(test);
    }
}

int
main(void)
{
    struct timestamp_test test = {
        .sleep = 200,
        .EXT_calibrated_timestamps = true,
        .loop = false,
    };

    timestamp_test_init(&test);
    timestamp_test_draw(&test);
    timestamp_test_cleanup(&test);

    return 0;
}
