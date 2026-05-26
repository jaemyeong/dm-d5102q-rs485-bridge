#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOARDS=("atoms3-lite" "atom-lite")

for board in "${BOARDS[@]}"; do
  mkdir -p "$ROOT/firmware/$board/src" "$ROOT/firmware/$board/data"
  rsync -a --delete "$ROOT/shared/src/" "$ROOT/firmware/$board/src/"
  rsync -a --delete "$ROOT/shared/data/" "$ROOT/firmware/$board/data/"
done
