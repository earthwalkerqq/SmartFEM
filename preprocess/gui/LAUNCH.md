# Запуск SmartFEM GUI

## Быстрый запуск

### Вариант 1: Через Finder (macOS)
```bash
cd /Users/matvej/Desktop/SmartFEM/build
open SmartFEM_GUI.app
```

### Вариант 2: Через командную строку
```bash
cd /Users/matvej/Desktop/SmartFEM/build
open SmartFEM_GUI.app
```

Или напрямую через исполняемый файл:
```bash
/Users/matvej/Desktop/SmartFEM/build/SmartFEM_GUI.app/Contents/MacOS/SmartFEM_GUI
```

### Вариант 3: Из директории GUI
```bash
cd /Users/matvej/Desktop/SmartFEM/preprocess/gui
../../build/SmartFEM_GUI.app/Contents/MacOS/SmartFEM_GUI
```

## Пересборка (если нужно)

```bash
cd /Users/matvej/Desktop/SmartFEM/preprocess/gui
qmake SmartFEM_GUI.pro
make
```

Или через Makefile:
```bash
cd /Users/matvej/Desktop/SmartFEM/preprocess/gui
make
```

## Использование GUI

1. **Выберите STEP файл** - нажмите "Обзор..." рядом с полем "STEP файл"
2. **Настройте параметры сетки**:
   - clmin: минимальный размер элемента (по умолчанию 0.1)
   - clmax: максимальный размер элемента (по умолчанию 1.0)
3. **Настройте параметры материала**:
   - E: модуль упругости (МПа, по умолчанию 2.1e5)
   - ν: коэффициент Пуассона (по умолчанию 0.3)
   - ρ: плотность (кг/м³, по умолчанию 7850)
   - h: толщина (м, по умолчанию 1.0)
4. **Выберите тип анализа**:
   - Статический FEM анализ
   - Модальный анализ (колебания)
5. **Для модального анализа** - укажите количество мод
6. **Нажмите "Запустить расчет"**

Результаты отобразятся в текстовом поле внизу окна.

