module;
export module phenotype.native.windows;

#ifndef __wasi__
import phenotype.native.platform;
import phenotype.native.stub;

export namespace phenotype::native {

inline platform_api const& windows_platform() {
    static platform_api api = make_stub_platform(
        "windows",
        "[phenotype-native] using Windows stub backend; renderer/text are not implemented yet");
    return api;
}

} // namespace phenotype::native
#endif
