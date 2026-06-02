export module phenotype.native;

import std;
export import phenotype;

export namespace phenotype::native {

template <typename App>
int run_app(int,
            int,
            char const*,
            App app,
            std::function<void(int, int, float)> on_viewport = {}) {
    if (on_viewport)
        on_viewport(960, 720, 1.0f);
    ui::run<App>(std::move(app));
    return 0;
}

} // namespace phenotype::native
