// Stage 2 scaffold. The real GameActivity lifecycle + Vulkan delegation
// lands in commit 4 of the shell-android-stage2 series. Today this just
// includes GameActivity's glue + logs that native entry was reached so
// the Gradle wiring can be verified independently of the device test.

// We link against `game-activity::game-activity_static`, which already
// ships GameActivity.cpp + the native_app_glue .c compiled in. Include
// only the headers.
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <android/log.h>

extern "C" void android_main(android_app* app) {
    __android_log_print(ANDROID_LOG_INFO, "phenotype",
                        "phenotype-android-example scaffold: entered android_main");
    (void)app;
    while (!app->destroyRequested) {
        int events = 0;
        android_poll_source* source = nullptr;
        if (ALooper_pollOnce(-1, nullptr, &events,
                             reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app, source);
        }
    }
}
