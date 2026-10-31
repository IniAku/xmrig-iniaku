#!/usr/bin/env bash
# Builds config.json from the HiveOS flight sheet.
#   Wallet and worker template: %WAL%.%WORKER_NAME%
#   Pool URL:                   us.epic.iniaku.my.id (or id./au.) — anything else = auto
#   Extra config arguments (optional): regular xmrig options, e.g. --threads=6, plus
#     lan=192.168.1.139  the pool node is in your own LAN (router without NAT loopback): the
#                        region's hostname is pointed at that IP in /etc/hosts (TLS still
#                        matches). Needs a regional Pool URL (id./us./au.epic.iniaku.my.id).

dir=$(dirname "${BASH_SOURCE[0]}")
. "$dir/h-manifest.conf"

[[ -z $CUSTOM_TEMPLATE ]] && echo "CUSTOM_TEMPLATE is empty (set it to %WAL%.%WORKER_NAME%)" && return 1

region=auto
case "$CUSTOM_URL" in
	*id.epic*) region=id ;;
	*us.epic*) region=us ;;
	*au.epic*) region=au ;;
esac

lan= args=()
for a in $CUSTOM_USER_CONFIG; do
	case "$a" in
		lan=*) lan=${a#*=} ;;
		*) args+=("$a") ;;
	esac
done
if [[ -n $lan && $region == auto ]]; then
	echo "lan= needs a regional Pool URL (id./us./au.epic.iniaku.my.id) — ignored"
	lan=
fi
echo "${lan:+$lan $region.epic.iniaku.my.id}" > "$dir/.lan_host"
echo "${args[*]}" > "$dir/.args"

jq -n --arg user "$CUSTOM_TEMPLATE" --arg pass "${CUSTOM_PASS:-x}" --arg region "$region" \
	--argjson port "$CUSTOM_API_PORT" '{
	autosave: false,
	"donate-level": 0,
	region: $region,
	cpu: {enabled: true, "huge-pages": true, "max-threads-hint": 100},
	randomx: {init: -1, mode: "auto", "1gb-pages": false},
	pools: [{user: $user, pass: $pass}],
	http: {enabled: true, host: "127.0.0.1", port: $port, "access-token": null, restricted: true},
	"print-time": 60,
	colors: false
}' > "$CUSTOM_CONFIG_FILENAME"
