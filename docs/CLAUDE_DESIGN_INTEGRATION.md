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

### Milestone 0 — Token foundation in `phenotype`

This milestone happens **entirely in the C++ repo** and must land before
the Core 6 port (M4) makes sense. It is the riskiest milestone because it
touches nearly every widget.

| ID | Task | Files | Done when |
|---|---|---|---|
| **A1** | Allow partial overrides in `theme_from_json` — merge incoming JSON on top of defaults instead of requiring all fields | [`src/phenotype.theme_json.cppm`](../src/phenotype.theme_json.cppm), tests | A JSON with only `{"accent": …}` parses successfully and yields a Theme with `accent` overridden and all other fields at default |
| **A2** | Accept color strings in Theme JSON alongside the `{r,g,b,a}` object form — support `"#rrggbb"`, `"#rrggbbaa"`, and `"rgba(r,g,b,a)"` | [`src/phenotype.theme_json.cppm`](../src/phenotype.theme_json.cppm) | Both forms round-trip; `theme_to_json` still emits the object form by default for backward compatibility |
| **A3** | Introduce a radius scale on `Theme`: `radius_sm / radius_md / radius_lg / radius_full` (values TBD during M2, but hardcoded default set replaces the current literals exactly) | [`src/phenotype.types.cppm`](../src/phenotype.types.cppm), widget callsites in [`src/phenotype.cppm`](../src/phenotype.cppm) | All `widget::*` and `layout::card` calls reference `theme.radius_*`; no `border_radius` literals remain in widget source |
| **A4** | Introduce a spacing scale on `Theme`: `space_xs / space_sm / space_md / space_lg / space_xl` with phenotype-current values captured as the defaults | [`src/phenotype.types.cppm`](../src/phenotype.types.cppm), widget callsites | `padding` / `gap` values in Core 6 widgets read `theme.space_*`; other widgets may continue to hardcode until M6 |
| **A5** | Introduce state color tokens on `Theme`: hover/active/disabled/error pairs (`*_bg`, `*_fg`) derived from accent/foreground/border by default, overridable in JSON | [`src/phenotype.types.cppm`](../src/phenotype.types.cppm), paint layer ([`src/phenotype.paint.cppm`](../src/phenotype.paint.cppm)), widget emitters | `LayoutNode::hover_*` inline fields are assigned from `theme.hover_bg` / `theme.hover_fg` at widget emission time; `disabled` rendering path exists and covers Button and TextField |

Milestone 0 completion: `exon test` passes on all platforms; the `native`
example renders identically to today; a JSON theme containing only
`accent` and `space_md` overrides is accepted and reflected.

Risk: every widget touches the changed Theme structure. Mitigation: land
A1 and A2 first (non-breaking), then A3/A4/A5 as a single PR with a
mechanical migration and a "no visual regression" check using the debug
snapshot contract from [`docs/DEBUG_WORKFLOW.md`](DEBUG_WORKFLOW.md).

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
