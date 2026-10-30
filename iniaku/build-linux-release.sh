#!/bin/sh
# Linux release build in an Ubuntu 20.04 container (static libuv/hwloc/openssl via
# scripts/build_deps.sh, glibc dynamic), then package: ./iniaku/build-linux-release.sh <version>
# Works without root (CI) or via sudo (files stay owned by the calling user).
set -eu
version=${1:?usage: build-linux-release.sh <version>}
root=$(cd "$(dirname "$0")/.." && pwd)
img=xmrig-iniaku-builder:20.04
uid=${SUDO_UID:-$(id -u)}
gid=${SUDO_GID:-$(id -g)}
docker build -t "$img" -f "$root/iniaku/docker/Dockerfile.linux" "$root/iniaku/docker"
docker run --rm --cpus=4 -u "$uid:$gid" -e HOME=/tmp -v "$root":/src "$img" sh -euc '
	if [ ! -f scripts/deps/lib/libuv.a ] || [ ! -f scripts/deps/lib/libssl.a ] || [ ! -f scripts/deps/lib/libhwloc.a ]; then
		(cd scripts && ./build_deps.sh)
	fi
	mkdir -p build-u2004 && cd build-u2004
	cmake .. -DCMAKE_BUILD_TYPE=Release -DXMRIG_DEPS=scripts/deps -DWITH_OPENCL=OFF -DWITH_CUDA=OFF
	make -j4
'
asuser() {
	if [ "$(id -u)" = 0 ] && [ "$uid" != 0 ]; then
		setpriv --reuid="$uid" --regid="$gid" --clear-groups "$@"
	else
		"$@"
	fi
}
asuser "$root/iniaku/package.sh" linux "$version" "$root/build-u2004/xmrig"
