#!/usr/bin/env bash
# Builds app.cgi for Rocky Linux 9 (VMServicios) inside a Rocky Linux 9
# container, so the binary links against the same glibc and MariaDB
# Connector/C that production has. Production machines have no compilers.
#
#   ./scripts/build-rocky.sh
#
# Needs podman or docker in WSL (for example: sudo apt install podman).
# Output: dist/, the bundle to copy to VMServicios.
#
# If Rocky's compiler (GCC 11) reports a warning that newer compilers do not,
# the build stops because of -Werror. Review the warning, and only if it is
# harmless, build with:   JC_WERROR=OFF ./scripts/build-rocky.sh
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${CONTAINER_ENGINE:-$(command -v podman || command -v docker || true)}"
IMAGE="docker.io/rockylinux/rockylinux:9"
DIST="$REPO/dist"

if [ -z "$ENGINE" ]; then
    echo "error: install podman or docker first" >&2
    exit 1
fi

rm -rf "$DIST"
mkdir -p "$DIST"

"$ENGINE" run --rm -e JC_WERROR="${JC_WERROR:-ON}" \
    -v "$REPO":/src:ro -v "$DIST":/out "$IMAGE" bash -euo pipefail -c '
    dnf -y -q install dnf-plugins-core > /dev/null
    dnf config-manager --set-enabled crb
    dnf -y -q install epel-release > /dev/null
    dnf -y -q install gcc-c++ make cmake pkgconf-pkg-config mariadb-connector-c-devel \
        openssl-devel libyubikey-devel libcurl-devel > /dev/null
    cmake -S /src -B /tmp/build -DCMAKE_BUILD_TYPE=Release -DJC_WERROR="$JC_WERROR" > /dev/null
    cmake --build /tmp/build -j"$(nproc)"
    ctest --test-dir /tmp/build --output-on-failure
    install -m 0755 /tmp/build/app.cgi /out/app.cgi
'

mkdir -p "$DIST/static"
cp "$REPO"/static/* "$DIST/static/"
cp "$REPO/deploy/apache/jesterschest.conf" "$REPO/deploy/install-vmservicios.sh" \
   "$REPO/deploy/mail-pin.sh" "$REPO/deploy/mail.conf.example" "$DIST/"
(cd "$DIST" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)

echo
echo "Bundle ready in dist/:"
cat "$DIST/SHA256SUMS"
