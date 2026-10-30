#!/bin/sh
# XMRig epic.iniaku.my.id edition — run from the package folder.
cd "$(dirname "$0")" && exec ./xmrig -c config.json "$@"
