# hcrn-ledger-app

Public **packaging** for the **HCRN Ledger Nano app** — cold-storage signing
intended to work with Filament / Filament-testnet wallets.

> **This is not Filament.** Do not look for Ledger builds under
> [`filament-testnet/releases`](https://github.com/IShyshatskyi/filament-testnet/releases).
> Filament-testnet is the light-client packaging repo only.

Historical / internal tree name in the private monorepo is `ledger-app-shisha/` —
that path name is legacy; the public product name is **HCRN Ledger App**.

## Status

| Item | State |
|------|--------|
| Sync from private monorepo | `./scripts/sync_ledger_app_public.sh` |
| GitHub Releases / `.hex` / sideload | **`v0.0.1-dev`** — Nano S+ `hcrn-ledger-nano_sp.hex` (+ `.elf`, `.sha256`). Unsigned sideload / developer mode only — **not** Ledger Live Store. Rebuild via Actions → `ledger-app-build`. |
| Product notes | [Filament Hardware](https://heaviest-chain.netlify.app/Filament_Hardware.html) |

## Sync from the private monorepo (maintainers)

Same pattern as [`filament-testnet`](https://github.com/IShyshatskyi/filament-testnet)
`scripts/sync_filament_public.sh`: run **inside this public clone**, point at a local
shisha checkout that contains `ledger-app-shisha/`.

```bash
cp scripts/local/upstream.env.example scripts/local/upstream.env
# set SHISHA=/absolute/path/to/shisha

./scripts/sync_ledger_app_public.sh --status
./scripts/sync_ledger_app_public.sh --dry-run
./scripts/sync_ledger_app_public.sh --apply

./scripts/check_public_boundary.sh
```

`sync_from_shisha.sh` is an alias for the same script.

What `--apply` does:

1. `rsync` `$SHISHA/ledger-app-shisha/` → this repo root  
2. Remaps `Makefile` `APPNAME` from historical `Shisha` → `HCRN`  
3. Restores packaging overlays (`README.md`, `scripts/`, `LICENSE`, …) from `keep_public.txt`  
4. Writes `VERSION` (`SYNCED_FROM_SHISHA=<git sha>`)  
5. Runs `check_public_boundary.sh`

## Build (after sync)

Requires Ledger’s BOLOS SDK (Docker builder recommended). Prefer building on a
machine or VM you control — same isolation idea as
[Build Filament in isolation](https://heaviest-chain.netlify.app/Build_Filament.html).

```bash
# Example (Nano S inside Ledger’s app-builder image):
# docker run --rm -v "$(pwd):/app" -e BOLOS_SDK=$NANOS_SDK \
#   ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder make
./build.sh   # if present after sync
```

## Releases

**GitHub Releases for this product belong only on this repository.**  
Do not look under Filament-testnet Releases for Ledger artifacts.

Current: [v0.0.1-dev](https://github.com/IShyshatskyi/hcrn-ledger-app/releases/tag/v0.0.1-dev) (`hcrn-ledger-nano_sp.hex`).

## Relation to other repos

| Repo | Product |
|------|---------|
| [`filament-testnet`](https://github.com/IShyshatskyi/filament-testnet) | MMR light client |
| [`stratum-worker-testnet`](https://github.com/IShyshatskyi/stratum-worker-testnet) | Pool worker |
| **`hcrn-ledger-app`** (this repo) | Ledger app |
| Private monorepo `ledger-app-shisha/` | Upstream source of truth |

## Licence

MIT — see [`LICENSE`](LICENSE).
