# Claude Design Integration

Status: **proposal** (planning document, no code yet).
Last updated: 2026-04-24.

## 1. Context

[Claude Design][cd-news], an Anthropic Labs product launched on 2026-04-17 and
powered by Claude Opus 4.7, reads a linked code repository to extract a
design system (colors, typography, components) and then lets users iterate
on prototypes that stay consistent with that system. The tool currently
supports a single input path: **"Link a code repository"** — and recommends
linking a specific subdirectory rather than a large monorepo ([Claude
Help Center][cd-help]).

phenotype is a C++23 cross-platform UI framework. Its rendering stack (WebGPU
via Dawn, native Metal/D3D12/Vulkan) and Iced/Elm-style application model
(`Msg`, `State`, `update`, `view`) are deliberately isolated from any web
ecosystem. As a direct consequence, Claude Design has no natural way to
ingest phenotype today: there is no React component tree to read, no CSS
variables to extract, no package the tool recognizes.

This document defines the integration strategy and the prerequisite work
needed on both sides before Claude Design can meaningfully collaborate on
phenotype UIs.

## 2. Current State (audit summary)

See the audit tables in the companion plan for detail. Highlights:

- **Theme** ([`src/phenotype.types.cppm:43-63`](../src/phenotype.types.cppm))
  has 18 fields: 10 color, 7 typography, 1 layout (`max_content_width`). All
  fields are mandatory in [`theme_from_json`][theme_json]; partial overrides
  are not supported.
- **No tokens exist** for radius, spacing scale, shadow, motion, semantic
  colors (success/warning/danger), state variants (hover/active/disabled/
  error), or dark-mode pairs.
- **Widgets hardcode** the missing layers. Radius values `{4, 6, 8, 3, 8, 4}`
  for `button / code / card / checkbox / radio / text_field` are literals
  in the widget source; so are most paddings and gaps. A `Theme` change
  cannot fix a "button radius looks wrong" problem today — only a source
  edit can.
- **State variants** exist only at the `LayoutNode` level: `hover_background`
  and `hover_text_color` fields, plus a paint-side focus override
  (`border_color = accent`, `border_width = 2`). There is no Theme-level
  token for hover/active/disabled/error colors.
- **JSON schema** is 2-level and phenotype-specific: Color as
  `{r, g, b, a}` integers, scalars as numbers. It does not match DTCG,
  Figma Tokens, or Tailwind config shapes. Any external design tool needs
  an adapter.
- **Examples**: `hello` (19 lines), `counter` (45 lines), `native`
  (382 lines). Only `native` exercises enough widgets to serve as a
  meaningful visual benchmark.

## 3. Chosen strategy

**phenotype mirrors a React reference implementation.**

A new, separate repository `misut/phenotype-web` holds the ground-truth
React + TypeScript implementation of the phenotype widget API. Claude Design
(and future design tools) link to this repo. The C++ `phenotype` project
then reproduces the visual and API surface of each widget that is finalized
there.

Rationale:

- Claude Design reads repositories; a React repo is the shape it expects.
- The iteration loop "prototype in React → finalize → port to C++" is
  dramatically cheaper than "rebuild phenotype → screenshot → tweak C++".
- Non-Anthropic tools that target the web ecosystem (Figma plugins,
  Storybook-native tools, future design agents) inherit the same integration
  for free.
- The `phenotype/docs/` site is already a phenotype-built WASM app
  (dogfooding). A React counterpart gives an automatic parity benchmark.

Alternatives considered and rejected for now:

- **DTCG JSON bridge only**: token-only, cannot convey components, and
  Claude Design infers components not just tokens.
- **Custom spec + converter**: incompatible with the "repo link" intake;
  design tools would not read it.
- **MCP server**: Claude Design has not yet published an MCP integration
  surface (Anthropic: "over the coming weeks, we'll make it easier to build
  integrations"). Revisit when public.

### Repository contract

- `misut/phenotype-web` (new): React + TypeScript + CSS variables. Uses
  conventional React patterns (hooks, state); internals are not mirrored
  from the C++ Iced/Elm model. Only **widget API shape and visual
  output** must match `phenotype`. Tech stack: Vite + React 18 + TypeScript
  + Storybook + Playwright.
- `misut/phenotype` (this repo): downstream. Every widget change follows
  the rule _"land in `phenotype-web` first, then in `phenotype`"_. The
  Theme JSON pipeline is **one-way**: `phenotype-web` exports a theme that
  `phenotype` consumes.

### Scope: Core 6 widgets first

MVP covers `Text / Button / TextField / Card / Column / Row`. Remaining
widgets (`Link / Code / Checkbox / Radio / Image / Scaffold / ListItems /
Divider / Spacer / Box / SizedBox / Surface`) follow in M6.

## 4. Prerequisite roadmap

### Milestone 0 — Token foundation in `phenotype` ✅

Status: **complete** (PRs [#160](https://github.com/misut/phenotype/pull/160)
M0-2, [#163](https://github.com/misut/phenotype/pull/163) M0-3,
[#164](https://github.com/misut/phenotype/pull/164) examples cppx pin
follow-up).

This milestone happened **entirely in the C++ repo** and was the
prerequisite for the Core 6 port (M4). It touched nearly every widget.

| ID | Task | Files | Status |
|---|---|---|---|
| **A1** | Allow partial overrides in `theme_from_json` — merge incoming JSON on top of defaults instead of requiring all fields | [`src/phenotype.theme_json.cppm`](../src/phenotype.theme_json.cppm), tests | ✅ M0-2: `txn::Mode::Partial` |
| **A2** | Accept color strings in Theme JSON alongside the `{r,g,b,a}` object form — support `"#rrggbb"`, `"#rrggbbaa"`, and `"rgba(r,g,b,a)"` | [`src/phenotype.theme_json.cppm`](../src/phenotype.theme_json.cppm) | ✅ M0-2: ADL `txn_from_value` hook for `Color` |
| **A3** | Introduce a radius scale on `Theme`: `radius_sm / radius_md / radius_lg / radius_full` | [`src/phenotype.types.cppm`](../src/phenotype.types.cppm), widget callsites in [`src/phenotype.cppm`](../src/phenotype.cppm) | ✅ M0-3: 4 new fields, widget literals replaced (button / text_field / code / card; checkbox=3 / radio=8 stay literal — see Out of scope) |
| **A4** | Introduce a spacing scale on `Theme`: `space_xs / space_sm / space_md / space_lg / space_xl` (+ `space_2xl`) with phenotype-current values captured as the defaults | [`src/phenotype.types.cppm`](../src/phenotype.types.cppm), widget callsites | ✅ M0-3: 6 new fields, widget/layout literals replaced (column / row / scaffold / card / list_items / item / button padding_h / text_field padding) |
| **A5** | Introduce state color tokens on `Theme`: hover/active/disabled/error pairs (`*_bg`, `*_fg`) derived from accent/foreground/border by default, overridable in JSON | [`src/phenotype.types.cppm`](../src/phenotype.types.cppm), paint layer ([`src/phenotype.paint.cppm`](../src/phenotype.paint.cppm)), widget emitters | ✅ M0-3: 12 new state fields incl. `state_focus_ring{,_width}` consumed by `paint.cppm`; button hover binds to `state_hover_bg`; `state_active_bg` defaults to `accent_strong`; disabled / error tokens shipped (no widget consumer yet) |
| **A6** | Add semantic color tokens for downstream widgets (`semantic_{success,warning,info,error}_{bg,fg,border}`) | [`src/phenotype.types.cppm`](../src/phenotype.types.cppm) | ✅ M0-3: 12 new fields with phenotype-web defaults; no widget consumers yet, ready for Toast / Banner in a later milestone |

Milestone 0 completion: `exon test` passes on five CI targets (macOS
native, macOS wasi, ubuntu wasi, windows native, docs wasi); the
`native`, `counter`, and `hello` examples render with the new default
Tiffany palette (the `examples/native` `showcase_theme()` override was
removed in M0-3 so the example tracks the default theme); a JSON theme
containing only `{"accent": "#0abab5"}` (string form) or
`{"radius_md": 6}` is accepted and merged on top of defaults.

#### Tooling lift

- `cppx` 1.7.3 → 1.9.0 ([cppx#101](https://github.com/misut/cppx/pull/101))
  to extend `cppx::reflect::detail::to_tuple`'s field cap from 24 to 64.
  Theme is now 54 fields, which the previous limit blocked.
- `txn` `main` (PR [#68](https://github.com/misut/txn/pull/68)) added
  partial-mode deserialization and the ADL customization hook.
  phenotype consumed it via a temporary `[dependencies.path]
  txn = "../txn"` declaration, switched back to a versioned dep in U-2
  once `txn` v0.7.0 was released.

#### Out of scope (deferred to later milestones)

The following design tokens that exist in `phenotype-web` are
intentionally **not** in M0. Each requires changes to the native render
pipeline (new `Cmd` opcodes, font / animation infrastructure, layout
DSL extensions) that are out of proportion with M0's "passive token
plumbing" goal.

| Token group | Why deferred |
|---|---|
| **Motion** (duration / easing / transition) | phenotype has no animation system; tween / interpolation infrastructure is its own milestone |
| **Elevation / shadow** | Would need a new `Cmd::DrawShadow` opcode plus shader changes in all four backends (Metal / D3D12 / Vulkan / WebGPU) |
| **Font weight scale** | CoreText / DirectWrite each need a weight-aware glyph cache; `Cmd` and `LayoutNode` would gain a weight field |
| **Breakpoints** | The widget DSL has no responsive primitives today |
| **Font family (Pretendard)** | Each native text stack needs a font-loading path; bundling the binary is its own conversation |
| **Per-widget radius / spacing extras** | `widget::checkbox` (radius=3), `widget::radio` (radius=8), `layout::list_items` (gap=6), `widget::button` (padding_v=6), `layout::scaffold` (header padding_v=48) keep their literals; introducing one-off tokens (`radius_xs`, `space_3xs`, etc.) would inflate Theme without a clear reuse story |
| **`themeToPhenotypeJson` round-trip CI** | phenotype-web side dump pipeline is a separate task; the manual round-trip works today |

Risk: every widget touched the changed Theme structure. Mitigation
applied: A1 and A2 landed first as non-breaking PRs, then A3-A6 went
in as a single PR with mechanical token replacement and `exon test` on
five CI targets as the regression gate.

## 5. Integration implementation roadmap

### Milestone 1 — `phenotype-web` bootstrap

- **B1**: Create `misut/phenotype-web`. Vite + React + TypeScript + Storybook
  + Playwright. Conventional Commits, same branch prefix policy as
  `phenotype`.
- **B2**: Define the Theme system as CSS custom properties at `:root`, with
  the phenotype-18 fields plus the M0 extensions (radius scale, spacing
  scale, state colors). Ship a `PhenotypeThemeProvider` React component
  that injects variables.

Done when: `pnpm storybook` runs locally, a "Theme" story renders a swatch
grid from CSS variables, Playwright visual baseline is committed.

### Milestone 2 — Core 6 widgets in React

- **B3**: Implement `Text`, `Button`, `TextField`, `Card`, `Column`, `Row`
  in `phenotype-web`. API mirrors the phenotype C++ widget signatures
  (prop names, default behavior). Each widget exposes a Storybook story
  per visual state (default / hover / focus / active / disabled / error,
  where applicable).

Done when: the 6 Storybook stories pass Playwright snapshot tests across
three themes (default, a "warm" variant, and a "dense" variant with
tightened spacing).

### Milestone 3 — Theme pipeline glue

- **B4**: In `phenotype-web`, ship `themeToPhenotypeJson(cssVars)` — a
  pure TypeScript utility that reads computed CSS variables and emits a
  JSON document compatible with `phenotype::theme_from_json` (post-A1/A2,
  so partial and hex are OK).
- **C1**: Set up CI in `phenotype-web` that runs the utility against each
  Storybook theme, stores the JSON as an artifact, and posts it to
  `phenotype` through a small `phenotype/tools/` consumer test that
  asserts the JSON parses without error.

Done when: a pushed change to a CSS variable in `phenotype-web` produces
a regenerated `tokens.json` artifact, and the `phenotype` consumer test
confirms the file parses into a valid Theme.

### Milestone 4 — phenotype mirrors Core 6

- For each Core 6 widget, adjust the C++ implementation so that its
  visual and API surface matches the React version established in M2.
  In practice this is mostly a no-op for API (the React side was written
  to match phenotype's existing API) and a mechanical adjustment for
  visuals (pick up exact radius / spacing / state tokens from the React
  theme defaults).

Done when: the `native` example renders each Core 6 widget in a state
that visually matches its React Storybook story within pixel-diff
tolerance. The comparison uses the native debug snapshot plus a
browser-side Playwright snapshot.

### Milestone 5 — Claude Design end-to-end

- Link `misut/phenotype-web` to Claude Design (subdirectory link if the
  repo has non-component content; otherwise repo root).
- Run a full loop: describe a change in Claude Design → apply changes to
  `phenotype-web` (via Claude Code handoff) → regenerate `tokens.json` →
  verify `phenotype` picks up the new visuals.

Done when: a design change initiated in Claude Design reaches the
`phenotype` native build with no manual JSON editing.

### Milestone 6 — Remaining widgets

- **B7**: Extend `phenotype-web` with `Link / Code / Checkbox / Radio /
  Image / Scaffold / ListItems / Divider / Spacer / Box / SizedBox /
  Surface`. One PR per widget family; each enters `phenotype` via the
  same "React first" rule.

### Milestone 7 — Showcase apps

- **B8**: Port `examples/counter` and `examples/native` to
  `phenotype-web/examples/` as React apps. These function as the
  integration-test surface for Claude Design: a realistic, non-trivial
  composition of widgets and theme tokens.

## 6. Open questions

- **Claude Design MCP surface**: if and when Anthropic publishes a
  programmatic integration surface for Claude Design, re-evaluate adding
  a `phenotype-mcp` server that exposes the current Theme, widget
  catalogue, and (eventually) the live app state. Keep the current
  roadmap unblocked on that.
- **DTCG adoption**: once the M3 pipeline is stable, consider adding a
  DTCG-shaped export alongside `tokens.json` to improve interop with
  Figma Tokens and other non-Claude tools. Not on the critical path.
- **Shadow / motion tokens**: intentionally deferred. Add when the first
  widget that actually needs them (likely `Tooltip`, `Popover`, or
  animated `Toast`) enters the roadmap.
- **Mobile-native mirror parity**: the iOS / Android builds currently
  share the C++ widget pipeline, so they inherit Theme changes for free.
  Confirm visual parity on Android Vulkan after M4; iOS has no build
  today.
- **Semantic colors (success/warning/danger/info)**: deferred until the
  first widget needs them (likely Toast / Banner). Not blocking MVP.

## 7. References

- Anthropic: [Introducing Claude Design][cd-news]
- Claude Help Center: [Get started with Claude Design][cd-help]
- TechCrunch launch coverage: [Anthropic launches Claude Design (2026-04-17)][cd-tc]
- phenotype [`README.md`](../README.md) · [`docs/ARCHITECTURE.md`](ARCHITECTURE.md) · [`docs/DEBUG_WORKFLOW.md`](DEBUG_WORKFLOW.md)
- Planning notes: `~/.claude/plans/phenotype-claude-design-peppy-rose.md`

[cd-news]: https://www.anthropic.com/news/claude-design-anthropic-labs
[cd-help]: https://support.claude.com/en/articles/14604416-get-started-with-claude-design
[cd-tc]: https://techcrunch.com/2026/04/17/anthropic-launches-claude-design-a-new-product-for-creating-quick-visuals/
[theme_json]: ../src/phenotype.theme_json.cppm
