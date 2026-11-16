#!/usr/bin/env bash
# Builds, runs the tests and installs app.cgi and the static files into the
# local Apache. Run after every change you want to see in the browser:
#   ./scripts/dev-deploy.sh            # Release build
#   ./scripts/dev-deploy.sh Debug      # with AddressSanitizer/UBSan
# Then open http://localhost/ (errors: /var/log/apache2/jesterschest-error.log)
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_TYPE="${1:-Release}"
BUILD_DIR="$REPO/build-$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"

cmake -S "$REPO" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" > /dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure

sudo install -m 0755 "$BUILD_DIR/app.cgi" /usr/lib/cgi-bin/app.cgi
sudo install -m 0644 "$REPO"/static/* /var/www/jesterschest/static/
echo "Deployed ($BUILD_TYPE): http://localhost/"
