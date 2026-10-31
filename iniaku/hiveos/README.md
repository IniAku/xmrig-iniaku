# HiveOS flight sheet — XMRig iniaku (RandomX rx/epic, epic.iniaku.my.id)

| Field | Value |
|---|---|
| Coin | EPIC (add it as a custom coin if missing) |
| Wallet | your Epic address, e.g. `es...@epicbox.iniaku.my.id` |
| Pool | Configure in miner |
| Miner | Custom |
| Installation URL | `https://epic.iniaku.my.id/storage/hiveos/iniaku_xmrig-<version>.tar.gz` |
| Hash algorithm | `randomx` |
| Wallet and worker template | `%WAL%.%WORKER_NAME%` |
| Pool URL | `us.epic.iniaku.my.id` / `id.epic.iniaku.my.id` / `au.epic.iniaku.my.id` (anything else = auto) |
| Pass | `x` |
| Extra config arguments | optional xmrig options, e.g. `--threads=6`; `lan=<node LAN IP>` if the pool node is in your LAN |

- Stats come from the xmrig HTTP API on 127.0.0.1:60060.
- HiveOS runs miners as root, so xmrig sets up huge pages itself.
