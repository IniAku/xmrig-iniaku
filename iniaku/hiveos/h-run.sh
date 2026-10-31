#!/usr/bin/env bash
cd "$(dirname "$0")" || exit 1
. h-manifest.conf

# lan= from the flight sheet: pin the region hostname to the LAN IP (and drop an old pin).
read -r lan_ip lan_host < .lan_host 2>/dev/null
sed -i '/# iniaku-lan-'"$CUSTOM_NAME"'$/d' /etc/hosts 2>/dev/null
[[ -n $lan_ip ]] && echo "$lan_ip $lan_host # iniaku-lan-$CUSTOM_NAME" >> /etc/hosts
mkdir -p "$(dirname "$CUSTOM_LOG_BASENAME")"
# .args = extra xmrig options from the flight sheet (h-config.sh, without lan=).
./xmrig -c "$CUSTOM_CONFIG_FILENAME" $(cat .args 2>/dev/null) 2>&1 | tee --append "$CUSTOM_LOG_BASENAME.log"
