#!/bin/bash

set -e
set -o pipefail

DEPS_DIR="$1"

# Switch to the `deps` directory
cd "$DEPS_DIR"

if [[ -f "libtickit/.libs/libtickit.a" ]]; then
    echo "Libtickit already built."
    exit 0
fi

# Check for Bazaar
if ! command -v bzr >/dev/null; then
    echo "Bazaar is not installed."
    exit 1
fi

if [[ ! -d libtickit ]]; then
    echo "Fetching libtickit..."
    bzr branch http://bazaar.leonerd.org.uk/c/libtickit
fi

pushd libtickit
patch -N -p1 < ../libtickit.patch || true
make
popd
