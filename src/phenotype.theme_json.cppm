module;
#include <expected>
#include <string>
#include <string_view>
export module phenotype.theme_json;

import phenotype.types;
import json;
import txn;

export namespace phenotype {

// Deserialize a Theme from a JSON string. All 18 fields must be
// present — partial override is not supported in v1. Use
// theme_to_json(Theme{}) to get the full default as a starting
// point, then edit the fields you want.
//
// Returns an error string on failure (missing key, type mismatch).
// Parse failures (malformed JSON) abort on wasm — callers should
// pre-validate with JSON.parse() on the JS side before passing the
// string into the WASM instance.
inline auto theme_from_json(std::string_view json_str)
    -> std::expected<Theme, std::string> {
    auto value = json::parse(json_str);
    auto result = txn::from_value<Theme>(value);
    if (!result)
        return std::unexpected(
            result.error().path + ": " + result.error().message);
    return *result;
}

// Serialize a Theme to a JSON string. Useful as a starting point
// for editing or for persisting the current theme.
inline auto theme_to_json(Theme const& theme) -> std::string {
    auto value = txn::to_value<json::Value>(theme);
    return json::emit(value);
}

} // namespace phenotype
