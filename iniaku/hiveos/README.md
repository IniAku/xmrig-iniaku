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
| Extra config arguments | optional xmrig options, e.g. `--threads=6`; `lan=<node LAN IP>` if the pool node is in your LAN; a fallback pool (below) |

Fallback pool (optional): the Epic network takes turns between RandomX, ProgPoW and Cuckoo, so
an Epic RandomX miner is idle about half of the time. With a fallback pool the CPU mines it
during the other turns and returns to epic.iniaku.my.id as soon as the RandomX turn starts.
Example extra config arguments:

```
fallback=xmr-eu1.nanopool.org:14433 fallback-user=<your XMR address> fallback-algo=rx/0
```

`fallback-pass=` and `fallback-tls=0|1` (default 1) are optional. Time on the fallback pool
carries a 2% fee; time on epic.iniaku.my.id is always 0%.

- Stats come from the xmrig HTTP API on 127.0.0.1:60060.
- HiveOS runs miners as root, so xmrig sets up huge pages itself.
