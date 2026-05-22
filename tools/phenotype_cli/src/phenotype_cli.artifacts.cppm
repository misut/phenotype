export module phenotype_cli.artifacts;

import cppx.cli;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import json;
import phenotype_cli.common;
import phenotype_cli.material_budget;
import phenotype_cli.runtime;
import std;

export namespace phenotype_cli::artifacts {

namespace fs = std::filesystem;
using namespace phenotype_cli::common;
using namespace phenotype_cli::material_budget;
using namespace phenotype_cli::runtime;

struct MaterialExecutorBudgetObservation {
    bool present = false;
    std::int64_t plan_count = -1;
    std::int64_t material_instance_count = -1;
    std::int64_t sampled_backdrop_instance_count = -1;
    std::int64_t fallback_instance_count = -1;
    std::int64_t execution_stage_count = -1;
    std::int64_t active_execution_stage_count = -1;
    std::int64_t backdrop_execution_stage_count = -1;
    std::int64_t dropped_execution_stage_count = -1;
    std::int64_t draw_calls = -1;
    std::int64_t total_sample_taps = -1;
    std::int64_t max_sample_taps = -1;
    std::int64_t upload_bytes = -1;
    std::int64_t buffer_capacity_bytes = -1;
    std::int64_t backdrop_copy_count = -1;
    std::int64_t backdrop_copy_pixels = -1;
    std::int64_t max_backdrop_pixels = -1;
    std::int64_t planned_frame_capture_count = -1;
    std::int64_t planned_frame_capture_pixels = -1;
    std::int64_t planned_surface_sample_pixels = -1;
    std::optional<bool> pipeline_ready;
    std::optional<bool> backdrop_source_ready;
    std::optional<bool> upload_required;
    std::optional<bool> draw_required;
    std::optional<bool> uploaded;
    std::optional<bool> drawn;
    std::optional<bool> backdrop_copy_required;
    std::string upload_status = "unknown";
    std::string draw_status = "unknown";
    std::string backdrop_copy_policy = "unknown";
    std::string backdrop_copy_skip_reason = "unknown";
};

struct MaterialObservationSummary {
    bool renderer_details_present = false;
    bool runtime_summary_present = false;
    std::int64_t contract_version = -1;
    std::int64_t declared_plan_count = -1;
    std::size_t plan_count = 0;
    std::size_t fallback_count = 0;
    std::size_t backdrop_sampling_count = 0;
    std::size_t shared_frame_capture_count = 0;
    std::size_t next_frame_capture_required_count = 0;
    std::int64_t runtime_plan_count = -1;
    std::int64_t runtime_backdrop_copy_count = -1;
    std::int64_t runtime_next_frame_capture_plan_count = -1;
    MaterialExecutorBudgetObservation executor_budget;
    std::vector<std::string> kinds;
    std::vector<std::string> roles;
    std::vector<std::string> fallback_paths;
    std::vector<std::string> fallback_reasons;
    std::vector<std::string> backdrop_sources;
    std::vector<std::string> backdrop_access_sources;
    std::vector<std::string> backdrop_capture_reasons;
    std::vector<std::string> primary_passes;
    std::vector<std::string> primary_executors;
    std::vector<std::string> likely_layers;
    std::vector<std::string> likely_passes;
    std::vector<std::string> decision_blockers;
};

struct SnapshotObservation {
    bool present = false;
    bool parse_ok = false;
    std::string parse_error;
    std::vector<std::string> top_level_keys;
    bool debug_present = false;
    bool semantic_tree_present = false;
    bool platform_capabilities_present = false;
    bool platform_runtime_present = false;
    bool runtime_details_present = false;
    bool renderer_details_present = false;
    bool material_surfaces_capability = false;
    bool material_backdrop_blur_capability = false;
    bool frame_image_capability = false;
    bool write_artifact_bundle_capability = false;
    std::string platform_name;
    MaterialObservationSummary material;
};

struct VerifierObservation {
    bool requested = false;
    bool executed = false;
    bool ok = false;
    int exit_code = 0;
    bool timed_out = false;
    std::string stdout_tail;
    std::string stderr_tail;
    std::optional<json::Value> report;
    std::string report_error;
};

struct ArtifactObservation {
    fs::path bundle;
    ArtifactSummary artifact;
    std::vector<Check> checks;
    SnapshotObservation snapshot;
    VerifierObservation verifier;
    std::vector<std::string> likely_owner_layers;
    std::vector<std::string> suggested_actions;
    bool ok = false;
};



auto artifact_checks(ArtifactSummary const& summary) -> std::vector<Check> {
    return {
        {.name = "bundle",
         .ok = summary.exists && summary.is_directory,
         .detail = path_string(summary.bundle),
         .hint = "Point to the artifact bundle directory produced by the renderer."},
        {.name = "snapshot_json",
         .ok = summary.snapshot_json && summary.snapshot_bytes > 0,
         .detail = std::format("{} bytes", summary.snapshot_bytes),
         .hint = "Expected snapshot.json with semantic/debug state."},
        {.name = "frame_bmp",
         .ok = summary.frame_bmp && summary.frame_bytes > 0,
         .detail = std::format("{} bytes", summary.frame_bytes),
         .hint = "Expected frame.bmp from the captured rendered frame."},
        {.name = "platform",
         .ok = summary.platform_directory && summary.platform_file_count > 0,
         .detail = std::format("{} files", summary.platform_file_count),
         .hint = "Expected platform runtime detail files for LLM debugging."},
    };
}

auto artifact_json(ArtifactSummary const& summary,
                   std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"artifact summary\",\"ok\":{},"
        "\"bundle\":{},\"snapshot_json\":{{\"present\":{},\"bytes\":{}}},"
        "\"frame_bmp\":{{\"present\":{},\"bytes\":{}}},"
        "\"platform\":{{\"present\":{},\"file_count\":{}}},\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        json_string(path_string(summary.bundle)),
        summary.snapshot_json ? "true" : "false",
        summary.snapshot_bytes,
        summary.frame_bmp ? "true" : "false",
        summary.frame_bytes,
        summary.platform_directory ? "true" : "false",
        summary.platform_file_count,
        checks_json(checks));
}

void append_unique(std::vector<std::string>& values, std::string value) {
    if (value.empty())
        return;
    if (!std::ranges::contains(values, value))
        values.push_back(std::move(value));
}

void append_json_string_field(json::Value const& value,
                              std::initializer_list<std::string_view> path,
                              std::vector<std::string>& out) {
    if (auto text = json_string_at(value, path))
        append_unique(out, *text);
}

void assign_json_integer(json::Value const& value,
                         std::initializer_list<std::string_view> path,
                         std::int64_t& out) {
    if (auto integer = json_integer_at(value, path))
        out = *integer;
}

void assign_json_string(json::Value const& value,
                        std::initializer_list<std::string_view> path,
                        std::string& out) {
    if (auto text = json_string_at(value, path))
        out = *text;
}

auto json_report_or_null(std::optional<json::Value> const& value)
    -> std::string {
    return value ? json::emit(*value) : "null";
}

auto optional_bool_json(std::optional<bool> value) -> std::string {
    if (!value)
        return "null";
    return *value ? "true" : "false";
}

auto material_executor_budget_from_runtime(json::Value const& runtime_value)
    -> MaterialExecutorBudgetObservation {
    auto budget = MaterialExecutorBudgetObservation{.present = true};
    assign_json_integer(runtime_value, {"plan_count"}, budget.plan_count);
    assign_json_integer(
        runtime_value,
        {"material_instance_count"},
        budget.material_instance_count);
    assign_json_integer(
        runtime_value,
        {"sampled_backdrop_instance_count"},
        budget.sampled_backdrop_instance_count);
    assign_json_integer(
        runtime_value,
        {"fallback_instance_count"},
        budget.fallback_instance_count);
    assign_json_integer(
        runtime_value,
        {"execution_stage_count"},
        budget.execution_stage_count);
    assign_json_integer(
        runtime_value,
        {"active_execution_stage_count"},
        budget.active_execution_stage_count);
    assign_json_integer(
        runtime_value,
        {"backdrop_execution_stage_count"},
        budget.backdrop_execution_stage_count);
    assign_json_integer(
        runtime_value,
        {"dropped_execution_stage_count"},
        budget.dropped_execution_stage_count);
    assign_json_integer(
        runtime_value,
        {"material_draw_calls"},
        budget.draw_calls);
    assign_json_integer(
        runtime_value,
        {"material_total_sample_taps"},
        budget.total_sample_taps);
    assign_json_integer(
        runtime_value,
        {"material_max_sample_taps"},
        budget.max_sample_taps);
    assign_json_integer(
        runtime_value,
        {"material_upload_bytes"},
        budget.upload_bytes);
    assign_json_integer(
        runtime_value,
        {"material_buffer_capacity_bytes"},
        budget.buffer_capacity_bytes);
    assign_json_integer(
        runtime_value,
        {"backdrop_copy_count"},
        budget.backdrop_copy_count);
    assign_json_integer(
        runtime_value,
        {"backdrop_copy_pixels"},
        budget.backdrop_copy_pixels);
    assign_json_integer(
        runtime_value,
        {"planned_frame_capture_count"},
        budget.planned_frame_capture_count);
    assign_json_integer(
        runtime_value,
        {"planned_frame_capture_pixels"},
        budget.planned_frame_capture_pixels);
    assign_json_integer(
        runtime_value,
        {"planned_surface_sample_pixels"},
        budget.planned_surface_sample_pixels);
    budget.pipeline_ready =
        json_bool_at(runtime_value, {"material_pipeline_ready"});
    budget.backdrop_source_ready =
        json_bool_at(runtime_value, {"material_backdrop_source_ready"});
    budget.upload_required = json_bool_at(
        runtime_value,
        {"material_sampled_backdrop_upload_required"});
    budget.draw_required = json_bool_at(
        runtime_value,
        {"material_sampled_backdrop_draw_required"});
    budget.uploaded =
        json_bool_at(runtime_value, {"material_sampled_backdrop_uploaded"});
    budget.drawn =
        json_bool_at(runtime_value, {"material_sampled_backdrop_drawn"});
    budget.backdrop_copy_required =
        json_bool_at(runtime_value, {"backdrop_copy_required"});
    assign_json_string(
        runtime_value,
        {"material_upload_status"},
        budget.upload_status);
    assign_json_string(
        runtime_value,
        {"material_draw_status"},
        budget.draw_status);
    assign_json_string(
        runtime_value,
        {"backdrop_copy_policy"},
        budget.backdrop_copy_policy);
    assign_json_string(
        runtime_value,
        {"backdrop_copy_skip_reason"},
        budget.backdrop_copy_skip_reason);
    return budget;
}

auto material_observation_from_snapshot(json::Value const& snapshot)
    -> MaterialObservationSummary {
    auto summary = MaterialObservationSummary{};
    auto const* renderer = json_object_at(
        snapshot,
        {"debug", "platform_runtime", "details", "renderer"});
    if (!renderer)
        return summary;

    summary.renderer_details_present = true;
    auto renderer_value = json::Value{*renderer};
    if (auto contract = json_integer_at(
            renderer_value,
            {"material_plan_contract_version"})) {
        summary.contract_version = *contract;
    }
    if (auto count = json_integer_at(renderer_value, {"material_plan_count"})) {
        summary.declared_plan_count = *count;
    }

    if (auto const* runtime = json_object_at(
            renderer_value,
            {"material_executor_summary"})) {
        summary.runtime_summary_present = true;
        auto runtime_value = json::Value{*runtime};
        summary.executor_budget =
            material_executor_budget_from_runtime(runtime_value);
        if (auto count = json_integer_at(runtime_value, {"plan_count"}))
            summary.runtime_plan_count = *count;
        if (auto count = json_integer_at(runtime_value, {"backdrop_copy_count"}))
            summary.runtime_backdrop_copy_count = *count;
        if (auto count = json_integer_at(
                runtime_value,
                {"next_frame_capture_plan_count"})) {
            summary.runtime_next_frame_capture_plan_count = *count;
        }
    }

    auto const* plans = json_array_at(renderer_value, {"material_plans"});
    if (!plans)
        return summary;

    summary.plan_count = plans->size();
    for (auto const& plan : *plans) {
        if (!plan.is_object())
            continue;
        append_json_string_field(plan, {"kind"}, summary.kinds);
        append_json_string_field(plan, {"role"}, summary.roles);
        append_json_string_field(plan, {"fallback_path"}, summary.fallback_paths);
        append_json_string_field(plan, {"fallback_reason"}, summary.fallback_reasons);
        append_json_string_field(plan, {"backdrop", "source"}, summary.backdrop_sources);
        append_json_string_field(
            plan,
            {"backdrop_access", "source"},
            summary.backdrop_access_sources);
        append_json_string_field(
            plan,
            {"backdrop_access", "capture_reason"},
            summary.backdrop_capture_reasons);
        append_json_string_field(plan, {"primary_pass", "name"}, summary.primary_passes);
        append_json_string_field(
            plan,
            {"primary_pass", "executor"},
            summary.primary_executors);
        append_json_string_field(plan, {"verifier", "likely_layer"}, summary.likely_layers);
        append_json_string_field(plan, {"verifier", "likely_pass"}, summary.likely_passes);
        append_json_string_field(
            plan,
            {"decision_trace", "first_blocker"},
            summary.decision_blockers);
        if (auto fallback = json_bool_at(plan, {"fallback"}); fallback.value_or(false))
            ++summary.fallback_count;
        if (auto sampling = json_bool_at(plan, {"backdrop_sampling"});
            sampling.value_or(false)) {
            ++summary.backdrop_sampling_count;
        }
        if (auto shared = json_bool_at(
                plan,
                {"backdrop_access", "shared_frame_capture"});
            shared.value_or(false)) {
            ++summary.shared_frame_capture_count;
        }
        if (auto next = json_bool_at(
                plan,
                {"backdrop_access", "next_frame_capture_required"});
            next.value_or(false)) {
            ++summary.next_frame_capture_required_count;
        }
        if (auto pixels = json_integer_at(
                plan,
                {"resource_budget", "max_backdrop_pixels"})) {
            summary.executor_budget.max_backdrop_pixels =
                std::max(summary.executor_budget.max_backdrop_pixels, *pixels);
        }
    }
    return summary;
}

auto observe_snapshot(fs::path const& bundle) -> SnapshotObservation {
    auto observation = SnapshotObservation{};
    auto snapshot_path = bundle / "snapshot.json";
    observation.present = path_exists(snapshot_path);
    if (!observation.present)
        return observation;

    auto text = read_text_file(snapshot_path);
    if (text.empty()) {
        observation.parse_error = "snapshot.json is empty or unreadable";
        return observation;
    }

    try {
        auto snapshot = json::parse(text);
        observation.parse_ok = true;
        if (snapshot.is_object()) {
            for (auto const& [key, _] : snapshot.as_object())
                observation.top_level_keys.push_back(key);
        }
        observation.debug_present = json_object_at(snapshot, {"debug"}) != nullptr;
        observation.semantic_tree_present =
            json_object_at(snapshot, {"debug", "semantic_tree"}) != nullptr;
        observation.platform_capabilities_present =
            json_object_at(snapshot, {"debug", "platform_capabilities"}) != nullptr;
        observation.platform_runtime_present =
            json_object_at(snapshot, {"debug", "platform_runtime"}) != nullptr;
        observation.runtime_details_present =
            json_object_at(snapshot, {"debug", "platform_runtime", "details"}) != nullptr;
        observation.renderer_details_present =
            json_object_at(
                snapshot,
                {"debug", "platform_runtime", "details", "renderer"}) != nullptr;
        observation.material_surfaces_capability =
            json_bool_at(
                snapshot,
                {"debug", "platform_capabilities", "material_surfaces"})
                .value_or(false);
        observation.material_backdrop_blur_capability =
            json_bool_at(
                snapshot,
                {"debug", "platform_capabilities", "material_backdrop_blur"})
                .value_or(false);
        observation.frame_image_capability =
            json_bool_at(
                snapshot,
                {"debug", "platform_capabilities", "frame_image"})
                .value_or(false);
        observation.write_artifact_bundle_capability =
            json_bool_at(
                snapshot,
                {"debug", "platform_capabilities", "write_artifact_bundle"})
                .value_or(false);
        if (auto platform = json_string_at(
                snapshot,
                {"debug", "platform_runtime", "platform"})) {
            observation.platform_name = *platform;
        }
        observation.material = material_observation_from_snapshot(snapshot);
    } catch (std::exception const& error) {
        observation.parse_error = error.what();
    }
    return observation;
}

auto material_executor_budget_json(
        MaterialExecutorBudgetObservation const& budget) -> std::string {
    return std::format(
        "{{\"present\":{},\"plan_count\":{},\"material_instance_count\":{},"
        "\"sampled_backdrop_instance_count\":{},"
        "\"fallback_instance_count\":{},\"execution_stage_count\":{},"
        "\"active_execution_stage_count\":{},"
        "\"backdrop_execution_stage_count\":{},"
        "\"dropped_execution_stage_count\":{},\"draw_calls\":{},"
        "\"total_sample_taps\":{},\"max_sample_taps\":{},"
        "\"upload_bytes\":{},\"buffer_capacity_bytes\":{},"
        "\"backdrop_copy_count\":{},\"backdrop_copy_pixels\":{},"
        "\"max_backdrop_pixels\":{},"
        "\"planned_frame_capture_count\":{},"
        "\"planned_frame_capture_pixels\":{},"
        "\"planned_surface_sample_pixels\":{},\"pipeline_ready\":{},"
        "\"backdrop_source_ready\":{},\"upload_required\":{},"
        "\"draw_required\":{},\"uploaded\":{},\"drawn\":{},"
        "\"backdrop_copy_required\":{},\"upload_status\":{},"
        "\"draw_status\":{},\"backdrop_copy_policy\":{},"
        "\"backdrop_copy_skip_reason\":{}}}",
        budget.present ? "true" : "false",
        budget.plan_count,
        budget.material_instance_count,
        budget.sampled_backdrop_instance_count,
        budget.fallback_instance_count,
        budget.execution_stage_count,
        budget.active_execution_stage_count,
        budget.backdrop_execution_stage_count,
        budget.dropped_execution_stage_count,
        budget.draw_calls,
        budget.total_sample_taps,
        budget.max_sample_taps,
        budget.upload_bytes,
        budget.buffer_capacity_bytes,
        budget.backdrop_copy_count,
        budget.backdrop_copy_pixels,
        budget.max_backdrop_pixels,
        budget.planned_frame_capture_count,
        budget.planned_frame_capture_pixels,
        budget.planned_surface_sample_pixels,
        optional_bool_json(budget.pipeline_ready),
        optional_bool_json(budget.backdrop_source_ready),
        optional_bool_json(budget.upload_required),
        optional_bool_json(budget.draw_required),
        optional_bool_json(budget.uploaded),
        optional_bool_json(budget.drawn),
        optional_bool_json(budget.backdrop_copy_required),
        json_string(budget.upload_status),
        json_string(budget.draw_status),
        json_string(budget.backdrop_copy_policy),
        json_string(budget.backdrop_copy_skip_reason));
}

auto material_observation_json(MaterialObservationSummary const& summary)
    -> std::string {
    return std::format(
        "{{\"renderer_details_present\":{},\"runtime_summary_present\":{},"
        "\"contract_version\":{},\"declared_plan_count\":{},"
        "\"plan_count\":{},\"fallback_count\":{},"
        "\"backdrop_sampling_count\":{},"
        "\"shared_frame_capture_count\":{},"
        "\"next_frame_capture_required_count\":{},"
        "\"runtime_plan_count\":{},\"runtime_backdrop_copy_count\":{},"
        "\"runtime_next_frame_capture_plan_count\":{},"
        "\"executor_budget\":{},"
        "\"kinds\":{},\"roles\":{},\"fallback_paths\":{},"
        "\"fallback_reasons\":{},\"backdrop_sources\":{},"
        "\"backdrop_access_sources\":{},"
        "\"backdrop_capture_reasons\":{},\"primary_passes\":{},"
        "\"primary_executors\":{},\"likely_layers\":{},"
        "\"likely_passes\":{},\"decision_blockers\":{}}}",
        summary.renderer_details_present ? "true" : "false",
        summary.runtime_summary_present ? "true" : "false",
        summary.contract_version,
        summary.declared_plan_count,
        summary.plan_count,
        summary.fallback_count,
        summary.backdrop_sampling_count,
        summary.shared_frame_capture_count,
        summary.next_frame_capture_required_count,
        summary.runtime_plan_count,
        summary.runtime_backdrop_copy_count,
        summary.runtime_next_frame_capture_plan_count,
        material_executor_budget_json(summary.executor_budget),
        string_array_json(summary.kinds),
        string_array_json(summary.roles),
        string_array_json(summary.fallback_paths),
        string_array_json(summary.fallback_reasons),
        string_array_json(summary.backdrop_sources),
        string_array_json(summary.backdrop_access_sources),
        string_array_json(summary.backdrop_capture_reasons),
        string_array_json(summary.primary_passes),
        string_array_json(summary.primary_executors),
        string_array_json(summary.likely_layers),
        string_array_json(summary.likely_passes),
        string_array_json(summary.decision_blockers));
}

auto snapshot_observation_json(SnapshotObservation const& observation)
    -> std::string {
    return std::format(
        "{{\"present\":{},\"parse_ok\":{},\"parse_error\":{},"
        "\"top_level_keys\":{},\"debug_present\":{},"
        "\"semantic_tree_present\":{},"
        "\"platform_capabilities_present\":{},"
        "\"platform_runtime_present\":{},"
        "\"runtime_details_present\":{},"
        "\"renderer_details_present\":{},"
        "\"platform_name\":{},"
        "\"capabilities\":{{\"material_surfaces\":{},"
        "\"material_backdrop_blur\":{},\"frame_image\":{},"
        "\"write_artifact_bundle\":{}}},"
        "\"material\":{}}}",
        observation.present ? "true" : "false",
        observation.parse_ok ? "true" : "false",
        json_string(observation.parse_error),
        string_array_json(observation.top_level_keys),
        observation.debug_present ? "true" : "false",
        observation.semantic_tree_present ? "true" : "false",
        observation.platform_capabilities_present ? "true" : "false",
        observation.platform_runtime_present ? "true" : "false",
        observation.runtime_details_present ? "true" : "false",
        observation.renderer_details_present ? "true" : "false",
        json_string(observation.platform_name),
        observation.material_surfaces_capability ? "true" : "false",
        observation.material_backdrop_blur_capability ? "true" : "false",
        observation.frame_image_capability ? "true" : "false",
        observation.write_artifact_bundle_capability ? "true" : "false",
        material_observation_json(observation.material));
}

auto verifier_observation_json(VerifierObservation const& verifier)
    -> std::string {
    auto const budget = verifier.report
        ? material_budget_from_report(*verifier.report)
        : std::optional<MaterialBudgetSummary>{};
    auto const manifest = verifier.report
        ? verifier_manifest_summary_from_report(*verifier.report)
        : std::optional<VerifierManifestSummary>{};
    auto const coverage = material_budget_coverage_summary(budget, manifest);
    auto const bound_summary = verifier.report
        ? material_budget_bound_summary_from_report(*verifier.report)
        : std::optional<MaterialBudgetBoundSummary>{};
    auto const bound_results = verifier.report
        ? material_budget_bound_results_from_report(*verifier.report)
        : std::optional<std::vector<MaterialBudgetBoundResult>>{};
    return std::format(
        "{{\"requested\":{},\"executed\":{},\"ok\":{},"
        "\"exit_code\":{},\"timed_out\":{},\"stdout_tail\":{},"
        "\"stderr_tail\":{},\"report_error\":{},"
        "\"material_budget\":{},\"verifier_manifest\":{},"
        "\"material_budget_coverage\":{},"
        "\"material_budget_bound_summary\":{},"
        "\"material_budget_bound_results\":{},\"report\":{}}}",
        verifier.requested ? "true" : "false",
        verifier.executed ? "true" : "false",
        verifier.ok ? "true" : "false",
        verifier.exit_code,
        verifier.timed_out ? "true" : "false",
        json_string(verifier.stdout_tail),
        json_string(verifier.stderr_tail),
        json_string(verifier.report_error),
        material_budget_json(budget),
        verifier_manifest_summary_json(manifest),
        material_budget_coverage_json(coverage),
        material_budget_bound_summary_json(bound_summary),
        material_budget_bound_results_json(bound_results),
        json_report_or_null(verifier.report));
}

auto artifact_observation_json(ArtifactObservation const& observation)
    -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"observe artifact\","
        "\"ok\":{},\"bundle\":{},\"artifact\":{},"
        "\"snapshot\":{},\"verifier\":{},\"likely_owner_layers\":{},"
        "\"suggested_actions\":{},\"checks\":{}}}",
        observation.ok ? "true" : "false",
        json_string(path_string(observation.bundle)),
        artifact_json(observation.artifact, observation.checks),
        snapshot_observation_json(observation.snapshot),
        verifier_observation_json(observation.verifier),
        string_array_json(observation.likely_owner_layers),
        string_array_json(observation.suggested_actions),
        checks_json(observation.checks));
}


auto optional_bool_text(std::optional<bool> value) -> std::string {
    if (!value)
        return "unknown";
    return *value ? "true" : "false";
}

void print_snapshot_material_budget(
        MaterialExecutorBudgetObservation const& budget) {
    if (!budget.present)
        return;

    std::println("snapshot material budget:");
    std::println(
        "  plans: {} sampled={} fallback={} stages={} active={} "
        "backdrop-stages={} dropped={}",
        budget_count(budget.plan_count),
        budget_count(budget.sampled_backdrop_instance_count),
        budget_count(budget.fallback_instance_count),
        budget_count(budget.execution_stage_count),
        budget_count(budget.active_execution_stage_count),
        budget_count(budget.backdrop_execution_stage_count),
        budget_count(budget.dropped_execution_stage_count));
    std::println(
        "  work: draw-calls={} taps={} max-taps={} upload={}/{} bytes "
        "upload-util={} copy={}/{} px copy-util={} "
        "frame-capture={}/{} px surface-sample={} px",
        budget_count(budget.draw_calls),
        budget_count(budget.total_sample_taps),
        budget_count(budget.max_sample_taps),
        budget_count(budget.upload_bytes),
        budget_count(budget.buffer_capacity_bytes),
        budget_ratio_text(budget.upload_bytes, budget.buffer_capacity_bytes),
        budget_count(budget.backdrop_copy_pixels),
        budget_count(budget.max_backdrop_pixels),
        budget_ratio_text(budget.backdrop_copy_pixels, budget.max_backdrop_pixels),
        budget_count(budget.planned_frame_capture_count),
        budget_count(budget.planned_frame_capture_pixels),
        budget_count(budget.planned_surface_sample_pixels));
    std::println(
        "  status: upload={} draw={} uploaded={} drawn={} "
        "copy-required={} copy-policy={} skip={}",
        budget.upload_status,
        budget.draw_status,
        optional_bool_text(budget.uploaded),
        optional_bool_text(budget.drawn),
        optional_bool_text(budget.backdrop_copy_required),
        budget.backdrop_copy_policy,
        budget.backdrop_copy_skip_reason);
}

void print_verifier_material_budget(VerifierObservation const& verifier) {
    if (!verifier.report)
        return;
    auto budget = material_budget_from_report(*verifier.report);
    if (!budget)
        return;

    std::println("verifier material budget:");
    std::println(
        "  plans: {} sampled={} fallback={} stages={} active={} "
        "backdrop-stages={} dropped={}",
        budget_count(budget->plan_count),
        budget_count(budget->sampled_backdrop_instance_count),
        budget_count(budget->fallback_instance_count),
        budget_count(budget->execution_stage_count),
        budget_count(budget->active_execution_stage_count),
        budget_count(budget->backdrop_execution_stage_count),
        budget_count(budget->dropped_execution_stage_count));
    std::println(
        "  work: draw-calls={} taps={} max-taps={} upload={}/{} bytes "
        "upload-util={} copy={}/{} px copy-util={} "
        "frame-capture={}/{} px surface-sample={} px",
        budget_count(budget->draw_calls),
        budget_count(budget->total_sample_taps),
        budget_count(budget->max_sample_taps),
        budget_count(budget->upload_bytes),
        budget_count(budget->buffer_capacity_bytes),
        budget_utilization_text(budget->upload_utilization),
        budget_count(budget->backdrop_copy_pixels),
        budget_count(budget->max_backdrop_pixels),
        budget_utilization_text(budget->backdrop_copy_utilization),
        budget_count(budget->planned_frame_capture_count),
        budget_count(budget->planned_frame_capture_pixels),
        budget_count(budget->planned_surface_sample_pixels));
    std::println(
        "  status: upload={} draw={} uploaded={} drawn={} "
        "copy-required={} copy-policy={} skip={}",
        budget->upload_status,
        budget->draw_status,
        budget_bool_text(budget->uploaded),
        budget_bool_text(budget->drawn),
        budget_bool_text(budget->backdrop_copy_required),
        budget->backdrop_copy_policy,
        budget->backdrop_copy_skip_reason);
}

void print_verifier_manifest_summary(VerifierObservation const& verifier) {
    if (!verifier.report)
        return;
    auto manifest = verifier_manifest_summary_from_report(*verifier.report);
    if (!manifest)
        return;

    std::println(
        "verifier manifest: name={} runtime-bounds={} budget-bounds={} "
        "pixel-regions={} metrics={} comparisons={} forbidden-colors={}",
        manifest->name,
        budget_count(manifest->runtime_numeric_bounds),
        budget_count(manifest->material_executor_budget_bounds),
        budget_count(manifest->pixel_regions),
        budget_count(manifest->pixel_region_metrics),
        budget_count(manifest->pixel_region_metric_comparisons),
        budget_count(manifest->forbid_pixel_region_colors));
}

void print_verifier_material_budget_coverage(
        VerifierObservation const& verifier) {
    if (!verifier.report)
        return;
    auto budget = material_budget_from_report(*verifier.report);
    auto manifest = verifier_manifest_summary_from_report(*verifier.report);
    auto coverage = material_budget_coverage_summary(budget, manifest);
    if (!coverage)
        return;

    std::println(
        "material budget coverage: {}",
        material_budget_coverage_text(*coverage));
}

void print_verifier_material_budget_bound_summary(
        VerifierObservation const& verifier) {
    if (!verifier.report)
        return;
    auto bound_summary =
        material_budget_bound_summary_from_report(*verifier.report);
    if (!bound_summary)
        return;

    std::println(
        "material budget bounds: {}",
        material_budget_bound_summary_text(*bound_summary));
    auto bound_results =
        material_budget_bound_results_from_report(*verifier.report);
    for (auto const& line : material_budget_bound_detail_lines(
             bound_results,
             bound_summary)) {
        std::println("  {}", line);
    }
}

auto first_positional_or_error(cppx::cli::Invocation const& invocation,
                               std::string_view command_name)
    -> std::expected<fs::path, std::string> {
    if (invocation.positionals.empty()) {
        return std::unexpected{
            std::format("{} requires one positional path", command_name)};
    }
    if (invocation.positionals.size() > 1) {
        return std::unexpected{
            std::format("{} accepts exactly one positional path", command_name)};
    }
    return fs::path{invocation.positionals.front()};
}

auto optional_positional_or_error(cppx::cli::Invocation const& invocation,
                                  std::string_view command_name,
                                  fs::path fallback)
    -> std::expected<fs::path, std::string> {
    if (invocation.positionals.empty())
        return fallback;
    if (invocation.positionals.size() > 1) {
        return std::unexpected{
            std::format("{} accepts at most one positional path", command_name)};
    }
    return fs::path{invocation.positionals.front()};
}

int run_artifact_summary(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "artifact summary");
    if (!path)
        return print_error("artifact summary", path.error(), invocation.has("json"));

    auto summary = artifact_summary(*path);
    auto checks = artifact_checks(summary);
    if (invocation.has("json")) {
        std::println("{}", artifact_json(summary, checks));
    } else {
        print_checks("phenotype artifact summary", checks);
    }
    return all_ok(checks) ? 0 : 1;
}

void append_option_arg(std::vector<std::string>& args,
                       cppx::cli::Invocation const& invocation,
                       std::string_view name) {
    if (auto value = invocation.value(name)) {
        auto option = std::string{"--"};
        option += name;
        args.push_back(std::move(option));
        args.push_back(std::string{*value});
    }
}

void append_path_option_arg(std::vector<std::string>& args,
                            cppx::cli::Invocation const& invocation,
                            std::string_view name) {
    if (auto value = invocation.value(name)) {
        auto option = std::string{"--"};
        option += name;
        args.push_back(std::move(option));
        args.push_back(absolute_path_string(fs::path{std::string{*value}}));
    }
}

void append_flag_arg(std::vector<std::string>& args,
                     cppx::cli::Invocation const& invocation,
                     std::string_view name) {
    if (!invocation.has(name))
        return;
    auto option = std::string{"--"};
    option += name;
    args.push_back(std::move(option));
}

void append_repeatable_arg(std::vector<std::string>& args,
                           cppx::cli::Invocation const& invocation,
                           std::string_view name) {
    for (auto const& value : invocation.values(name)) {
        auto option = std::string{"--"};
        option += name;
        args.push_back(std::move(option));
        args.push_back(value);
    }
}

auto comma_join(std::span<std::string const> values) -> std::string {
    auto result = std::string{};
    for (auto const& value : values) {
        if (!result.empty())
            result += ",";
        result += value;
    }
    return result;
}

auto artifact_verify_args(fs::path const& bundle,
                          cppx::cli::Invocation const& invocation)
    -> std::vector<std::string> {
    auto args = std::vector<std::string>{
        "exec",
        "--",
        "uv",
        "run",
        "--frozen",
        "python",
        "tools/verify_artifact_bundle.py",
        absolute_path_string(bundle),
    };

    append_path_option_arg(args, invocation, "manifest");
    append_option_arg(args, invocation, "expect-platform");
    append_flag_arg(args, invocation, "require-frame");
    append_repeatable_arg(args, invocation, "require-label");
    append_repeatable_arg(args, invocation, "require-label-contains");
    append_repeatable_arg(args, invocation, "require-role");
    append_option_arg(args, invocation, "require-disabled-count");
    append_repeatable_arg(args, invocation, "require-material-kind");
    append_repeatable_arg(args, invocation, "require-material-surface-role");
    append_flag_arg(args, invocation, "require-material-fallback");
    append_flag_arg(args, invocation, "require-material-plan");
    append_flag_arg(args, invocation, "require-material-semantic-runtime-match");
    append_repeatable_arg(args, invocation, "require-capability");
    append_repeatable_arg(args, invocation, "require-runtime-detail");
    append_repeatable_arg(args, invocation, "require-debug-detail");
    append_repeatable_arg(args, invocation, "require-pixel-region");
    append_repeatable_arg(args, invocation, "require-pixel-region-metric");
    return args;
}

auto last_nonempty_line(std::string_view text) -> std::string {
    auto end = text.size();
    while (end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r'))
        --end;
    auto begin = text.rfind('\n', end == 0 ? 0 : end - 1);
    if (begin == std::string_view::npos)
        return std::string{text.substr(0, end)};
    return std::string{text.substr(begin + 1, end - begin - 1)};
}

auto capture_artifact_verifier(fs::path const& bundle,
                               cppx::cli::Invocation const& invocation)
    -> std::expected<VerifierObservation, std::string> {
    auto root = find_repo_root(fs::current_path());
    if (!root) {
        return std::unexpected{
            "could not find phenotype repository root from current directory"};
    }

    auto spec = cppx::process::ProcessSpec{
        .program = "mise",
        .args = artifact_verify_args(bundle, invocation),
        .cwd = *root,
        .timeout = std::chrono::minutes{5},
        .env_overrides = {
            {"UV_PROJECT_ENVIRONMENT", uv_project_environment()},
        },
    };

    auto result = cppx::process::system::capture(spec);
    if (!result) {
        return std::unexpected{std::format(
            "failed to run mise/uv verifier: {}",
            cppx::process::to_string(result.error()))};
    }

    auto verifier = VerifierObservation{
        .requested = true,
        .executed = true,
        .ok = !result->timed_out && result->exit_code == 0,
        .exit_code = result->exit_code,
        .timed_out = result->timed_out,
        .stdout_tail = std::string{output_tail(result->stdout_text)},
        .stderr_tail = std::string{output_tail(result->stderr_text)},
    };

    if (!result->stdout_text.empty()) {
        try {
            verifier.report = json::parse(result->stdout_text);
        } catch (std::exception const& error) {
            verifier.report_error = error.what();
        }
    }
    if (!verifier.report && !result->stdout_text.empty()) {
        auto line = last_nonempty_line(result->stdout_text);
        if (!line.empty() && line.front() == '{') {
            try {
                verifier.report = json::parse(line);
                verifier.report_error.clear();
            } catch (std::exception const&) {
            }
        }
    }
    if (!verifier.report && !result->stdout_text.empty()
        && verifier.report_error.empty()) {
        verifier.report_error = "verifier stdout did not contain a JSON object";
    }
    return verifier;
}

int run_artifact_verify(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "artifact verify");
    if (!path)
        return print_error("artifact verify", path.error(), invocation.has("json"));

    auto root = find_repo_root(fs::current_path());
    if (!root) {
        return print_error(
            "artifact verify",
            "could not find phenotype repository root from current directory",
            invocation.has("json"));
    }

    auto spec = cppx::process::ProcessSpec{
        .program = "mise",
        .args = artifact_verify_args(*path, invocation),
        .cwd = *root,
        .timeout = std::chrono::minutes{5},
        .env_overrides = {
            {"UV_PROJECT_ENVIRONMENT", uv_project_environment()},
        },
    };

    auto result = cppx::process::system::capture(spec);
    if (!result) {
        return print_error(
            "artifact verify",
            std::format("failed to run mise/uv verifier: {}",
                        cppx::process::to_string(result.error())),
            invocation.has("json"));
    }

    if (!result->stdout_text.empty()) {
        std::print("{}", result->stdout_text);
        if (!result->stdout_text.ends_with('\n'))
            std::println("");
    }
    if (!result->stderr_text.empty()) {
        std::print(std::cerr, "{}", result->stderr_text);
        if (!result->stderr_text.ends_with('\n'))
            std::println(std::cerr, "");
    }

    if (result->timed_out)
        return result->exit_code == 0 ? 124 : result->exit_code;
    return result->exit_code;
}

void append_artifact_observation_guidance(ArtifactObservation& observation) {
    if (!observation.artifact.exists || !observation.artifact.is_directory) {
        append_unique(
            observation.likely_owner_layers,
            "artifact-bundle");
        append_unique(
            observation.suggested_actions,
            "Pass the artifact bundle directory produced by a phenotype runtime capture.");
        return;
    }
    if (!observation.artifact.snapshot_json) {
        append_unique(observation.likely_owner_layers, "snapshot-writer");
        append_unique(
            observation.suggested_actions,
            "Run the example with PHENOTYPE_ARTIFACT_DIR and PHENOTYPE_ARTIFACT_EXIT=1, then inspect snapshot.json.");
    }
    if (observation.artifact.frame_bmp) {
        // Structural frame presence is enough here; pixel metrics remain in the verifier.
    } else {
        append_unique(observation.likely_owner_layers, "frame-capture");
        append_unique(
            observation.suggested_actions,
            "Inspect frame capture support and the backend artifact writer for missing frame.bmp.");
    }
    if (!observation.artifact.platform_directory) {
        append_unique(observation.likely_owner_layers, "platform-runtime");
        append_unique(
            observation.suggested_actions,
            "Inspect platform runtime detail writers; LLM-debuggable artifacts need platform/*.json files.");
    }
    if (observation.snapshot.present && !observation.snapshot.parse_ok) {
        append_unique(observation.likely_owner_layers, "snapshot-json");
        append_unique(
            observation.suggested_actions,
            "Inspect snapshot.json for truncation or malformed JSON before debugging renderer policy.");
    }
    if (observation.snapshot.parse_ok
        && !observation.snapshot.semantic_tree_present) {
        append_unique(observation.likely_owner_layers, "semantic-tree");
        append_unique(
            observation.suggested_actions,
            "Inspect debug.semantic_tree serialization; material failures need semantic node context.");
    }
    if (observation.snapshot.parse_ok
        && observation.snapshot.material.plan_count == 0) {
        append_unique(observation.likely_owner_layers, "material-plan");
        append_unique(
            observation.suggested_actions,
            "Inspect debug.platform_runtime.details.renderer.material_plans and layout material_surface calls.");
    }
    for (auto const& layer : observation.snapshot.material.likely_layers)
        append_unique(observation.likely_owner_layers, layer);
    if (observation.verifier.executed && !observation.verifier.ok) {
        append_unique(observation.likely_owner_layers, "artifact-verifier");
        append_unique(
            observation.suggested_actions,
            "Read verifier.report.failure_summary first, then follow the reported JSON path and likely pass.");
    }
}

auto observe_artifact(fs::path bundle,
                      cppx::cli::Invocation const& invocation)
    -> ArtifactObservation {
    auto observation = ArtifactObservation{
        .bundle = std::move(bundle),
    };
    observation.artifact = artifact_summary(observation.bundle);
    observation.checks = artifact_checks(observation.artifact);
    if (!invocation.has("require-frame")) {
        for (auto& check : observation.checks) {
            if (check.name == "frame_bmp") {
                check.ok = observation.artifact.frame_bmp
                    ? observation.artifact.frame_bytes > 0
                    : true;
                check.hint.clear();
            }
        }
    }
    observation.snapshot = observe_snapshot(observation.bundle);

    auto verifier_requested =
        invocation.has("verify") || invocation.value("manifest").has_value();
    observation.verifier.requested = verifier_requested;
    if (verifier_requested) {
        if (auto verifier = capture_artifact_verifier(
                observation.bundle,
                invocation)) {
            observation.verifier = std::move(*verifier);
        } else {
            observation.verifier = VerifierObservation{
                .requested = true,
                .executed = false,
                .ok = false,
                .report_error = verifier.error(),
            };
        }
    }

    append_artifact_observation_guidance(observation);
    observation.ok = all_ok(observation.checks)
        && observation.snapshot.present
        && observation.snapshot.parse_ok
        && (!verifier_requested || observation.verifier.ok);
    return observation;
}

void print_artifact_observation(ArtifactObservation const& observation) {
    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "bundle",
         .value = path_string(observation.bundle),
         .status = observation.artifact.exists && observation.artifact.is_directory
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "snapshot",
         .value = observation.snapshot.parse_ok
            ? "parsed"
            : (observation.snapshot.present ? "parse failed" : "missing"),
         .status = observation.snapshot.parse_ok
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::fail},
        {.label = "material plans",
         .value = std::format(
             "{} plans, {} fallback, {} backdrop",
             observation.snapshot.material.plan_count,
             observation.snapshot.material.fallback_count,
             observation.snapshot.material.backdrop_sampling_count),
         .status = observation.snapshot.material.plan_count > 0
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::skip},
        {.label = "platform",
         .value = observation.snapshot.platform_name.empty()
            ? "<unknown>"
            : observation.snapshot.platform_name,
         .status = observation.snapshot.platform_runtime_present
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::skip},
        {.label = "frame",
         .value = observation.artifact.frame_bmp
            ? std::format("{} bytes", observation.artifact.frame_bytes)
            : "not present",
         .status = observation.artifact.frame_bmp
            ? cppx::terminal::StatusKind::ok
            : cppx::terminal::StatusKind::skip},
        {.label = "verifier",
         .value = observation.verifier.requested
            ? (observation.verifier.executed
                ? std::format("exit {}", observation.verifier.exit_code)
                : observation.verifier.report_error)
            : "not requested",
         .status = !observation.verifier.requested
            ? cppx::terminal::StatusKind::skip
            : (observation.verifier.ok ? cppx::terminal::StatusKind::ok
                                       : cppx::terminal::StatusKind::fail)},
    };
    std::println("phenotype observe artifact");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    print_snapshot_material_budget(
        observation.snapshot.material.executor_budget);
    print_verifier_manifest_summary(observation.verifier);
    print_verifier_material_budget_coverage(observation.verifier);
    print_verifier_material_budget_bound_summary(observation.verifier);
    print_verifier_material_budget(observation.verifier);
    if (!observation.suggested_actions.empty()) {
        std::println("suggestions:");
        for (auto const& suggestion : observation.suggested_actions)
            std::println("  - {}", suggestion);
    }
}

int run_observe(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "observe");
    if (!path)
        return print_error("observe", path.error(), invocation.has("json"));

    auto observation = observe_artifact(*path, invocation);
    if (invocation.has("json")) {
        std::println("{}", artifact_observation_json(observation));
    } else {
        print_artifact_observation(observation);
    }
    return observation.ok ? 0 : 1;
}

} // namespace phenotype_cli::artifacts
