#!/bin/sh
# Release version: from the tag (v6.24.0-iniaku.1 -> 6.24.0-iniaku.1), else APP_VERSION-dev.<commit>.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
tag=$(git -C "$root" describe --tags --exact-match 2>/dev/null || true)
case "$tag" in
v*) echo "${tag#v}" ;;
*) echo "$(sed -n 's/^#define APP_VERSION *"\(.*\)"/\1/p' "$root/src/version.h")-dev.$(git -C "$root" rev-parse --short HEAD)" ;;
esac
