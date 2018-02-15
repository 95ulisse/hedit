#!/bin/bash

set -e
set -o pipefail

DEPS_DIR="$1"
VERSION="$2"
ARCH="$3"

# Switch to the `deps` directory
cd "$DEPS_DIR"

if [[ -f "v8/out/$ARCH.release/obj.target/src/libv8_base.a" ]]; then
    echo "V8 already built."
    exit 0
fi

# Check for depot-tools
if ! command -v fetch >/dev/null; then
    echo "Depot-tools are required to build V8." >&2
    echo "Please, install them from: http://dev.chromium.org/developers/how-tos/install-depot-tools" >&2
    exit 1
fi

# Check for python 2
if ! python --version 2>&1 | grep -qi "^python\s*2"; then
    echo "Depot-tools only work with python 2." >&2
    echo "Please, set the default python interpreter to python2." >&2
    exit 1
fi

if [[ ! -d v8 ]]; then
    echo "Fetching v8..."
    fetch v8
fi
pushd v8

git checkout "remotes/branch-heads/$VERSION"
gclient sync

if gcc -v 2>&1 | grep -qi "gcc\s*version\s*7\."; then
    echo "Applying patch for gcc 7..."
    patch -N -p1 < ../v8-gcc7.patch || true
fi

popd

# Build v8
GYPFLAGS="-Dclang=0" make -C v8 "$ARCH.release" werror=no
