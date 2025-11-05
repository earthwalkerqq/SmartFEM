#!/bin/bash
# Генерация сетки через командную строку Gmsh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# HyperMesh находится в SmartFEM/HyperMesh, поэтому PROJECT_ROOT на 1 уровень выше
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CLMIN="${2:-0.1}"
CLMAX="${3:-1.0}"
SHOW_GUI="${4:-0}"  # 4-й аргумент: 1 = показать GUI, 0 = не показывать
OUTPUT_DIR="${SCRIPT_DIR}/../build"
OUTPUT_MSH="${OUTPUT_DIR}/HyperMesh.msh"
OUTPUT_NODE="${OUTPUT_DIR}/node.txt"
OUTPUT_ADJ="${OUTPUT_DIR}/adjacency.txt"  # файл со смежными элементами

mkdir -p "${OUTPUT_DIR}"

# Обработка пути к STEP файлу
if [ -n "$1" ]; then
    # Если путь абсолютный, используем как есть
    if [[ "$1" = /* ]]; then
        STEP_FILE="$1"
    # Проверяем все возможные варианты по порядку
    else
        # 1. Относительно PROJECT_ROOT (SmartFEM)
        if [ -f "${PROJECT_ROOT}/$1" ]; then
            STEP_FILE="${PROJECT_ROOT}/$1"
        # 2. Относительно текущей директории (откуда вызывается скрипт)
        elif [ -f "$(pwd)/$1" ]; then
            STEP_FILE="$(cd "$(dirname "$(pwd)/$1")" && pwd)/$(basename "$1")"
        # 3. Относительно SCRIPT_DIR
        elif [ -f "${SCRIPT_DIR}/$1" ]; then
            STEP_FILE="${SCRIPT_DIR}/$1"
        # 4. Попробуем разрешить относительный путь - убираем лишние ../
        else
            # Если путь начинается с ../../, пробуем с ../ вместо
            if [[ "$1" = ../../* ]]; then
                REL_PATH="${1#../../}"
                if [ -f "${PROJECT_ROOT}/${REL_PATH}" ]; then
                    STEP_FILE="${PROJECT_ROOT}/${REL_PATH}"
                else
                    STEP_FILE="$1"
                fi
            else
                STEP_FILE="$1"
            fi
        fi
    fi
else
    STEP_FILE="${PROJECT_ROOT}/materials/data-sample/52.stp"
fi

# Преобразовать в абсолютный путь если еще не абсолютный и файл существует
if [[ ! "$STEP_FILE" = /* ]] && [ -f "$STEP_FILE" ]; then
    STEP_FILE="$(cd "$(dirname "$STEP_FILE")" 2>/dev/null && pwd)/$(basename "$STEP_FILE")" || true
fi

echo "Generating mesh from: ${STEP_FILE}"
echo "Mesh size: min=${CLMIN}, max=${CLMAX}"

if [ ! -f "${STEP_FILE}" ]; then
    echo "Error: STEP file not found: ${STEP_FILE}"
    exit 1
fi

# Генерация сетки через gmsh CLI
cd "${OUTPUT_DIR}"
# Генерация без GUI (batch mode)
gmsh -2 "${STEP_FILE}" -clmin "${CLMIN}" -clmax "${CLMAX}" -format msh2 -o "${OUTPUT_MSH}" 2>&1

if [ ! -f "${OUTPUT_MSH}" ]; then
    echo "Error: Failed to generate mesh file"
    exit 1
fi

echo "Mesh generated: ${OUTPUT_MSH}"

# Конвертация .msh в node.txt формат (для FEM модуля)
# Удаляем старый файл, если существует, чтобы избежать конфликтов
rm -f "${OUTPUT_NODE}"
python3 "${SCRIPT_DIR}/convert_msh.py" "${OUTPUT_MSH}" "${OUTPUT_NODE}"

# Проверка корректности созданного файла
if [ -f "${OUTPUT_NODE}" ]; then
    NODE_COUNT=$(head -1 "${OUTPUT_NODE}" | awk '{print $1}')
    if [ "${NODE_COUNT}" -gt 1000000 ]; then
        echo "Ошибка: Подозрительно большое количество узлов (${NODE_COUNT}). Файл может быть поврежден." >&2
        exit 1
    fi
fi

# Генерация файла с координатами узлов и смежными элементами
# Удаляем старый файл, если существует
rm -f "${OUTPUT_ADJ}"
python3 "${SCRIPT_DIR}/generate_adjacency.py" "${OUTPUT_MSH}" "${OUTPUT_ADJ}"

if [ ! -f "${OUTPUT_NODE}" ]; then
    echo "Error: Failed to convert mesh to node.txt format"
    exit 1
fi

echo "Output: ${OUTPUT_NODE}"
echo "Adjacency file: ${OUTPUT_ADJ}"
echo "Done!"

# Открыть GUI Gmsh если запрошено
if [ "${SHOW_GUI}" = "1" ] || [ "${SHOW_GUI}" = "gui" ]; then
    echo ""
    echo "Opening Gmsh GUI to visualize mesh..."
    gmsh "${OUTPUT_MSH}" &
    echo "Gmsh GUI opened in background. Close it manually when done."
fi

