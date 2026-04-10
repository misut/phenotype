import phenotype;

struct State {};
struct Msg {};

void update(State&, Msg) {}

void view(State const&) {
    using namespace phenotype;
    layout::column([&] {
        widget::text("Hello from C++!");
        widget::text("This page is rendered by phenotype, a C++ WASM UI framework.");
    });
}

int main() {
    phenotype::run<State, Msg>(view, update);
    return 0;
}
