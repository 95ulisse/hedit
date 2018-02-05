#!/bin/bash

# This script generates the needed C++ source and include files from all the builtin JS modules.
# Usage: ./gen-js.sh <path_to_js_modules_folder> <path_to_output_file>

set -e
set -o pipefail

MODULES_FOLDER="$1"
OUTPUT_FILE="$2"

MINIFIER="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/rjsmin.py"

strLen() {
    local bytelen sreal oLang=$LANG oLcAll=$LC_ALL
    LANG=C LC_ALL=C
    bytelen=${#1}
    LANG=$oLang LC_ALL=$oLcAll
    echo $bytelen
}

gen_code() {

    declare -A MODULE_MAP

    cat <<EOF
#include <memory>
#include <map>
#include <js.h>
EOF

    for x in $(find . -name '*.js'); do

        # Normalize the name of the variable and the module
        VAR_NAME="$(echo "$x" | tr './' '_')"
        MODULE_NAME="hedit/$(echo "${x#./}" | sed 's/.js$//')"
        if [[ "$MODULE_NAME" == "hedit/hedit" ]]; then
            MODULE_NAME="hedit"
        fi
        MODULE_MAP[$MODULE_NAME]="$VAR_NAME"

        # Minify the source
        MINIFIED="$("$MINIFIER" <"$x")"
        MINIFIED_LEN="$(strLen "$MINIFIED")"

        # Emit the declaration
        echo "static unsigned char $VAR_NAME[] = {"
        echo "$MINIFIED" | xxd -i
        echo "};"
        echo "static unsigned int ${VAR_NAME}_len = $MINIFIED_LEN;"
        
        echo "Packaged JS module $MODULE_NAME ($MINIFIED_LEN bytes)." >&3

    done

    cat <<EOF

std::map<std::string, std::shared_ptr<JsBuiltinModule>> JsBuiltinModule::_all_modules = {
EOF

    for k in "${!MODULE_MAP[@]}"; do
        echo "{ \"$k\", std::shared_ptr<JsBuiltinModule>(new JsBuiltinModule(\"$k\", ${MODULE_MAP[$k]}, ${MODULE_MAP[$k]}_len)) },"
    done

    echo "};"

}

mkdir -p "$(dirname $OUTPUT_FILE)" >/dev/null 2>/dev/null || true

(
    pushd "$MODULES_FOLDER" >/dev/null 2>/dev/null
    gen_code 3>&1 >&4
    popd >/dev/null 2>/dev/null
) 4>"$OUTPUT_FILE"
