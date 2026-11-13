XMRig — epic.iniaku.my.id edition (Epic Cash RandomX)
=====================================================

XMRig 6.24 with Epic Cash RandomX ("rx/epic") and the native Epic stratum protocol, for the
epic.iniaku.my.id pool (ID / US / AU servers, TLS port 3334).

Mining on epic.iniaku.my.id: 0% fee.
Mining on any other pool (a fallback pool, or other pools only): 1% fee, see "Fee" below.

Quick start
-----------
1. Open config.json and set your Epic address and a worker name:
       "user": "<your Epic address>.<worker name>"
   or pass it on the command line:  -u <your Epic address>.<worker name>
2. Linux:   ./start.sh        (or ./xmrig)
   Windows: start.cmd         (or xmrig.exe)
3. Check your worker at https://epic.iniaku.my.id

Windows: antivirus (Microsoft Defender)
---------------------------------------
Like every XMRig build, xmrig.exe is flagged as a "CoinMiner" by Defender and most
antivirus programs and is deleted right after unpacking or starting. Before unpacking,
add exclusions for the miner folder AND the miner process. In PowerShell as administrator
(adjust the folder):
    Add-MpPreference -ExclusionPath "C:\Miners\xmrig-iniaku"
    Add-MpPreference -ExclusionProcess "C:\Miners\xmrig-iniaku\xmrig.exe"
or: Windows Security -> Virus & threat protection -> Manage settings -> Exclusions ->
Add an exclusion -> Folder (and Process: xmrig.exe).
If the file was already removed: Windows Security -> Protection history -> Restore, or unpack
the zip again after adding the exclusions.

Region
------
"region": "auto" (default) measures the connection time to id / us / au and uses the
closest server; if it goes down XMRig switches to the next one automatically.
Force a region in config.json ("region": "us") or on the command line: --region us
Valid values: auto, id, us, au.

Fallback pool: no idle time between Epic RandomX turns
------------------------------------------------------
Epic takes turns per block: RandomX, ProgPoW, RandomX, ProgPoW, ... and Cuckoo every 25th
block, each turn about one minute, and only the RandomX turn can produce a RandomX block.
Add a second pool and the CPU mines it during the ProgPoW/Cuckoo turns, switching back to
epic.iniaku.my.id the moment the RandomX turn starts:

    "pools": [
        { "user": "<your Epic address>.<worker>", "pass": "x" },
        { "url": "xmr-eu1.nanopool.org:14433", "user": "<your XMR address>", "algo": "rx/0", "tls": true }
    ]

- The first entry without "url" (or with an *.epic.iniaku.my.id url) is epic.iniaku.my.id;
  every pool with another url is a fallback pool (several = failover, in order).
- Any CPU algorithm XMRig supports works ("algo": "rx/0", "rx/wow", "gr", "cn/r", ...).
  Set "algo" (or "coin") so the miner knows it before the first job.
- Two RandomX datasets stay in memory (rx/epic + the fallback's): about 4.6 GB RAM instead of
  2.3 GB, so switching is instant. With huge pages set vm.nr_hugepages=2600 for both.
- Without an epic.iniaku.my.id entry the miner works like a normal XMRig on your pools.

Fee
---
Time spent on epic.iniaku.my.id is always free. Time spent on other pools carries a 1% fee:
after 99 minutes on other pools the miner mines 1 minute for the fee, with the same algorithm
(rx/epic -> epic.iniaku.my.id, anything else -> MoneroOcean, paid in XMR), and never during
an Epic RandomX turn. The log shows "fee 1% started" / "fee finished".

Faster mining: huge pages (administrator / root)
-----------------------------------------------
RandomX is noticeably faster with huge pages; Linux can additionally use the MSR mod.
  Windows: right-click start.cmd -> "Run as administrator" once. XMRig grants the
           "Lock pages in memory" right; after a reboot the log shows "huge pages 100%".
           (The MSR mod is not available on Windows in this build: its WinRing0 driver
           is blocked by Windows as a vulnerable driver and is not included.)
  Linux:   run as root (sudo ./start.sh) for huge pages + MSR, or enable huge pages once:
           sudo sysctl -w vm.nr_hugepages=1280
The miner also works without them, just slower ("huge pages 0%" in the log).

Notes
-----
- Epic rotates the algorithm per block (ProgPoW / RandomX / Cuckoo). Without a fallback pool
  the miner pauses during the ProgPoW/Cuckoo turns ("waiting for the Epic RandomX turn") and
  resumes when the RandomX turn starts, so the CPU does not burn power on work that cannot
  become a RandomX block.
- About 2.3 GB RAM is used for the RandomX dataset ("fast" mode), 4.6 GB with a RandomX
  fallback pool. On machines with little RAM set "mode": "light" in the "randomx" section
  (much slower).
- Keys while running: h = hashrate, p = pause, r = resume, s = results, c = connection.

Source & releases: https://github.com/IniAku/xmrig-iniaku
License: GPL-3.0 (see LICENSE). Based on XMRig (https://github.com/xmrig/xmrig).
