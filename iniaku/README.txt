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
Valid values: auto, id, us, au. Pool URLs given with -o or in config.json are ignored.

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
- Epic rotates the algorithm per block (ProgPoW / RandomX / Cuckoo). This miner keeps
  mining RandomX all the time; the pool accepts RandomX shares in every turn.
- About 2.3 GB RAM is used for the RandomX dataset ("fast" mode). On machines with
  little RAM set "mode": "light" in the "randomx" section (much slower).
- Keys while running: h = hashrate, p = pause, r = resume, s = results, c = connection.

Source & releases: https://github.com/IniAku/xmrig-iniaku
License: GPL-3.0 (see LICENSE). Based on XMRig (https://github.com/xmrig/xmrig).
