# Быстрый старт - SmartFEM

## 📋 Обзор пайплайна

```
STEP файл → Gmsh (генерация сетки) → node.txt → FEM модуль → Результаты
                                      ↓
                                 Модальный анализ
```

## 🚀 Вариант 1: Полный пайплайн (автоматически)

### Шаг 1: Генерация сетки из STEP файла

```bash
cd /Users/matvej/Desktop/SmartFEM
./run_fem.sh materials/data-sample/52.stp 0.1 1.0
```

**Параметры:**
- `materials/data-sample/52.stp` - путь к STEP файлу
- `0.1` - минимальный размер элемента сетки
- `1.0` - максимальный размер элемента сетки

**Что происходит:**
1. Генерируется сетка через Gmsh CLI
2. Создается `build/node.txt` (для FEM модуля)
3. Создается `build/adjacency.txt` (координаты + смежные элементы)
4. Запускается FEM расчет

**Результаты:**
- `fem-module/2D/build/result.txt` - результаты FEM расчета

---

## 🛠️ Вариант 2: Пошаговый запуск

### Шаг 1: Генерация сетки через Gmsh

```bash
cd /Users/matvej/Desktop/SmartFEM/HyperMesh
./generate_mesh.sh ../../materials/data-sample/52.stp 0.1 1.0
```

**С визуализацией в GUI:**
```bash
./generate_mesh.sh ../../materials/data-sample/52.stp 0.1 1.0 gui
```

**Результаты:**
- `../build/HyperMesh.msh` - сетка в формате Gmsh
- `../build/node.txt` - сетка для FEM модуля (простой формат)
- `../build/adjacency.txt` - координаты узлов + смежные элементы (макс. 3 на узел)

**Формат adjacency.txt:**
```
14813
1 23.5 -20.0 -26.0 3 5245 5246 21372
2 23.5 20.0 -26.0 3 5244 5247 10875
...
```
- Строка 1: количество узлов
- Для каждого узла: `номер_узла x y z количество_смежных_элементов список_элементов`

### Шаг 2: FEM расчет (статический анализ)

```bash
cd /Users/matvej/Desktop/SmartFEM/fem-module/2D
make
../build/fem ../build/node.txt
```

**Результаты:**
- `build/result.txt` - перемещения, деформации, напряжения

### Шаг 3: Модальный анализ (колебания)

```bash
cd /Users/matvej/Desktop/SmartFEM/fem-module/modal
make
../build/modal ../build/node.txt 5 7850 1.0
```

**Параметры:**
- `../build/node.txt` - файл сетки
- `5` - количество мод для расчета
- `7850` - плотность материала (кг/м³, сталь)
- `1.0` - толщина (м)

**Результаты:** Выводит в консоль собственные частоты и формы колебаний

---

## 📁 Структура файлов

```
SmartFEM/
├── materials/data-sample/
│   ├── 52.stp              # Входной STEP файл
│   └── node.txt            # Тестовая сетка (опционально)
├── build/
│   ├── HyperMesh.msh      # Сетка Gmsh (после генерации)
│   ├── node.txt            # Сетка для FEM (после генерации)
│   └── adjacency.txt       # Координаты + смежные элементы
├── fem-module/
│   ├── 2D/
│   │   └── build/
│   │       └── result.txt  # Результаты FEM расчета
│   └── modal/
│       └── build/
│           └── modal       # Бинарь модального анализа
└── HyperMesh/
    ├── generate_mesh.sh    # Скрипт генерации сетки
    └── convert_msh.py      # Конвертер MSH → node.txt
```

---

## 🔧 Примеры использования

### Пример 1: Генерация сетки с визуализацией

```bash
cd /Users/matvej/Desktop/SmartFEM/HyperMesh
./generate_mesh.sh ../../materials/data-sample/52.stp 0.1 1.0 gui
```

Откроется GUI Gmsh для визуализации сетки.

### Пример 2: Мелкая сетка для точного расчета

```bash
cd /Users/matvej/Desktop/SmartFEM/HyperMesh
./generate_mesh.sh ../../materials/data-sample/52.stp 0.05 0.5
```

Более мелкая сетка (min=0.05, max=0.5).

### Пример 3: FEM расчет с кастомной сеткой

```bash
cd /Users/matvej/Desktop/SmartFEM/fem-module/2D
make
../build/fem /path/to/your/mesh.txt
```

### Пример 4: Модальный анализ (5 мод)

```bash
cd /Users/matvej/Desktop/SmartFEM/fem-module/modal
make
../build/modal ../build/node.txt 5 7850 1.0
```

---

## 📝 Формат файла node.txt

Файл `node.txt` имеет простой формат для FEM модуля:

```
4              # Количество узлов
0.0 0.0 0.0    # Узел 1: x y z
0.0 1.0 0.0    # Узел 2: x y z
1.0 0.0 0.0    # Узел 3: x y z
1.0 1.0 0.0    # Узел 4: x y z
2              # Количество элементов
1 2 3          # Элемент 1: узлы 1, 2, 3
2 4 3          # Элемент 2: узлы 2, 4, 3
```

---

## 📊 Формат файла adjacency.txt

Файл с координатами и смежными элементами:

```
14813          # Количество узлов
1 23.5 -20.0 -26.0 3 5245 5246 21372    # Узел 1: номер x y z количество_смежных список
2 23.5 20.0 -26.0 3 5244 5247 10875    # Узел 2: номер x y z количество_смежных список
...
29622          # Количество элементов
1 2 3          # Элемент 1: узлы
2 4 3          # Элемент 2: узлы
...
```

**Структура строки узла:**
`номер_узла x y z количество_смежных_элементов элемент1 элемент2 элемент3`

Максимум 3 смежных элемента на узел.

---

## ⚙️ Настройка параметров

### Размер сетки
- Мелкая сетка (точнее): `clmin=0.05, clmax=0.5`
- Средняя сетка: `clmin=0.1, clmax=1.0`
- Крупная сетка (быстрее): `clmin=1.0, clmax=5.0`

### Материал (для модального анализа)
- Сталь: `rho=7850` кг/м³
- Алюминий: `rho=2700` кг/м³
- Толщина: `h=1.0` м (по умолчанию)

---

## 🐛 Решение проблем

### Проблема: "STEP file not found"
**Решение:** Используйте абсолютный путь или путь относительно `HyperMesh/`:
```bash
./generate_mesh.sh /Users/matvej/Desktop/SmartFEM/materials/data-sample/52.stp 0.1 1.0
```

### Проблема: "Segmentation fault" в FEM
**Решение:** Проверьте, что сетка имеет граничные условия (узлы с x=0 или y=0 для закрепления)

### Проблема: Gmsh не найден
**Решение:** Установите Gmsh:
```bash
brew install gmsh  # macOS
```

---

## 📚 Дополнительные команды

### Очистка build файлов
```bash
cd /Users/matvej/Desktop/SmartFEM
rm -rf build/*.msh build/*.txt
cd fem-module/2D && make clean
cd ../modal && make clean
```

### Пересборка всех модулей
```bash
cd /Users/matvej/Desktop/SmartFEM/fem-module/2D && make clean && make
cd ../modal && make clean && make
```

---

## 🎯 Рекомендуемый workflow

1. **Генерация сетки:**
   ```bash
   cd HyperMesh
   ./generate_mesh.sh ../../materials/data-sample/52.stp 0.1 1.0 gui
   ```
   Проверьте сетку в GUI Gmsh.

2. **FEM расчет:**
   ```bash
   cd ../fem-module/2D
   make
   ../build/fem ../build/node.txt
   ```
   Проверьте `build/result.txt`.

3. **Модальный анализ:**
   ```bash
   cd ../modal
   make
   ../build/modal ../build/node.txt 5 7850 1.0
   ```
   Получите собственные частоты и формы.

---

## 📞 Быстрая справка

| Команда | Описание |
|---------|----------|
| `./generate_mesh.sh FILE STEP clmin clmax` | Генерация сетки |
| `./generate_mesh.sh FILE STEP clmin clmax gui` | Генерация + GUI |
| `../build/fem node.txt` | FEM расчет |
| `../build/modal node.txt modes rho h` | Модальный анализ |

---

Готово к использованию! 🚀

