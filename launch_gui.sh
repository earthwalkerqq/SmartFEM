#!/bin/bash
# Скрипт для запуска SmartFEM GUI

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GUI_APP="${SCRIPT_DIR}/build/SmartFEM_GUI.app"

if [ -d "$GUI_APP" ]; then
    echo "Запуск SmartFEM GUI..."
    open "$GUI_APP"
    echo "GUI приложение запущено!"
elif [ -f "${SCRIPT_DIR}/build/SmartFEM_GUI" ]; then
    echo "Запуск SmartFEM GUI (executable)..."
    "${SCRIPT_DIR}/build/SmartFEM_GUI" &
    echo "GUI приложение запущено!"
else
    echo "Ошибка: GUI приложение не найдено!"
    echo "Сначала соберите проект:"
    echo "  cd preprocess/gui && make"
    exit 1
fi
