#!/bin/bash
# Compile lighting GLSL shaders to SPIR-V

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "Compiling lighting shaders..."

# Vertex shader (shared by all lighting effects)
glslc -fshader-stage=vertex lighting.vert.glsl -o lighting.vert.spv
echo "  lighting.vert.spv"

# Fragment shaders
glslc -fshader-stage=fragment point_light.frag.glsl -o point_light.frag.spv
echo "  point_light.frag.spv"

glslc -fshader-stage=fragment point_light_shadow.frag.glsl -o point_light_shadow.frag.spv
echo "  point_light_shadow.frag.spv"

glslc -fshader-stage=fragment spot_light.frag.glsl -o spot_light.frag.spv
echo "  spot_light.frag.spv"

glslc -fshader-stage=fragment spot_light_shadow.frag.glsl -o spot_light_shadow.frag.spv
echo "  spot_light_shadow.frag.spv"

glslc -fshader-stage=fragment composite.frag.glsl -o composite.frag.spv
echo "  composite.frag.spv"

glslc -fshader-stage=fragment ambient.frag.glsl -o ambient.frag.spv
echo "  ambient.frag.spv"

echo "Done!"
