#!/bin/bash

# This script generates the needed C++ source and include files from all the builtin JS modules.
# Usage: ./gen-js.sh <path_to_js_modules_folder> <path_to_output_file>

set -e
set -o pipefail

MODULES_FOLDER="$1"
OUTPUT_FILE="$2"

gen_code() {

    declare -A MODULE_MAP

    cat <<EOF
#include <map>
#include <js.h>
EOF

    for x in *.js; do
        VAR_NAME="$(echo "$x" | tr './' '_')"
        MODULE_NAME="hedit/$(echo "$VAR_NAME" | sed 's/_js$//')"
        if [[ "$MODULE_NAME" == "hedit/hedit" ]]; then
            MODULE_NAME="hedit"
        fi
        MODULE_MAP[$MODULE_NAME]="$VAR_NAME"

        xxd -i $x | sed -E 's/unsigned\s+(char|int)/static unsigned \1/'
        
        echo "Packaged JS module $MODULE_NAME." >&3
    done

    cat <<EOF

std::map<std::string, JsBuiltinModule*> JsBuiltinModule::_all_modules = {
EOF

    for k in "${!MODULE_MAP[@]}"; do
        echo "{ \"$k\", new JsBuiltinModule(\"$k\", ${MODULE_MAP[$k]}, ${MODULE_MAP[$k]}_len) },"
    done

    echo "};"

}

mkdir -p "$(dirname $OUTPUT_FILE)" >/dev/null 2>/dev/null || true

(
    pushd "$MODULES_FOLDER" >/dev/null 2>/dev/null
    gen_code 3>&1 >&4
    popd >/dev/null 2>/dev/null
) 4>"$OUTPUT_FILE"
