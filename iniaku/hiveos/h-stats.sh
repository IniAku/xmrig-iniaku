#!/usr/bin/env bash
# Sourced by the HiveOS agent: sets $khs and $stats from the xmrig HTTP API.

. "$(dirname "${BASH_SOURCE[0]}")/h-manifest.conf"

summary=$(curl -s --max-time 2 "http://127.0.0.1:$CUSTOM_API_PORT/2/summary")
if [[ -z $summary ]] || ! jq -e . <<< "$summary" > /dev/null 2>&1; then
	khs=0
	stats=null
	return 0 2>/dev/null || exit 0
fi

# 60 s average (falls back to 10 s right after start).
hs=$(jq '.hashrate.total[1] // .hashrate.total[0] // 0' <<< "$summary")
khs=$(jq -n --argjson h "$hs" '$h / 1000')
cpu_temp=$(cpu-temp 2>/dev/null | grep -oE '^[0-9]+' || echo 0)

stats=$(jq -c --argjson temp "${cpu_temp:-0}" '{
	hs: [(.hashrate.total[1] // .hashrate.total[0] // 0)],
	hs_units: "hs",
	temp: [$temp],
	fan: [0],
	uptime: .uptime,
	ver: .version,
	ar: [.results.shares_good, ((.results.shares_total // 0) - (.results.shares_good // 0))],
	algo: "randomx"
}' <<< "$summary")
