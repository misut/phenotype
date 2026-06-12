import phenotype;

int main() {
    constexpr phenotype::MaterialSymbolOptions default_options;
    static_assert(!default_options.fill);
    static_assert(default_options.weight == 400.0f);
    static_assert(default_options.grade == 0.0f);
    static_assert(default_options.optical_size == 24.0f);

    constexpr auto left = phenotype::MaterialSymbolChevronGeometryFor(
        phenotype::MaterialSymbolIcon::chevron_left);
    constexpr auto right = phenotype::MaterialSymbolChevronGeometryFor(
        phenotype::MaterialSymbolIcon::chevron_right);
    static_assert(left.outer_x == -right.outer_x);
    static_assert(left.center_x == -right.center_x);
    static_assert(left.top_y == right.top_y);
    static_assert(left.bottom_y == right.bottom_y);

    return 0;
}
