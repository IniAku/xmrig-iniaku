#!/usr/bin/env bash
cd "$(dirname "$0")" || exit 1
. h-manifest.conf

mkdir -p "$(dirname "$CUSTOM_LOG_BASENAME")"
# CUSTOM_USER_CONFIG = extra xmrig options from the flight sheet (e.g. --threads=6 --region=us).
./xmrig -c "$CUSTOM_CONFIG_FILENAME" $CUSTOM_USER_CONFIG 2>&1 | tee --append "$CUSTOM_LOG_BASENAME.log"
