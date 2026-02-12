#!/bin/bash
set -euo pipefail
shopt -s globstar nullglob

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

command -v glslc >/dev/null 2>&1 || { echo -e "${RED}glslc not found. Install Vulkan SDK.${NC}"; exit 1; }
command -v spirv-opt >/dev/null 2>&1 || { echo -e "${RED}spirv-opt not found. Install SPIRV-Tools.${NC}"; exit 1; }
command -v spirv-val >/dev/null 2>&1 || { echo -e "${RED}spirv-val not found. Install SPIRV-Tools.${NC}"; exit 1; }

SHADER_DIR="engine/shaders"
EXTENSIONS=("rgen" "rmiss" "rchit" "rint" "rahit" "vert" "frag" "comp")
OPTIMIZE=false

echo "Compiling shaders in $SHADER_DIR..."

for ext in "${EXTENSIONS[@]}"; do
  for src in "$SHADER_DIR"/**/*."$ext"; do
    [[ -e "$src" ]] || continue

    base="${src%.*}"
    spv="$base.$ext.spv"
    opt_spv="$base.opt.spv"

    echo "Compiling $src -> $spv"
    glslc -I"$SHADER_DIR" -g --target-env=vulkan1.3 "$src" -o "$spv" || {
        echo -e "${RED}Error: glslc failed on $src${NC}"
        exit 1
    }

    if [ "$OPTIMIZE" = true ]; then
        echo "Optimizing $spv -> $opt_spv"
        spirv-opt -O "$spv" -o "$opt_spv" || {
            echo -e "${RED}Error: spirv-opt failed on $spv${NC}"
            exit 1
        }

        echo "Validating $opt_spv"
        spirv-val "$opt_spv" || {
            echo -e "${RED}Error: spirv-val failed on $opt_spv${NC}"
            exit 1
        }

        mv "$opt_spv" "$spv"
    fi

    echo -e "${GREEN}✔ Successfully compiled: $src${NC}"
  done
done

echo -e "${GREEN}[ALL SHADERS COMPILED]${NC} $SHADER_DIR/*.{${EXTENSIONS[*]}}.spv"
