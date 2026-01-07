#include <android/choreographer.h>
// #include <android/hardware_buffer.h>
#include <android/input.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>
// #include <android/surface_control.h>
// #include <android/surface_control_input_receiver.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

enum android_test_looper_ident {
    MY_TEST_LOOPER_IDENT_INPUT,
};

struct android_test_state {
    AInputQueue *queue;
    ANativeWindow *win;
};

struct android_test {
    bool verbose;
    enum AHardwareBuffer_Format format;
    ANativeActivity *act;

    mtx_t mutex;
    cnd_t cond;
    thrd_t thread;
    bool run;

    ALooper *looper;
    AChoreographer *choreo;
    struct android_test_state cur;
    struct android_test_state next;
};

static inline void
android_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    __android_log_vprint(ANDROID_LOG_INFO, "MY", format, ap);
    va_end(ap);
}

static inline void
android_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    __android_log_vprint(ANDROID_LOG_INFO, "MY", format, ap);
    va_end(ap);

    abort();
}

static void
android_test_handle_frame(int64_t ts, void *arg)
{
    struct android_test *test = arg;

    if (test->verbose)
        android_log("frame: %" PRIi64, ts);

    if (!test->cur.win)
        return;

    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(test->cur.win, &buf, NULL)) {
        android_log("failed to lock window");
        return;
    }

    const int cpp = buf.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM ? 4 : 1;
    for (int y = 0; y < buf.height; y++) {
        void *row = buf.bits + y * buf.stride * cpp;

        if (buf.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM) {
            for (int x = 0; x < buf.width; x++) {
                uint8_t *rgba = row + x * cpp;
                rgba[0] = 0xff;
                rgba[1] = 0x80;
                rgba[2] = 0x80;
                rgba[3] = 0xff;
            }
        } else {
            memset(row, 0xff, buf.width * cpp);
        }
    }

    ANativeWindow_unlockAndPost(test->cur.win);
}

static void
android_test_handle_input(struct android_test *test)
{
    AInputQueue *queue = test->cur.queue;
    AInputEvent *ev;

    while (AInputQueue_getEvent(queue, &ev) >= 0) {
        if (!AInputQueue_preDispatchEvent(queue, ev))
            AInputQueue_finishEvent(queue, ev, false);
    }
}

static void
android_test_handle_state(struct android_test *test)
{
    struct android_test_state *cur = &test->cur;
    struct android_test_state *next = &test->next;

    if (!memcmp(cur, next, sizeof(*cur)))
        return;

    if (cur->queue != next->queue) {
        if (cur->queue)
            AInputQueue_detachLooper(cur->queue);

        cur->queue = next->queue;

        if (cur->queue) {
            AInputQueue_attachLooper(cur->queue, test->looper, MY_TEST_LOOPER_IDENT_INPUT, NULL,
                                     NULL);
        }
    }

    if (cur->win != next->win) {
        cur->win = next->win;

        if (cur->win)
            AChoreographer_postFrameCallback64(test->choreo, android_test_handle_frame, test);
    }

    cnd_signal(&test->cond);
}

static int
android_test_thread(void *arg)
{
    struct android_test *test = arg;

    mtx_lock(&test->mutex);

    test->looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if (!test->looper)
        android_die("failed to prepare looper");

    test->choreo = AChoreographer_getInstance();
    if (!test->choreo)
        android_die("failed to get choreographer");

    /* signal readiness */
    cnd_signal(&test->cond);
    android_log("thread ready");

    while (test->run) {
        mtx_unlock(&test->mutex);
        const int ident = ALooper_pollOnce(-1, NULL, NULL, NULL);
        mtx_lock(&test->mutex);

        switch (ident) {
        case ALOOPER_POLL_ERROR:
            android_die("failed to poll");
            break;
        case MY_TEST_LOOPER_IDENT_INPUT:
            android_test_handle_input(test);
            break;
        default:
            break;
        }

        android_test_handle_state(test);
    }

    if (test->cur.queue)
        AInputQueue_detachLooper(test->cur.queue);
    test->looper = NULL;

    mtx_unlock(&test->mutex);

    return 0;
}

static void
android_test_init(struct android_test *test)
{
    if (mtx_init(&test->mutex, mtx_plain) != thrd_success)
        android_die("failed to init mutex");

    if (cnd_init(&test->cond) != thrd_success)
        android_die("failed to init cond");

    test->run = true;
    if (thrd_create(&test->thread, android_test_thread, test) != thrd_success)
        android_die("failed to create thread");

    /* wait for readiness */
    mtx_lock(&test->mutex);
    while (!test->looper)
        cnd_wait(&test->cond, &test->mutex);
    mtx_unlock(&test->mutex);

    android_log("main ready");
}

static void
android_test_cleanup(struct android_test *test)
{
    mtx_lock(&test->mutex);
    test->run = false;
    ALooper_wake(test->looper);
    mtx_unlock(&test->mutex);

    thrd_join(test->thread, NULL);

    cnd_destroy(&test->cond);
    mtx_destroy(&test->mutex);
}

static void
android_test_set_window(struct android_test *test, ANativeWindow *win)
{
    mtx_lock(&test->mutex);

    test->next.win = win;
    ALooper_wake(test->looper);

    if (!win) {
        while (test->cur.win != test->next.win)
            cnd_wait(&test->cond, &test->mutex);
    }

    mtx_unlock(&test->mutex);
}

static void
android_test_set_queue(struct android_test *test, AInputQueue *queue)
{
    mtx_lock(&test->mutex);

    test->next.queue = queue;
    ALooper_wake(test->looper);

    if (!queue) {
        while (test->cur.queue != test->next.queue)
            cnd_wait(&test->cond, &test->mutex);
    }

    mtx_unlock(&test->mutex);
}

static void
ANativeActivity_onStart(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onStart");
}

static void
ANativeActivity_onResume(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onResume");
}

static void *
ANativeActivity_onSaveInstanceState(ANativeActivity *act, size_t *outSize)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onSaveInstanceState");

    *outSize = 0;
    return NULL;
}

static void
ANativeActivity_onPause(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onPause");
}

static void
ANativeActivity_onStop(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onStop");
}

static void
ANativeActivity_onDestroy(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onDestroy");

    android_test_cleanup(test);

    free(test);
    act->instance = NULL;
}

static void
ANativeActivity_onWindowFocusChanged(ANativeActivity *act, int hasFocus)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onWindowFocusChanged: %d", hasFocus);
}

static void
ANativeActivity_onNativeWindowCreated(ANativeActivity *act, ANativeWindow *win)
{
    struct android_test *test = act->instance;

    if (ANativeWindow_setBuffersGeometry(win, 0, 0, test->format))
        android_die("failed to set window format");

    if (test->verbose) {
        android_log("onNativeWindowCreated: %p, %dx%d, format 0x%x", win,
                    ANativeWindow_getWidth(win), ANativeWindow_getHeight(win),
                    ANativeWindow_getFormat(win));
    }

    android_test_set_window(test, win);
}

static void
ANativeActivity_onNativeWindowResized(ANativeActivity *act, ANativeWindow *win)
{
    struct android_test *test = act->instance;

    if (test->verbose) {
        android_log("onNativeWindowResized: %p, %dx%d", win, ANativeWindow_getWidth(win),
                    ANativeWindow_getHeight(win));
    }
}

static void
ANativeActivity_onNativeWindowRedrawNeeded(ANativeActivity *act, ANativeWindow *win)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onNativeWindowRedrawNeeded: %p", win);
}

static void
ANativeActivity_onNativeWindowDestroyed(ANativeActivity *act, ANativeWindow *win)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onNativeWindowDestroyed: %p", win);

    android_test_set_window(test, NULL);
}

static void
ANativeActivity_onInputQueueCreated(ANativeActivity *act, AInputQueue *queue)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onInputQueueCreated: %p", queue);

    android_test_set_queue(test, queue);
}

static void
ANativeActivity_onInputQueueDestroyed(ANativeActivity *act, AInputQueue *queue)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onInputQueueDestroyed: %p", queue);

    android_test_set_queue(test, NULL);
}

static void
ANativeActivity_onContentRectChanged(ANativeActivity *act, const ARect *rect)
{
    struct android_test *test = act->instance;

    if (test->verbose) {
        android_log("onContentRectChanged: (%d, %d, %d, %d)", rect->left, rect->top, rect->right,
                    rect->bottom);
    }
}

static void
ANativeActivity_onConfigurationChanged(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onConfigurationChanged");
}

static void
ANativeActivity_onLowMemory(ANativeActivity *act)
{
    struct android_test *test = act->instance;

    if (test->verbose)
        android_log("onLowMemory");
}

void
ANativeActivity_onCreate(ANativeActivity *act, void *savedState, size_t savedStateSize)
{
    const struct android_test test_templ = {
        .verbose = true,
        .format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
        .act = act,
    };

    struct android_test *test = calloc(1, sizeof(*test));
    if (!test)
        android_die("failed to alloc test");
    *test = test_templ;

    if (test->verbose) {
        android_log("onCreate: sdk %d, internal %s, external %s", act->sdkVersion,
                    act->internalDataPath, act->externalDataPath);
    }

    ANativeActivityCallbacks *cbs = act->callbacks;
    cbs->onStart = ANativeActivity_onStart;
    cbs->onResume = ANativeActivity_onResume;
    cbs->onSaveInstanceState = ANativeActivity_onSaveInstanceState;
    cbs->onPause = ANativeActivity_onPause;
    cbs->onStop = ANativeActivity_onStop;
    cbs->onDestroy = ANativeActivity_onDestroy;
    cbs->onWindowFocusChanged = ANativeActivity_onWindowFocusChanged;
    cbs->onNativeWindowCreated = ANativeActivity_onNativeWindowCreated;
    cbs->onNativeWindowResized = ANativeActivity_onNativeWindowResized;
    cbs->onNativeWindowRedrawNeeded = ANativeActivity_onNativeWindowRedrawNeeded;
    cbs->onNativeWindowDestroyed = ANativeActivity_onNativeWindowDestroyed;
    cbs->onInputQueueCreated = ANativeActivity_onInputQueueCreated;
    cbs->onInputQueueDestroyed = ANativeActivity_onInputQueueDestroyed;
    cbs->onContentRectChanged = ANativeActivity_onContentRectChanged;
    cbs->onConfigurationChanged = ANativeActivity_onConfigurationChanged;
    cbs->onLowMemory = ANativeActivity_onLowMemory;

    act->instance = test;

    android_test_init(test);
}
