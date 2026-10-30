XMRig — epic.iniaku.my.id edition (Epic Cash RandomX)
=====================================================

XMRig 6.24 with Epic Cash RandomX ("rx/epic") and the native Epic stratum protocol.
This build mines ONLY to the epic.iniaku.my.id pool (ID / US / AU servers, TLS port 3334).
No developer donation.

Quick start
-----------
1. Open config.json and set your Epic address and a worker name:
       "user": "<your Epic address>.<worker name>"
   or pass it on the command line:  -u <your Epic address>.<worker name>
2. Linux:   ./start.sh        (or ./xmrig)
   Windows: start.cmd         (or xmrig.exe)
3. Check your worker at https://epic.iniaku.my.id

Region
------
"region": "auto" (default) measures the connection time to id / us / au and uses the
closest server; if it goes down XMRig switches to the next one automatically.
Force a region in config.json ("region": "us") or on the command line: --region us
Valid values: auto, id, us, au. Pool URLs given with -o or in config.json are ignored.

Faster mining: huge pages and MSR (administrator / root)
--------------------------------------------------------
RandomX is 20-50% faster with huge pages and the MSR mod enabled. Both need elevated
rights:
  Windows: right-click start.cmd -> "Run as administrator" (the first run may ask for a
           reboot so the "Lock pages in memory" right takes effect).
  Linux:   run as root (sudo ./start.sh), or enable huge pages once:
           sudo sysctl -w vm.nr_hugepages=1280
The miner also works without them, just slower ("huge pages 0%" in the log).

Notes
-----
- Epic rotates the algorithm per block (ProgPoW / RandomX / Cuckoo). This miner keeps
  mining RandomX all the time; the pool accepts RandomX shares in every turn.
- About 2.3 GB RAM is used for the RandomX dataset ("fast" mode). On machines with
  little RAM set "mode": "light" in the "randomx" section (much slower).
- Keys while running: h = hashrate, p = pause, r = resume, s = results, c = connection.

Source & releases: https://github.com/IniAku/xmrig-iniaku
License: GPL-3.0 (see LICENSE). Based on XMRig (https://github.com/xmrig/xmrig).
