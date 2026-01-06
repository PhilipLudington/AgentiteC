#!/bin/bash
# Compile transition GLSL shaders to SPIR-V

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "Compiling transition shaders..."

# Vertex shader (shared by all transition effects)
glslc -fshader-stage=vertex transition.vert.glsl -o transition.vert.spv
echo "  transition.vert.spv"

# Fragment shaders
glslc -fshader-stage=fragment crossfade.frag.glsl -o crossfade.frag.spv
echo "  crossfade.frag.spv"

glslc -fshader-stage=fragment wipe.frag.glsl -o wipe.frag.spv
echo "  wipe.frag.spv"

glslc -fshader-stage=fragment circle.frag.glsl -o circle.frag.spv
echo "  circle.frag.spv"

glslc -fshader-stage=fragment slide.frag.glsl -o slide.frag.spv
echo "  slide.frag.spv"

glslc -fshader-stage=fragment dissolve.frag.glsl -o dissolve.frag.spv
echo "  dissolve.frag.spv"

echo "Done!"
