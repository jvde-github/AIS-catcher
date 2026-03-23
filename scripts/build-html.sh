#!/usr/bin/env bash
# Builds the frontend and bakes it into Source/Application/WebDB.cpp.
# Usage: bash scripts/build-html.sh [repo-root]
set -euo pipefail

REPO_ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"

echo "==> Building frontend..."
cd "$REPO_ROOT/frontend"
npm ci --silent
npm run build

echo "==> Generating WebDB.cpp..."
cd "$REPO_ROOT"
bash scripts/build-web-db.sh
