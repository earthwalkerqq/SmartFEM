#!/bin/bash
# Скрипт для запуска полного пайплайна: STEP -> Gmsh -> FEM

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STEP_FILE="${1:-materials/data-sample/52.stp}"
CLMIN="${2:-0.1}"
CLMAX="${3:-1.0}"

echo "=== SmartFEM Pipeline ==="
echo "Input: $STEP_FILE"
echo "Mesh size: min=$CLMIN, max=$CLMAX"
echo ""

# Шаг 1: Генерация сетки через Gmsh CLI
echo "[1/2] Generating mesh with Gmsh..."
cd "$SCRIPT_DIR/HyperMesh"
if ./generate_mesh.sh "$SCRIPT_DIR/$STEP_FILE" "$CLMIN" "$CLMAX" > /dev/null 2>&1; then
    echo "✓ Mesh generated successfully"
else
    echo "Error: Mesh generation failed"
    echo "Make sure Gmsh is installed: brew install gmsh (macOS) or apt-get install gmsh (Linux)"
    exit 1
fi

if [ ! -f "../build/node.txt" ]; then
    echo "Error: Mesh generation failed - node.txt not created"
    exit 1
fi

echo ""

# Шаг 2: FEM расчет
echo "[2/2] Running FEM analysis..."
cd "$SCRIPT_DIR/fem-module/2D"
if make fem > /dev/null 2>&1; then
    ./../build/fem ../build/node.txt
else
    echo "Error: Failed to build FEM module"
    exit 1
fi

echo ""
echo "=== Complete ==="
echo "Results saved to: fem-module/2D/build/result.txt"

