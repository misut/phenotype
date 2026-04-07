import phenotype;

int main() {
    auto h1 = phenotype::create_element("h1", 2);
    phenotype::set_text(h1, "Hello from C++!", 15);

    auto p = phenotype::create_element("p", 1);
    phenotype::set_text(p, "This page is rendered by phenotype, a C++ WASM UI framework.", 60);

    phenotype::append_child(0, h1);
    phenotype::append_child(0, p);
    phenotype::flush();

    return 0;
}
