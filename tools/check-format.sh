#!/usr/bin/env bash
set -euo pipefail

root=$(git rev-parse --show-toplevel)
cd "$root"

intron=${INTRON:-$(mise which intron)}
exon=${EXON:-$(mise which exon)}

find . -name exon.toml -not -path '*/.exon/*' -print | sort | while IFS= read -r manifest; do
  dir=$(dirname "$manifest")
  if [ -d "$dir/src" ]; then
    echo "Formatting $dir"
    (cd "$dir" && "$intron" exec -- "$exon" fmt)
  fi
done

if ! git diff --quiet --; then
  echo "exon fmt changed files. Review and stage the formatting changes before committing." >&2
  git diff --stat >&2
  exit 1
fi
