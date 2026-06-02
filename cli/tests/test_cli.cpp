import phenotype.cli;
import std;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::println(std::cerr, "FAIL: {}", message);
        ++failures;
    }
}

void test_port_parser() {
    auto port = phenotype::cli::parse_port("4174");
    check(port && *port == 4174, "parse valid port");
    check(!phenotype::cli::parse_port("").has_value(), "reject empty port");
    check(!phenotype::cli::parse_port("abc").has_value(), "reject non-numeric port");
    check(!phenotype::cli::parse_port("70000").has_value(), "reject out-of-range port");
}

void test_build_spec_defaults_to_mise() {
    auto options = phenotype::cli::Options{};
    options.repo_root = std::filesystem::path{"/repo"};
    auto spec = phenotype::cli::build_process_spec(options);
    check(spec.program == "mise", "default program is mise");
    check(spec.args.size() == 6, "default build arg count");
    check(spec.args[0] == "exec", "mise exec");
    check(spec.args[1] == "--", "mise separator");
    check(spec.args[2] == "exon", "default exon program");
    check(spec.args[3] == "build", "build command");
    check(spec.args[4] == "--target", "target flag");
    check(spec.args[5] == "wasm32-wasi", "default target");
    check(spec.cwd == std::filesystem::path{"/repo/docs"}, "build cwd is docs");
}

void test_build_spec_direct_exon_release() {
    auto options = phenotype::cli::Options{};
    options.repo_root = std::filesystem::path{"/repo"};
    options.no_mise = true;
    options.exon = "/tools/exon";
    options.release = true;
    auto spec = phenotype::cli::build_process_spec(options);
    check(spec.program == "/tools/exon", "direct exon program");
    check(spec.args.size() == 4, "release arg count");
    check(spec.args[0] == "build", "direct build command");
    check(spec.args[3] == "--release", "release flag");
}

void test_default_paths() {
    auto root = std::filesystem::path{"/repo"};
    auto out = phenotype::cli::default_out_dir(root, "wasm32-wasi", false);
    check(out == root / ".phenotype" / "site" / "wasm32-wasi" / "debug",
          "default debug out dir");

    auto artifact = phenotype::cli::build_artifact_path(
        root / "docs",
        "wasm32-wasi",
        true);
    check(artifact == root / "docs" / ".exon" / "wasm32-wasi" / "release" / "docs",
          "release wasm artifact path");
}

void test_stage_site() {
    auto temp = std::filesystem::temp_directory_path() /
                std::format("phenotype-test-{}",
                            std::chrono::steady_clock::now()
                                .time_since_epoch()
                                .count());
    auto root = temp / "repo";
    std::filesystem::create_directories(root / "docs" / ".exon" / "wasm32-wasi" / "debug");
    std::filesystem::create_directories(root / "shim");

    {
        auto out = std::ofstream{root / "exon.toml"};
        out << "[package]\nname = \"phenotype\"\n";
    }
    {
        auto out = std::ofstream{root / "docs" / "exon.toml"};
        out << "[package]\nname = \"docs\"\n";
    }
    {
        auto out = std::ofstream{root / "docs" / "index.html"};
        out << "<script>mount('docs.wasm')</script>";
    }
    {
        auto out = std::ofstream{root / "shim" / "phenotype.js"};
        out << "export async function mount() {}";
    }
    {
        auto out = std::ofstream{root / "docs" / ".exon" / "wasm32-wasi" /
                                 "debug" / "docs",
                                 std::ios::binary};
        auto bytes = std::array<char, 4>{'\0', 'a', 's', 'm'};
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    auto options = phenotype::cli::Options{};
    options.repo_root = root;
    options.out_dir = root / ".phenotype" / "site" / "wasm32-wasi" / "debug";
    auto staged = phenotype::cli::stage_site(options);
    check(staged.has_value(), "stage site succeeds");
    if (staged) {
        check(std::filesystem::is_regular_file(staged->staged_index_html),
              "staged index exists");
        check(std::filesystem::is_regular_file(staged->staged_shim_js),
              "staged shim exists");
        check(std::filesystem::is_regular_file(staged->staged_wasm),
              "staged wasm exists");
    }

    std::filesystem::remove_all(temp);
}

} // namespace

int main() {
    test_port_parser();
    test_build_spec_defaults_to_mise();
    test_build_spec_direct_exon_release();
    test_default_paths();
    test_stage_site();
    return failures == 0 ? 0 : 1;
}
