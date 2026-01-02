#!/bin/bash
#
# Compile post-processing shaders to SPIR-V
#
# Requires glslc from the Vulkan SDK or shaderc
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}"

# Check for glslc
if ! command -v glslc &> /dev/null; then
    echo "Error: glslc not found. Install the Vulkan SDK."
    exit 1
fi

echo "Compiling post-processing shaders..."

# Compile vertex shader (shared by all post-process effects)
glslc -fshader-stage=vertex "${SCRIPT_DIR}/fullscreen.vert.glsl" -o "${OUTPUT_DIR}/fullscreen.vert.spv"
echo "Compiled: fullscreen.vert.spv"

# Compile fragment shaders
for frag in grayscale sepia invert brightness contrast saturation blur_box chromatic scanlines pixelate sobel flash vignette_pp; do
    if [ -f "${SCRIPT_DIR}/${frag}.frag.glsl" ]; then
        glslc -fshader-stage=fragment "${SCRIPT_DIR}/${frag}.frag.glsl" -o "${OUTPUT_DIR}/${frag}.frag.spv"
        echo "Compiled: ${frag}.frag.spv"
    fi
done

echo "Post-processing shader compilation complete."
