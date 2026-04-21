// GameActivity driver for phenotype. Talks to the phenotype library via
// the C ABI (`phenotype_android_*`) that phenotype.native.android exposes,
// so this file does not need to import the phenotype C++ modules. Stage 2
// scope: clear background per frame, survive the INIT_WINDOW / TERM_WINDOW
// lifecycle. Stage 6 adds touch event routing on top.

#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <android/log.h>

extern "C" {
void phenotype_android_attach_surface(void* native_window);
void phenotype_android_detach_surface(void);
void phenotype_android_draw_frame(void);
char const* phenotype_android_startup_message(void);
}

namespace {

constexpr char const* TAG = "phenotype";

bool g_surface_ready = false;

void handle_cmd(android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window) {
            phenotype_android_attach_surface(app->window);
            g_surface_ready = true;
            __android_log_print(ANDROID_LOG_INFO, TAG, "APP_CMD_INIT_WINDOW");
        }
        break;
    case APP_CMD_TERM_WINDOW:
        g_surface_ready = false;
        phenotype_android_detach_surface();
        __android_log_print(ANDROID_LOG_INFO, TAG, "APP_CMD_TERM_WINDOW");
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        // Swapchain recreate is handled opportunistically inside the
        // Vulkan backend on VK_ERROR_OUT_OF_DATE_KHR; no explicit reset
        // needed here for Stage 2.
        break;
    default:
        break;
    }
}

} // namespace

extern "C" void android_main(android_app* app) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "phenotype example starting");
    if (auto const* msg = phenotype_android_startup_message()) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "%s", msg);
    }
    app->onAppCmd = handle_cmd;

    while (!app->destroyRequested) {
        int events = 0;
        android_poll_source* source = nullptr;
        int timeout = g_surface_ready ? 0 : -1;
        while (ALooper_pollOnce(timeout, nullptr, &events,
                                reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) break;
            timeout = 0;
        }

        if (g_surface_ready) phenotype_android_draw_frame();
    }

    phenotype_android_detach_surface();
    __android_log_print(ANDROID_LOG_INFO, TAG, "phenotype example exiting");
}
