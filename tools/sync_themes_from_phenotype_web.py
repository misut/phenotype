#!/usr/bin/env python3
"""Mirror phenotype-web's theme snapshots into phenotype's fixtures and
the raw-string literals in tests/test_token_roundtrip.cpp.

Run from the phenotype repo root:

    python tools/sync_themes_from_phenotype_web.py

Requires the `gh` CLI to be authenticated against misut/phenotype-web
(the repo is private). Files are pulled from the latest commit on
phenotype-web's main branch and written into both
`tests/fixtures/themes/<theme>.theme.json` and the matching
`<THEME>_THEME_JSON` raw-string literal in
`tests/test_token_roundtrip.cpp`.

The script is idempotent: re-running with no upstream change exits
without touching either side. After it makes edits, run `exon test`
to confirm the parity guard and round-trip cases stay green, then
commit the diff as the phenotype-side mirror PR.
"""

import base64
import re
import subprocess
import sys
from pathlib import Path

THEMES = ["default", "dark", "warm", "dense"]
PHENOTYPE_WEB_REPO = "misut/phenotype-web"
SNAPSHOT_PATH_TEMPLATE = "src/theme/__snapshots__/{theme}.theme.json"

REPO_ROOT = Path(__file__).resolve().parent.parent
FIXTURES_DIR = REPO_ROOT / "tests" / "fixtures" / "themes"
ROUNDTRIP_CPP = REPO_ROOT / "tests" / "test_token_roundtrip.cpp"


def fetch_snapshot(theme: str) -> str:
    """Download the snapshot file content via `gh api`."""
    path = SNAPSHOT_PATH_TEMPLATE.format(theme=theme)
    result = subprocess.run(
        [
            "gh",
            "api",
            f"repos/{PHENOTYPE_WEB_REPO}/contents/{path}",
            "--jq",
            ".content",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    return base64.b64decode(result.stdout.strip()).decode("utf-8")


def write_fixture(theme: str, content: str) -> bool:
    """Write theme JSON to fixtures dir. Return True if changed."""
    out_path = FIXTURES_DIR / f"{theme}.theme.json"
    existing = out_path.read_text() if out_path.exists() else None
    if existing == content:
        return False
    out_path.write_text(content)
    return True


def update_cpp_raw_string(theme: str, content: str, cpp_text: str) -> str:
    """Replace the `R"json(...)json"` block for the given theme."""
    var_name = f"{theme.upper()}_THEME_JSON"
    pattern = rf'(constexpr char const\* {var_name} = R"json\()(.*?)(\)json";)'
    new_text, count = re.subn(
        pattern,
        lambda m: m.group(1) + content + m.group(3),
        cpp_text,
        flags=re.DOTALL,
    )
    if count == 0:
        sys.exit(
            f"error: could not find raw-string for {var_name} in "
            f"{ROUNDTRIP_CPP.relative_to(REPO_ROOT)}",
        )
    return new_text


def main() -> int:
    print(f"Pulling theme snapshots from {PHENOTYPE_WEB_REPO}/main...")

    snapshots: dict[str, str] = {}
    for theme in THEMES:
        snapshots[theme] = fetch_snapshot(theme)
        print(f"  fetched {theme}.theme.json ({len(snapshots[theme])} bytes)")

    fixture_changed: list[str] = []
    for theme, content in snapshots.items():
        if write_fixture(theme, content):
            fixture_changed.append(theme)

    cpp_text = ROUNDTRIP_CPP.read_text()
    new_cpp_text = cpp_text
    for theme, content in snapshots.items():
        new_cpp_text = update_cpp_raw_string(theme, content, new_cpp_text)
    cpp_changed = new_cpp_text != cpp_text
    if cpp_changed:
        ROUNDTRIP_CPP.write_text(new_cpp_text)

    if not fixture_changed and not cpp_changed:
        print("\nAll snapshots already in sync with phenotype-web/main.")
        return 0

    if fixture_changed:
        print(f"\nUpdated fixture files: {', '.join(fixture_changed)}")
    if cpp_changed:
        print(
            f"Updated raw-string literals in "
            f"{ROUNDTRIP_CPP.relative_to(REPO_ROOT)}",
        )
    print("\nNext steps: review the diff, run `exon test`, commit, push.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
