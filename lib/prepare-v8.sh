#!/bin/bash

set -e
set -o pipefail

VERSION="$1"

if [[ ! -d v8 ]]; then
    echo "Fetching v8..."
    fetch v8
fi
pushd v8

git checkout "remotes/branch-heads/$VERSION"
gclient sync

if gcc -v 2>&1 | grep -qiP "gcc\s*version\s*7\."; then
    echo "Applying patch for gcc 7..."
    patch -N -p1 < ../v8-gcc7.patch || true
fi

popd

