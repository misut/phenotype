#include <cassert>
#include <cstdio>
#include <string_view>

import phenotype.theme_contract;

namespace theme = phenotype::theme_contract;

int main() {
    auto contract = theme::default_glass_theme_contract();
    assert(theme::theme_contract_version == 1);
    assert(contract.profile_name == "apple-glass-light");
    assert(contract.reference.find("Apple HIG Materials")
           != std::string_view::npos);
    assert(contract.reference.find("Liquid Glass")
           != std::string_view::npos);
    assert(contract.font_policy.find("Pretendard")
           != std::string_view::npos);
    assert(contract.material_policy.find("pure material planner")
           != std::string_view::npos);
    assert(contract.iconography_policy.find("macos_finder")
           != std::string_view::npos);
    assert(contract.iconography_policy.find("sf_symbols_aligned")
           != std::string_view::npos);
    assert(contract.iconography_policy.find("simplified_universal_metaphors")
           != std::string_view::npos);
    assert(contract.icon_asset_policy.find("phenotype_owned_svg_symbols")
           != std::string_view::npos);
    assert(contract.icon_asset_policy.find("audited_permissive_svg_sources")
           != std::string_view::npos);
    assert(contract.icon_asset_policy.find("without_embedded_apple_artwork")
           != std::string_view::npos);
    assert(contract.usage_policy.find("navigation_controls")
           != std::string_view::npos);
    assert(contract.usage_policy.find("not_content_fill")
           != std::string_view::npos);
    assert(contract.container_policy.find("explicit_container_spacing")
           != std::string_view::npos);
    assert(contract.performance_policy.find("bounded_glass_surfaces")
           != std::string_view::npos);
    assert(contract.accessibility_policy.find("reduced_transparency")
           != std::string_view::npos);
    assert(contract.fallback_policy.find("unsupported_backends")
           != std::string_view::npos);
    assert((contract.background == theme::ColorToken{242, 242, 247, 255}));
    assert((contract.foreground == theme::ColorToken{28, 28, 30, 255}));
    assert((contract.accent == theme::ColorToken{0, 122, 255, 255}));
    assert((contract.surface == theme::ColorToken{255, 255, 255, 238}));
    assert(contract.radius.sm == 10.0f);
    assert(contract.radius.md == 14.0f);
    assert(contract.radius.lg == 22.0f);
    assert(contract.typography.default_font_family == "Pretendard");
    assert(theme::glass_surface_roles().size() == 7);
    std::puts("PASS: pure theme contract metadata");
}
