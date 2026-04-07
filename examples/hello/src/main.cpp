import phenotype;
using namespace phenotype;

auto HelloApp() {
    Column([&] {
        Text("Hello from C++!");
        Text("This page is rendered by phenotype, a C++ WASM UI framework.");
    });
}

int main() {
    express(HelloApp);
    return 0;
}
