// GameActivity driver for phenotype. Talks to the phenotype library via
// the C ABI (`phenotype_android_*`) that phenotype.native.android exposes,
// so this file does not need to import the phenotype C++ modules. Stage 2
// scope: clear background per frame, survive the INIT_WINDOW / TERM_WINDOW
// lifecycle. Stage 6 adds touch event routing on top.

#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <android/log.h>

extern "C" {
// Stage 4 adds a JNI-backed text pipeline; phenotype needs the process
// JavaVM* before text_init / text_build_atlas can run. GameActivity
// exposes it at app->activity->vm.
// Stage 5 adds an AAssetManager-backed image loader for `asset://`
// URLs; GameActivity exposes the manager at app->activity->assetManager.
// Stage 6 adds the view/update loop + input dispatch.
void phenotype_android_bind_jvm(void* jvm);
void phenotype_android_bind_assets(void* asset_manager);
void phenotype_android_attach_surface(void* native_window);
void phenotype_android_detach_surface(void);
void phenotype_android_draw_frame(void);
void phenotype_android_start_app(void);
void phenotype_android_dispatch_pointer(float x, float y, int action);
void phenotype_android_dispatch_key(int android_keycode, int action, int mods);
void phenotype_android_dispatch_char(unsigned int codepoint);
void phenotype_android_dispatch_scroll(double dy);
char const* phenotype_android_startup_message(void);
}

namespace {

constexpr char const* TAG = "phenotype";

bool g_surface_ready = false;
bool g_app_started = false;

void handle_cmd(android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window) {
            phenotype_android_attach_surface(app->window);
            if (!g_app_started) {
                // Stage 6: phenotype_android_start_app calls the shell's
                // run_host<demo6::State, demo6::Msg>. Once-per-process:
                // subsequent INIT_WINDOW cycles reuse the same app state.
                phenotype_android_start_app();
                g_app_started = true;
            }
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
    // Hand phenotype the process-scope JavaVM before any attach_surface
    // so the Stage 4 text pipeline can cache its JNI refs on first use.
    // Stage 5 adds the AAssetManager so `asset://` DrawImage URLs
    // resolve to APK-bundled files.
    if (app->activity) {
        phenotype_android_bind_jvm(app->activity->vm);
        phenotype_android_bind_assets(app->activity->assetManager);
    }
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

        if (!g_surface_ready) continue;

        // Stage 6: drain GameActivity's input buffer every tick and
        // forward each motion / key / char event to phenotype's input
        // dispatch via the C ABI. All three dispatch entry points run
        // on this same native_app_glue thread, so there's no cross-
        // thread synchronization with the Vulkan render below.
        if (android_input_buffer* ib = android_app_swap_input_buffers(app)) {
            for (uint64_t i = 0; i < ib->motionEventsCount; ++i) {
                GameActivityMotionEvent const& ev = ib->motionEvents[i];
                int32_t masked = ev.action & AMOTION_EVENT_ACTION_MASK;
                int ptr = (ev.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                        >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                float x = GameActivityPointerAxes_getX(&ev.pointers[ptr]);
                float y = GameActivityPointerAxes_getY(&ev.pointers[ptr]);
                if (masked == AMOTION_EVENT_ACTION_DOWN
                    || masked == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                    phenotype_android_dispatch_pointer(x, y, 0);
                } else if (masked == AMOTION_EVENT_ACTION_MOVE) {
                    phenotype_android_dispatch_pointer(x, y, 1);
                } else if (masked == AMOTION_EVENT_ACTION_UP
                        || masked == AMOTION_EVENT_ACTION_POINTER_UP
                        || masked == AMOTION_EVENT_ACTION_CANCEL) {
                    phenotype_android_dispatch_pointer(x, y, 2);
                } else if (masked == AMOTION_EVENT_ACTION_SCROLL) {
                    // Stage 7: Mouse wheel / trackpad / emulator
                    // `input roll` events surface with a VSCROLL axis.
                    // Touch-screen two-finger scroll doesn't generate
                    // this — it's a future Stage 8 gesture story.
                    float dy = GameActivityPointerAxes_getAxisValue(
                        &ev.pointers[ptr], AMOTION_EVENT_AXIS_VSCROLL);
                    if (dy != 0.0f) {
                        phenotype_android_dispatch_scroll(
                            static_cast<double>(dy));
                    }
                }
            }
            android_app_clear_motion_events(ib);

            for (uint64_t i = 0; i < ib->keyEventsCount; ++i) {
                GameActivityKeyEvent const& ev = ib->keyEvents[i];
                int action;
                if (ev.action == AKEY_EVENT_ACTION_DOWN) {
                    action = (ev.repeatCount > 0) ? 2 : 1; // Repeat vs Press
                } else if (ev.action == AKEY_EVENT_ACTION_UP) {
                    action = 0; // Release
                } else {
                    continue;
                }
                phenotype_android_dispatch_key(ev.keyCode, action, ev.modifiers);
                if (ev.unicodeChar > 0 && action != 0) {
                    phenotype_android_dispatch_char(
                        static_cast<unsigned int>(ev.unicodeChar));
                }
            }
            android_app_clear_key_events(ib);
        }

        phenotype_android_draw_frame();
    }

    phenotype_android_detach_surface();
    __android_log_print(ANDROID_LOG_INFO, TAG, "phenotype example exiting");
}
