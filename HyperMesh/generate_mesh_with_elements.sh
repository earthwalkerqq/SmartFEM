#!/bin/bash
# Генерация сетки через Gmsh API с заданным количеством элементов

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

TARGET_ELEMENTS="${2:-50}"
OUTPUT_DIR="${SCRIPT_DIR}/../build"
OUTPUT_MSH="${OUTPUT_DIR}/HyperMesh.msh"
OUTPUT_NODE="${OUTPUT_DIR}/node.txt"
OUTPUT_ADJ="${OUTPUT_DIR}/adjacency.txt"

mkdir -p "${OUTPUT_DIR}"

# Обработка пути к STEP файлу
if [ -n "$1" ]; then
    if [[ "$1" = /* ]]; then
        STEP_FILE="$1"
    else
        if [ -f "${PROJECT_ROOT}/$1" ]; then
            STEP_FILE="${PROJECT_ROOT}/$1"
        elif [ -f "$(pwd)/$1" ]; then
            STEP_FILE="$(cd "$(dirname "$(pwd)/$1")" && pwd)/$(basename "$1")"
        elif [ -f "${SCRIPT_DIR}/$1" ]; then
            STEP_FILE="${SCRIPT_DIR}/$1"
        else
            STEP_FILE="$1"
        fi
    fi
else
    STEP_FILE="${PROJECT_ROOT}/materials/data-sample/52.stp"
fi

if [[ ! "$STEP_FILE" = /* ]] && [ -f "$STEP_FILE" ]; then
    STEP_FILE="$(cd "$(dirname "$STEP_FILE")" 2>/dev/null && pwd)/$(basename "$STEP_FILE")" || true
fi

echo "Generating mesh from: ${STEP_FILE}"
echo "Target elements: ${TARGET_ELEMENTS}"

if [ ! -f "${STEP_FILE}" ]; then
    echo "Error: STEP file not found: ${STEP_FILE}"
    exit 1
fi

# Компилируем программу для генерации сетки с заданным количеством элементов
cd "${SCRIPT_DIR}"
make -s 2>/dev/null || {
    echo "Error: Failed to build mesh generator"
    exit 1
}

# Запускаем программу для генерации сетки
cd "${OUTPUT_DIR}"
"${SCRIPT_DIR}/../build/hypermesh" "${STEP_FILE}" "${TARGET_ELEMENTS}"

if [ ! -f "${OUTPUT_MSH}" ]; then
    echo "Error: Failed to generate mesh file"
    exit 1
fi

echo "Mesh generated: ${OUTPUT_MSH}"

# Генерация файла смежности
if [ -f "${OUTPUT_NODE}" ]; then
    rm -f "${OUTPUT_ADJ}"
    python3 "${SCRIPT_DIR}/generate_adjacency.py" "${OUTPUT_MSH}" "${OUTPUT_ADJ}" 2>/dev/null || true
fi

echo "Output: ${OUTPUT_NODE}"
echo "Adjacency file: ${OUTPUT_ADJ}"
echo "Done!"

