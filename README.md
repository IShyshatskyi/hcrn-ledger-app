# hcrn-ledger-app

Public **packaging** stub for the **HCRN Ledger Nano app** — cold-storage signing
intended to work with Filament / Filament-testnet wallets.

> **This is not Filament.** Do not look for Ledger builds under
> [`filament-testnet/releases`](https://github.com/IShyshatskyi/filament-testnet/releases).
> Filament-testnet is the light-client packaging repo only.

## Status (2026-10)

| Item | State |
|------|--------|
| Public source tree | **Not synced yet** — this README is a placeholder |
| GitHub Releases / `.hex` / sideload bundles | **None** — download unavailable |
| Product notes | Marketing [Filament Hardware](https://heaviest-chain.netlify.app/Filament_Hardware.html) |

Historical / internal tree name in the private monorepo may still appear as
`ledger-app-shisha/` — that path name is legacy; the public product name is **HCRN Ledger App**.

## Planned layout (when synced)

```text
hcrn-ledger-app/
  README.md
  LICENSE            ← MIT (intended)
  Makefile / build.sh
  src/               ← BOLOS app sources
  docs/              ← load on Nano S+/X, test vectors
```

Build and load instructions will match Ledger’s current BOLOS SDK docs when source
is published. Prefer building on a machine or VM you control (same isolation idea as
[Build Filament in isolation](https://heaviest-chain.netlify.app/Build_Filament.html)).

## Relation to other repos

| Repo | Product |
|------|---------|
| [`filament-testnet`](https://github.com/IShyshatskyi/filament-testnet) | MMR light client |
| [`stratum-worker-testnet`](https://github.com/IShyshatskyi/stratum-worker-testnet) | Pool worker stub |
| **`hcrn-ledger-app`** (this repo) | Ledger app |
| Private monorepo `ledger-app-shisha/` | Upstream until sync |

## Releases

**GitHub Releases for this product belong only on this repository** once artifacts exist.
Empty Releases here means “no binary drop yet,” not “go look at Filament.”

## Licence

MIT (intended). Licence file will ship with the first source sync.

## Links

- Marketing Downloads: https://heaviest-chain.netlify.app/Downloads.html  
- Filament Hardware: https://heaviest-chain.netlify.app/Filament_Hardware.html  
- Filament-testnet (light client, separate): https://github.com/IShyshatskyi/filament-testnet  
