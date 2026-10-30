#!/usr/bin/env bash
# Convenience alias — same as sync_ledger_app_public.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec "$ROOT/scripts/sync_ledger_app_public.sh" "$@"
