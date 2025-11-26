#!/bin/bash
#
# Compile shaders for Carbon UI
#
# On macOS: Compiles .metal files to .metallib
# On Linux/Windows: Would compile HLSL to SPIRV using glslc or dxc
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/compiled"

mkdir -p "$OUTPUT_DIR"

# Detect platform
case "$(uname -s)" in
    Darwin)
        echo "Compiling Metal shaders for macOS..."

        # Compile .metal to .air (intermediate)
        xcrun -sdk macosx metal -c "${SCRIPT_DIR}/ui.metal" -o "${OUTPUT_DIR}/ui.air"

        # Link .air to .metallib
        xcrun -sdk macosx metallib "${OUTPUT_DIR}/ui.air" -o "${OUTPUT_DIR}/ui.metallib"

        # Clean up intermediate file
        rm -f "${OUTPUT_DIR}/ui.air"

        echo "Created: ${OUTPUT_DIR}/ui.metallib"
        ;;

    Linux)
        echo "Compiling SPIRV shaders for Linux..."

        # Check for glslc (from Vulkan SDK)
        if command -v glslc &> /dev/null; then
            # First convert HLSL to GLSL or use glslc directly
            # glslc supports HLSL with -x hlsl flag
            glslc -x hlsl -fshader-stage=vertex "${SCRIPT_DIR}/ui.vert.hlsl" -o "${OUTPUT_DIR}/ui.vert.spv"
            glslc -x hlsl -fshader-stage=fragment "${SCRIPT_DIR}/ui.frag.hlsl" -o "${OUTPUT_DIR}/ui.frag.spv"
            echo "Created: ${OUTPUT_DIR}/ui.vert.spv, ui.frag.spv"
        elif command -v dxc &> /dev/null; then
            # Use DirectXShaderCompiler
            dxc -spirv -T vs_6_0 -E main "${SCRIPT_DIR}/ui.vert.hlsl" -Fo "${OUTPUT_DIR}/ui.vert.spv"
            dxc -spirv -T ps_6_0 -E main "${SCRIPT_DIR}/ui.frag.hlsl" -Fo "${OUTPUT_DIR}/ui.frag.spv"
            echo "Created: ${OUTPUT_DIR}/ui.vert.spv, ui.frag.spv"
        else
            echo "Error: No shader compiler found. Install Vulkan SDK (glslc) or DirectXShaderCompiler (dxc)."
            exit 1
        fi
        ;;

    MINGW*|MSYS*|CYGWIN*)
        echo "Compiling DXIL shaders for Windows..."

        if command -v dxc &> /dev/null; then
            # Compile to DXIL for D3D12
            dxc -T vs_6_0 -E main "${SCRIPT_DIR}/ui.vert.hlsl" -Fo "${OUTPUT_DIR}/ui.vert.dxil"
            dxc -T ps_6_0 -E main "${SCRIPT_DIR}/ui.frag.hlsl" -Fo "${OUTPUT_DIR}/ui.frag.dxil"
            echo "Created: ${OUTPUT_DIR}/ui.vert.dxil, ui.frag.dxil"
        else
            echo "Error: dxc not found. Install DirectXShaderCompiler."
            exit 1
        fi
        ;;

    *)
        echo "Unknown platform: $(uname -s)"
        exit 1
        ;;
esac

echo "Shader compilation complete."
