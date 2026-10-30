#!/bin/sh
# Package a release: ./iniaku/package.sh <linux|windows> <version> <path to xmrig binary>
set -eu
os=${1:?usage: package.sh <linux|windows> <version> <binary>}
version=${2:?}
bin=${3:?}
root=$(cd "$(dirname "$0")/.." && pwd)
name="xmrig-iniaku-$version-$os-x64"
out="$root/dist/$name"

rm -rf "$out"
mkdir -p "$out"
cp "$root/iniaku/config.json" "$out/"
cp "$root/LICENSE" "$out/"
if [ "$os" = windows ]; then
	cp "$bin" "$out/xmrig.exe"
	cp "$root/iniaku/start.cmd" "$out/"
	sed 's/$/\r/' "$root/iniaku/README.txt" > "$out/README.txt"
	sed 's/$/\r/' "$root/iniaku/config.json" > "$out/config.json"
	cd "$root/dist" && 7z a -tzip "$name.zip" "$name" > /dev/null
	sha256sum "$name.zip" > "$name.zip.sha256"
	echo "$root/dist/$name.zip"
else
	cp "$bin" "$out/xmrig"
	strip "$out/xmrig" 2>/dev/null || true
	cp "$root/iniaku/start.sh" "$out/"
	cp "$root/iniaku/README.txt" "$out/"
	chmod +x "$out/xmrig" "$out/start.sh"
	tar -C "$root/dist" -czf "$root/dist/$name.tar.gz" "$name"
	(cd "$root/dist" && sha256sum "$name.tar.gz" > "$name.tar.gz.sha256")
	echo "$root/dist/$name.tar.gz"
fi
