module;

#include <csignal>

export module phenotype.cli;

import cppx.cli;
import cppx.http;
import cppx.http.server;
import cppx.http.system;
import cppx.process;
import cppx.process.system;
import std;

export namespace phenotype::cli {

inline constexpr auto default_host = std::string_view{"127.0.0.1"};
inline constexpr auto default_serve_port = std::uint16_t{4174};
inline constexpr auto default_target = std::string_view{"wasm32-wasi"};
inline constexpr auto program_name = std::string_view{"phenotype"};

inline volatile std::sig_atomic_t serve_shutdown_signal = 0;

void request_serve_shutdown(int signal_number) {
    serve_shutdown_signal = static_cast<std::sig_atomic_t>(signal_number);
}

auto signal_name(int signal_number) -> std::string_view {
    switch (signal_number) {
    case SIGINT:
        return "SIGINT";
#if defined(SIGTERM)
    case SIGTERM:
        return "SIGTERM";
#endif
    default:
        return "signal";
    }
}

class ServeSignalScope {
    using Handler = void (*)(int);

    Handler previous_int_ = SIG_DFL;
    Handler previous_term_ = SIG_DFL;
    bool has_int_ = false;
    bool has_term_ = false;

public:
    ServeSignalScope() {
        serve_shutdown_signal = 0;

        previous_int_ = std::signal(SIGINT, request_serve_shutdown);
        has_int_ = previous_int_ != SIG_ERR;

#if defined(SIGTERM)
        previous_term_ = std::signal(SIGTERM, request_serve_shutdown);
        has_term_ = previous_term_ != SIG_ERR;
#endif
    }

    ServeSignalScope(ServeSignalScope const&) = delete;
    auto operator=(ServeSignalScope const&) -> ServeSignalScope& = delete;

    ~ServeSignalScope() {
        if (has_int_)
            std::signal(SIGINT, previous_int_);
#if defined(SIGTERM)
        if (has_term_)
            std::signal(SIGTERM, previous_term_);
#endif
    }

    auto requested() const -> bool {
        return serve_shutdown_signal != 0;
    }

    auto requested_signal() const -> int {
        return static_cast<int>(serve_shutdown_signal);
    }
};

struct CliError {
    std::string message;
    int exit_code = 1;
};

struct Options {
    std::filesystem::path repo_root;
    std::filesystem::path out_dir;
    std::string target = std::string{default_target};
    bool release = false;
    bool no_mise = false;
    std::string mise = "mise";
    std::string exon = "exon";
    bool no_build = false;
    std::string host = std::string{default_host};
    std::uint16_t port = default_serve_port;
    bool open = false;
};

struct SitePaths {
    std::filesystem::path repo_root;
    std::filesystem::path docs_dir;
    std::filesystem::path index_html;
    std::filesystem::path shim_js;
    std::filesystem::path wasm_artifact;
    std::filesystem::path out_dir;
    std::filesystem::path staged_index_html;
    std::filesystem::path staged_shim_js;
    std::filesystem::path staged_wasm;
};

struct SmokeResult {
    std::uint16_t port = 0;
    std::size_t wasm_bytes = 0;
};

auto command_spec() -> cppx::cli::CommandSpec {
    auto common = std::vector<cppx::cli::OptionSpec>{
        {.name = "help", .short_name = 'h',
         .description = "Show help for this command"},
        {.name = "root", .arity = cppx::cli::OptionArity::one,
         .value_name = "dir",
         .description = "phenotype repository root"},
        {.name = "out-dir", .arity = cppx::cli::OptionArity::one,
         .value_name = "dir",
         .description = "staged browser site directory"},
        {.name = "target", .arity = cppx::cli::OptionArity::one,
         .value_name = "triple",
         .description = "exon build target",
         .value_hints = {std::string{default_target}}},
        {.name = "release",
         .description = "build with exon release profile"},
        {.name = "no-mise",
         .description = "invoke exon directly instead of `mise exec -- exon`"},
        {.name = "mise", .arity = cppx::cli::OptionArity::one,
         .value_name = "program",
         .description = "mise executable to run"},
        {.name = "exon", .arity = cppx::cli::OptionArity::one,
         .value_name = "program",
         .description = "exon executable to run"},
    };

    auto build_options = common;
    auto serve_options = common;
    serve_options.push_back({
        .name = "host", .arity = cppx::cli::OptionArity::one,
        .value_name = "host",
        .description = "host interface for the local preview server",
    });
    serve_options.push_back({
        .name = "port", .short_name = 'p',
        .arity = cppx::cli::OptionArity::one,
        .value_name = "port",
        .description = "port for the local preview server",
    });
    serve_options.push_back({
        .name = "no-build",
        .description = "serve the existing staged files without rebuilding",
    });
    serve_options.push_back({
        .name = "open", .short_name = 'o',
        .description = "open the preview URL in the default browser",
    });

    auto test_options = common;
    test_options.push_back({
        .name = "host", .arity = cppx::cli::OptionArity::one,
        .value_name = "host",
        .description = "host interface for the temporary test server",
    });
    test_options.push_back({
        .name = "port", .short_name = 'p',
        .arity = cppx::cli::OptionArity::one,
        .value_name = "port",
        .description = "port for the temporary test server",
    });
    test_options.push_back({
        .name = "no-build",
        .description = "test the existing staged files without rebuilding",
    });

    return {
        .name = std::string{program_name},
        .summary = "Build and preview phenotype WASI apps locally",
        .description =
            "Builds the docs WASI binary, stages the browser shim and HTML, "
            "serves them with cppx HTTP, and can run an HTTP smoke test.",
        .options = {
            {.name = "help", .short_name = 'h',
             .description = "Show help"},
            {.name = "version", .short_name = 'v',
             .description = "Show version"},
        },
        .subcommands = {
            {
                .name = "build",
                .summary = "Build docs for WASI and stage a browser site",
                .options = build_options,
                .positional_name = "root",
                .positional_description =
                    "Optional phenotype repository root",
                .examples = {
                    "phenotype build",
                    "phenotype build --release --out-dir dist/phenotype",
                },
            },
            {
                .name = "serve",
                .summary = "Build and serve the staged browser site",
                .options = serve_options,
                .positional_name = "root",
                .positional_description =
                    "Optional phenotype repository root",
                .examples = {
                    "phenotype serve",
                    "phenotype serve --port 4174 --open",
                    "phenotype serve --no-build --out-dir .phenotype/site/wasm32-wasi/debug",
                },
            },
            {
                .name = "test",
                .summary = "Build, serve, and verify browser assets over HTTP",
                .options = test_options,
                .positional_name = "root",
                .positional_description =
                    "Optional phenotype repository root",
                .examples = {
                    "phenotype test",
                    "phenotype test --no-build",
                },
            },
            {
                .name = "help",
                .summary = "Show help",
                .positional_name = "command",
                .positional_description = "Optional command name",
            },
        },
    };
}

auto find_command(cppx::cli::CommandSpec const& root,
                  std::span<std::string const> path)
    -> cppx::cli::CommandSpec const* {
    auto const* command = &root;
    for (std::size_t i = 1; i < path.size(); ++i) {
        auto found = std::ranges::find_if(
            command->subcommands,
            [&](cppx::cli::CommandSpec const& candidate) {
                return candidate.name == path[i] ||
                       std::ranges::contains(candidate.aliases, path[i]);
            });
        if (found == command->subcommands.end())
            return command;
        command = &*found;
    }
    return command;
}

auto repository_markers_exist(std::filesystem::path const& dir) -> bool {
    auto const root_manifest = dir / "exon.toml";
    auto const docs_manifest = dir / "docs" / "exon.toml";
    auto const shim = dir / "shim" / "phenotype.js";
    return std::filesystem::is_regular_file(root_manifest) &&
           std::filesystem::is_regular_file(docs_manifest) &&
           std::filesystem::is_regular_file(shim);
}

auto canonical_or_absolute(std::filesystem::path path)
    -> std::filesystem::path {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec)
        return canonical;
    return std::filesystem::absolute(path);
}

auto discover_repo_root(std::filesystem::path start)
    -> std::expected<std::filesystem::path, CliError> {
    auto current = canonical_or_absolute(std::move(start));
    if (std::filesystem::is_regular_file(current))
        current = current.parent_path();

    for (;;) {
        if (repository_markers_exist(current))
            return current;
        if (!current.has_parent_path() || current == current.parent_path())
            break;
        current = current.parent_path();
    }

    return std::unexpected{CliError{
        .message =
            "could not find phenotype root; pass --root or run inside the repository",
        .exit_code = 2,
    }};
}

auto parse_port(std::string_view raw) -> std::expected<std::uint16_t, CliError> {
    if (raw.empty())
        return std::unexpected{CliError{.message = "port must not be empty",
                                        .exit_code = 2}};

    unsigned value = 0;
    for (auto ch : raw) {
        if (ch < '0' || ch > '9')
            return std::unexpected{CliError{
                .message = std::format("invalid port '{}'", raw),
                .exit_code = 2,
            }};
        value = value * 10U + static_cast<unsigned>(ch - '0');
        if (value > 65535U)
            return std::unexpected{CliError{
                .message = std::format("port out of range '{}'", raw),
                .exit_code = 2,
            }};
    }
    return static_cast<std::uint16_t>(value);
}

auto profile_name(bool release) -> std::string_view {
    return release ? "release" : "debug";
}

auto default_out_dir(std::filesystem::path const& root,
                     std::string_view target,
                     bool release) -> std::filesystem::path {
    return root / ".phenotype" / "site" / target / profile_name(release);
}

auto build_artifact_path(std::filesystem::path const& docs_dir,
                         std::string_view target,
                         bool release) -> std::filesystem::path {
    return docs_dir / ".exon" / target / profile_name(release) / "docs";
}

auto make_site_paths(Options const& options) -> SitePaths {
    auto docs_dir = options.repo_root / "docs";
    return {
        .repo_root = options.repo_root,
        .docs_dir = docs_dir,
        .index_html = docs_dir / "index.html",
        .shim_js = options.repo_root / "shim" / "phenotype.js",
        .wasm_artifact =
            build_artifact_path(docs_dir, options.target, options.release),
        .out_dir = options.out_dir,
        .staged_index_html = options.out_dir / "index.html",
        .staged_shim_js = options.out_dir / "phenotype.js",
        .staged_wasm = options.out_dir / "docs.wasm",
    };
}

auto option_path(cppx::cli::Invocation const& invocation,
                 std::string_view name) -> std::optional<std::filesystem::path> {
    if (auto value = invocation.value(name))
        return std::filesystem::path{std::string{*value}};
    return std::nullopt;
}

auto resolve_options(cppx::cli::Invocation const& invocation,
                     bool serve_defaults) -> std::expected<Options, CliError> {
    if (invocation.positionals.size() > 1)
        return std::unexpected{CliError{
            .message = "expected at most one root positional argument",
            .exit_code = 2,
        }};

    auto root_candidate = std::optional<std::filesystem::path>{};
    if (auto root_option = option_path(invocation, "root"))
        root_candidate = *root_option;
    else if (!invocation.positionals.empty())
        root_candidate = std::filesystem::path{invocation.positionals.front()};

    auto root = root_candidate
        ? discover_repo_root(*root_candidate)
        : discover_repo_root(std::filesystem::current_path());
    if (!root)
        return std::unexpected{root.error()};

    auto options = Options{};
    options.repo_root = *root;
    options.target = invocation.value("target")
        .transform([](std::string_view v) { return std::string{v}; })
        .value_or(std::string{default_target});
    options.release = invocation.has("release");
    options.no_mise = invocation.has("no-mise");
    options.mise = invocation.value("mise")
        .transform([](std::string_view v) { return std::string{v}; })
        .value_or("mise");
    options.exon = invocation.value("exon")
        .transform([](std::string_view v) { return std::string{v}; })
        .value_or("exon");
    options.no_build = invocation.has("no-build");
    options.host = invocation.value("host")
        .transform([](std::string_view v) { return std::string{v}; })
        .value_or(std::string{default_host});
    options.open = invocation.has("open");

    if (auto port = invocation.value("port")) {
        auto parsed = parse_port(*port);
        if (!parsed)
            return std::unexpected{parsed.error()};
        options.port = *parsed;
    } else {
        options.port = serve_defaults ? default_serve_port : 0;
    }

    if (serve_defaults && options.port == 0)
        return std::unexpected{CliError{
            .message = "serve requires a non-zero port",
            .exit_code = 2,
        }};

    options.out_dir = option_path(invocation, "out-dir")
        .transform([&](std::filesystem::path p) {
            return canonical_or_absolute(options.repo_root / p);
        })
        .value_or(default_out_dir(options.repo_root,
                                  options.target,
                                  options.release));
    return options;
}

auto build_process_spec(Options const& options)
    -> cppx::process::ProcessStreamSpec {
    auto args = std::vector<std::string>{};
    auto program = std::string{};
    if (options.no_mise) {
        program = options.exon;
    } else {
        program = options.mise;
        args.push_back("exec");
        args.push_back("--");
        args.push_back(options.exon);
    }

    args.push_back("build");
    args.push_back("--target");
    args.push_back(options.target);
    if (options.release)
        args.push_back("--release");

    auto spec = cppx::process::ProcessStreamSpec{};
    spec.program = std::move(program);
    spec.args = std::move(args);
    spec.cwd = options.repo_root / "docs";
    return spec;
}

auto process_error_message(std::string_view context,
                           cppx::process::process_error error) -> std::string {
    return std::format("{}: {}", context, cppx::process::to_string(error));
}

auto run_build(Options const& options) -> std::expected<void, CliError> {
    auto spec = build_process_spec(options);
    std::println("{}: building docs ({}, {})",
                 program_name,
                 options.target,
                 profile_name(options.release));

    auto summary = cppx::process::system::stream(
        std::move(spec),
        [](cppx::process::ProcessEvent const& event) {
            if (event.kind == cppx::process::ProcessEventKind::stdout_chunk)
                std::print("{}", event.text);
            if (event.kind == cppx::process::ProcessEventKind::stderr_chunk)
                std::print(std::cerr, "{}", event.text);
        });

    if (!summary)
        return std::unexpected{CliError{
            .message = process_error_message("failed to spawn exon", summary.error()),
        }};
    if (!summary->ok())
        return std::unexpected{CliError{
            .message = std::format("exon build failed with exit code {}",
                                   summary->exit_code),
            .exit_code = summary->exit_code == 0 ? 1 : summary->exit_code,
        }};
    return {};
}

auto remove_and_rename(std::filesystem::path const& from,
                       std::filesystem::path const& to)
    -> std::expected<void, CliError> {
    std::error_code ec;
    std::filesystem::remove_all(to, ec);
    if (ec)
        return std::unexpected{CliError{
            .message = std::format("failed to clear '{}': {}",
                                   to.string(),
                                   ec.message()),
        }};
    std::filesystem::rename(from, to, ec);
    if (ec)
        return std::unexpected{CliError{
            .message = std::format("failed to publish '{}': {}",
                                   to.string(),
                                   ec.message()),
        }};
    return {};
}

auto copy_file_checked(std::filesystem::path const& from,
                       std::filesystem::path const& to)
    -> std::expected<void, CliError> {
    std::error_code ec;
    std::filesystem::copy_file(
        from,
        to,
        std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec)
        return std::unexpected{CliError{
            .message = std::format("failed to copy '{}' to '{}': {}",
                                   from.string(),
                                   to.string(),
                                   ec.message()),
        }};
    return {};
}

auto require_file(std::filesystem::path const& path,
                  std::string_view description)
    -> std::expected<void, CliError> {
    if (std::filesystem::is_regular_file(path))
        return {};
    return std::unexpected{CliError{
        .message = std::format("missing {} at '{}'", description, path.string()),
    }};
}

auto stage_site(Options const& options) -> std::expected<SitePaths, CliError> {
    auto paths = make_site_paths(options);
    for (auto const& [path, description] : std::array{
             std::pair{paths.index_html, std::string_view{"docs index"}},
             std::pair{paths.shim_js, std::string_view{"browser shim"}},
             std::pair{paths.wasm_artifact, std::string_view{"WASI artifact"}},
         }) {
        auto ok = require_file(path, description);
        if (!ok)
            return std::unexpected{ok.error()};
    }

    auto temp_dir = paths.out_dir;
    temp_dir += ".tmp";

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
    if (ec)
        return std::unexpected{CliError{
            .message = std::format("failed to clear '{}': {}",
                                   temp_dir.string(),
                                   ec.message()),
        }};
    std::filesystem::create_directories(temp_dir, ec);
    if (ec)
        return std::unexpected{CliError{
            .message = std::format("failed to create '{}': {}",
                                   temp_dir.string(),
                                   ec.message()),
        }};

    auto temp_paths = paths;
    temp_paths.out_dir = temp_dir;
    temp_paths.staged_index_html = temp_dir / "index.html";
    temp_paths.staged_shim_js = temp_dir / "phenotype.js";
    temp_paths.staged_wasm = temp_dir / "docs.wasm";

    if (auto copied = copy_file_checked(paths.index_html,
                                        temp_paths.staged_index_html); !copied)
        return std::unexpected{copied.error()};
    if (auto copied = copy_file_checked(paths.shim_js,
                                        temp_paths.staged_shim_js); !copied)
        return std::unexpected{copied.error()};
    if (auto copied = copy_file_checked(paths.wasm_artifact,
                                        temp_paths.staged_wasm); !copied)
        return std::unexpected{copied.error()};

    if (auto published = remove_and_rename(temp_dir, paths.out_dir); !published)
        return std::unexpected{published.error()};

    return paths;
}

auto find_free_port(std::string_view host)
    -> std::expected<std::uint16_t, CliError> {
    auto listener = cppx::http::system::listener::bind(host, 0);
    if (!listener)
        return std::unexpected{CliError{
            .message = std::format("failed to bind test listener: {}",
                                   cppx::http::to_string(listener.error())),
        }};
    auto port = listener->local_port();
    listener->close();
    if (port == 0)
        return std::unexpected{CliError{.message = "failed to discover a test port"}};
    return port;
}

auto local_url(std::string_view host, std::uint16_t port, std::string_view path)
    -> std::string {
    return std::format("http://{}:{}{}", host, port, path);
}

auto open_url(std::string const& url) -> std::expected<void, CliError> {
#if defined(_WIN32)
    auto spec = cppx::process::ProcessSpec{
        .program = "cmd",
        .args = {"/c", "start", "", url},
    };
#elif defined(__APPLE__)
    auto spec = cppx::process::ProcessSpec{
        .program = "open",
        .args = {url},
    };
#else
    auto spec = cppx::process::ProcessSpec{
        .program = "xdg-open",
        .args = {url},
    };
#endif
    auto result = cppx::process::system::run(spec);
    if (!result)
        return std::unexpected{CliError{
            .message = process_error_message("failed to open browser",
                                             result.error()),
        }};
    if (result->exit_code != 0)
        return std::unexpected{CliError{
            .message = std::format("browser opener exited with code {}",
                                   result->exit_code),
        }};
    return {};
}

auto run_serve(Options const& options,
               SitePaths const& paths) -> std::expected<void, CliError> {
    auto server =
        cppx::http::server<cppx::http::system::listener,
                           cppx::http::system::stream>{};
    server.serve_static("/", paths.out_dir);

    auto url = local_url(options.host, options.port, "/");
    std::println("{}: serving '{}' at {}",
                 program_name,
                 paths.out_dir.string(),
                 url);
    std::println("{}: press Ctrl-C to stop", program_name);

    auto signal_scope = ServeSignalScope{};
    auto watcher_done = std::atomic<bool>{false};
    auto signal_announced = std::atomic<bool>{false};
    auto wake_url = local_url(options.host,
                              options.port,
                              "/__phenotype_shutdown__");
    auto shutdown_watcher = std::thread{[&] {
        while (!watcher_done.load() && !signal_scope.requested())
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        if (!signal_scope.requested())
            return;
        if (!signal_announced.exchange(true))
            std::println("{}: received {}, shutting down gracefully",
                         program_name,
                         signal_name(signal_scope.requested_signal()));
        server.stop();
        (void)cppx::http::system::get(wake_url);
    }};

    auto opener = std::thread{};
    if (options.open) {
        opener = std::thread{[url] {
            std::this_thread::sleep_for(std::chrono::milliseconds{250});
            if (auto opened = open_url(url); !opened)
                std::println(std::cerr, "warning: {}", opened.error().message);
        }};
    }

    auto result = server.run(options.host, options.port);
    watcher_done.store(true);
    if (signal_scope.requested()) {
        server.stop();
        (void)cppx::http::system::get(wake_url);
    }
    if (shutdown_watcher.joinable())
        shutdown_watcher.join();
    if (opener.joinable())
        opener.join();
    if (signal_scope.requested()) {
        std::println("{}: stopped", program_name);
        return {};
    }
    if (!result)
        return std::unexpected{CliError{
            .message = std::format("server failed: {}",
                                   cppx::http::to_string(result.error())),
        }};
    return {};
}

auto expect_get(std::string const& url,
                std::string_view expected_type,
                std::function<std::expected<void, CliError>(
                    cppx::http::response const&)> check)
    -> std::expected<cppx::http::response, CliError> {
    auto response = cppx::http::system::get(url);
    if (!response)
        return std::unexpected{CliError{
            .message = std::format("GET {} failed: {}",
                                   url,
                                   cppx::http::to_string(response.error())),
        }};
    if (response->stat.code != 200)
        return std::unexpected{CliError{
            .message = std::format("GET {} returned HTTP {}",
                                   url,
                                   response->stat.code),
        }};
    auto content_type = response->hdrs.get("content-type").value_or("");
    if (content_type != expected_type)
        return std::unexpected{CliError{
            .message = std::format("GET {} returned content-type '{}', expected '{}'",
                                   url,
                                   content_type,
                                   expected_type),
        }};
    if (auto checked = check(*response); !checked)
        return std::unexpected{checked.error()};
    return *response;
}

auto run_smoke_test(Options options,
                    SitePaths const& paths) -> std::expected<SmokeResult, CliError> {
    if (options.port == 0) {
        auto port = find_free_port(options.host);
        if (!port)
            return std::unexpected{port.error()};
        options.port = *port;
    }

    auto server =
        cppx::http::server<cppx::http::system::listener,
                           cppx::http::system::stream>{};
    server.serve_static("/", paths.out_dir);

    auto server_result =
        std::optional<std::expected<void, cppx::http::net_error>>{};
    auto finished = std::atomic<bool>{false};
    auto thread = std::thread{[&] {
        server_result = server.run(options.host, options.port);
        finished.store(true);
    }};

    auto base_url = local_url(options.host, options.port, "");
    auto ready = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (std::chrono::steady_clock::now() < deadline) {
        if (finished.load()) {
            if (server_result && !*server_result)
                break;
        }
        auto response = cppx::http::system::get(base_url + "/");
        if (response && response->stat.code == 200) {
            ready = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }

    auto stop_server = [&] {
        server.stop();
        (void)cppx::http::system::get(base_url + "/__phenotype_cli_stop__");
        if (thread.joinable())
            thread.join();
    };

    if (!ready) {
        stop_server();
        if (server_result && !*server_result)
            return std::unexpected{CliError{
                .message = std::format("test server failed: {}",
                                       cppx::http::to_string(server_result->error())),
            }};
        return std::unexpected{CliError{
            .message = "test server did not become ready within 5s",
        }};
    }

    auto index = expect_get(
        base_url + "/",
        "text/html",
        [](cppx::http::response const& response)
            -> std::expected<void, CliError> {
            if (!response.body_string().contains("mount('docs.wasm')"))
                return std::unexpected{CliError{
                    .message = "index.html does not mount docs.wasm",
                }};
            return {};
        });
    if (!index) {
        stop_server();
        return std::unexpected{index.error()};
    }

    auto shim = expect_get(
        base_url + "/phenotype.js",
        "application/javascript",
        [](cppx::http::response const& response)
            -> std::expected<void, CliError> {
            if (!response.body_string().contains("export async function mount"))
                return std::unexpected{CliError{
                    .message = "phenotype.js does not export mount",
                }};
            return {};
        });
    if (!shim) {
        stop_server();
        return std::unexpected{shim.error()};
    }

    auto wasm = expect_get(
        base_url + "/docs.wasm",
        "application/wasm",
        [](cppx::http::response const& response)
            -> std::expected<void, CliError> {
            auto body = response.body.view();
            auto data = body.data();
            auto wasm_magic =
                body.size() >= 4 &&
                data[0] == std::byte{0x00} &&
                data[1] == std::byte{0x61} &&
                data[2] == std::byte{0x73} &&
                data[3] == std::byte{0x6d};
            if (!wasm_magic)
                return std::unexpected{CliError{
                    .message = "docs.wasm does not have a WebAssembly header",
                }};
            return {};
        });
    if (!wasm) {
        stop_server();
        return std::unexpected{wasm.error()};
    }

    auto result = SmokeResult{
        .port = options.port,
        .wasm_bytes = wasm->body.size(),
    };
    stop_server();
    return result;
}

auto build_and_stage(Options const& options)
    -> std::expected<SitePaths, CliError> {
    if (!options.no_build) {
        if (auto built = run_build(options); !built)
            return std::unexpected{built.error()};
    }
    auto staged = stage_site(options);
    if (!staged)
        return std::unexpected{staged.error()};
    std::println("{}: staged '{}'", program_name, staged->out_dir.string());
    return staged;
}

auto cmd_build(cppx::cli::Invocation const& invocation) -> int {
    auto options = resolve_options(invocation, false);
    if (!options)
        return std::println(std::cerr, "error: {}", options.error().message),
               options.error().exit_code;
    auto staged = build_and_stage(*options);
    if (!staged)
        return std::println(std::cerr, "error: {}", staged.error().message),
               staged.error().exit_code;
    return 0;
}

auto cmd_serve(cppx::cli::Invocation const& invocation) -> int {
    auto options = resolve_options(invocation, true);
    if (!options)
        return std::println(std::cerr, "error: {}", options.error().message),
               options.error().exit_code;
    auto staged = build_and_stage(*options);
    if (!staged)
        return std::println(std::cerr, "error: {}", staged.error().message),
               staged.error().exit_code;
    auto served = run_serve(*options, *staged);
    if (!served)
        return std::println(std::cerr, "error: {}", served.error().message),
               served.error().exit_code;
    return 0;
}

auto cmd_test(cppx::cli::Invocation const& invocation) -> int {
    auto options = resolve_options(invocation, false);
    if (!options)
        return std::println(std::cerr, "error: {}", options.error().message),
               options.error().exit_code;
    auto staged = build_and_stage(*options);
    if (!staged)
        return std::println(std::cerr, "error: {}", staged.error().message),
               staged.error().exit_code;
    auto smoke = run_smoke_test(*options, *staged);
    if (!smoke)
        return std::println(std::cerr, "error: {}", smoke.error().message),
               smoke.error().exit_code;
    std::println("{}: smoke test passed at http://{}:{}/ ({} wasm bytes)",
                 program_name,
                 options->host,
                 smoke->port,
                 smoke->wasm_bytes);
    return 0;
}

auto print_help(cppx::cli::CommandSpec const& root,
                std::optional<std::string_view> command_name = std::nullopt)
    -> int {
    if (!command_name) {
        std::print("{}", cppx::cli::render_help(root));
        return 0;
    }

    auto found = std::ranges::find_if(
        root.subcommands,
        [&](cppx::cli::CommandSpec const& command) {
            return command.name == *command_name ||
                   std::ranges::contains(command.aliases, *command_name);
        });
    if (found == root.subcommands.end()) {
        std::println(std::cerr, "error: unknown command '{}'", *command_name);
        return 2;
    }
    std::print("{}", cppx::cli::render_help(
                         *found,
                         std::format("{} {}", root.name, found->name)));
    return 0;
}

auto run(cppx::cli::Invocation const& invocation,
         cppx::cli::CommandSpec const& root) -> int {
    auto const* command = find_command(root, invocation.command_path);
    if (invocation.has("help"))
        return print_help(root, command == &root
                                    ? std::optional<std::string_view>{}
                                    : std::string_view{command->name});

    if (invocation.command_path.size() < 2)
        return print_help(root);

    auto command_name = std::string_view{invocation.command_path[1]};
    if (command_name == "build")
        return cmd_build(invocation);
    if (command_name == "serve")
        return cmd_serve(invocation);
    if (command_name == "test")
        return cmd_test(invocation);
    if (command_name == "help")
        return print_help(
            root,
            invocation.positionals.empty()
                ? std::optional<std::string_view>{}
                : std::string_view{invocation.positionals.front()});

    std::println(std::cerr, "error: unknown command '{}'", command_name);
    return 2;
}

auto main(int argc, char** argv) -> int {
    auto root = command_spec();
    auto args = std::vector<std::string_view>{};
    for (int i = 1; i < argc; ++i)
        args.push_back(argv[i]);

    if (args.empty())
        return print_help(root);
    if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h"))
        return print_help(root);
    if (args.size() == 1 && (args[0] == "--version" || args[0] == "-v")) {
        std::println("{} {}", program_name, EXON_PKG_VERSION);
        return 0;
    }

    auto parsed = cppx::cli::parse(root, args);
    if (!parsed) {
        std::println(std::cerr, "error: {}", parsed.error().message);
        if (parsed.error().suggestion)
            std::println(std::cerr, "hint: did you mean '{}'?",
                         *parsed.error().suggestion);
        return 2;
    }
    return run(*parsed, root);
}

} // namespace phenotype::cli
