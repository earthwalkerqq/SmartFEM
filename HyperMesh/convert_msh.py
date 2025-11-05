#!/usr/bin/env python3
import sys

if len(sys.argv) < 3:
    print("Usage: convert_msh.py <msh_file> <node_file>")
    sys.exit(1)

msh_file = sys.argv[1]
node_file = sys.argv[2]

with open(msh_file, 'r') as f:
    lines = f.readlines()

# Найти секцию узлов
nodes = []
i = 0
while i < len(lines):
    line = lines[i].strip()
    if line == '$Nodes':
        i += 1
        if i < len(lines):
            num_nodes = int(lines[i].strip().split()[0])
            i += 1
            for j in range(num_nodes):
                if i < len(lines):
                    parts = lines[i].strip().split()
                    if len(parts) >= 4:
                        # Формат: номер_узла x y z
                        nodes.append((float(parts[1]), float(parts[2]), float(parts[3])))
                    i += 1
        break
    i += 1

# Найти секцию элементов (треугольники)
elements = []
i = 0
found = False
while i < len(lines):
    line = lines[i].strip()
    if '$Elements' in line:
        found = True
        i += 1
        if i < len(lines):
            num_elem = int(lines[i].strip().split()[0])
            i += 1
            for j in range(num_elem):
                if i < len(lines):
                    parts = lines[i].strip().split()
                    if len(parts) >= 4:
                        try:
                            elem_type = int(parts[1])
                            if elem_type == 2:  # тип 2 = треугольник
                                # Формат: номер_элемента тип_элемента количество_тегов тег1 тег2 ... узел1 узел2 узел3
                                num_tags = int(parts[2])
                                idx = 3 + num_tags
                                # Проверить, что есть достаточно элементов для узлов
                                if len(parts) >= idx + 3:
                                    elem_nodes = [int(parts[idx]), int(parts[idx+1]), int(parts[idx+2])]
                                    elements.append(elem_nodes)
                        except (ValueError, IndexError):
                            pass
                    i += 1
        break
    i += 1

if not found:
    print("Warning: $Elements section not found in MSH file", file=sys.stderr)

# Автоматическое определение граничных условий
# Находим минимальные и максимальные координаты
if nodes:
    x_coords = [n[0] for n in nodes]
    y_coords = [n[1] for n in nodes]
    z_coords = [n[2] for n in nodes]
    
    x_min, x_max = min(x_coords), max(x_coords)
    y_min, y_max = min(y_coords), max(y_coords)
    z_min, z_max = min(z_coords), max(z_coords)
    
    # Определяем узлы на границах для закрепления
    # Закрепляем узлы на нижней грани (y_min) и левой грани (x_min)
    # Используем допуск для учета погрешностей с плавающей точкой
    # Используем процент от размера модели для более надежного определения границ
    x_range = x_max - x_min
    y_range = y_max - y_min
    # Увеличиваем tolerance для захвата большего количества узлов на границах
    tolerance_x = max(1e-6, x_range * 0.05)  # 5% от размера или минимум 1e-6
    tolerance_y = max(1e-6, y_range * 0.05)
    
    # Счетчики для отладки
    u_fixed_count = 0
    v_fixed_count = 0
    both_fixed_count = 0
    
    # Записать в формат node.txt с граничными условиями
    with open(node_file, 'w') as f:
        f.write(f"{len(nodes)}\n")
        for i, (x, y, z) in enumerate(nodes):
            # Определяем граничные условия
            # car[0][i] == 0 означает закрепление по U (x)
            # car[1][i] == 0 означает закрепление по V (y)
            # Важно: координаты сохраняются как есть, но для узлов на границах
            # устанавливаем координату в 0 для указания закрепления
            
            x_bc = x
            y_bc = y
            
            # Определяем, на каких границах находится узел
            on_left_edge = abs(x - x_min) < tolerance_x
            on_right_edge = abs(x - x_max) < tolerance_x
            on_bottom_edge = abs(y - y_min) < tolerance_y
            on_top_edge = abs(y - y_max) < tolerance_y
            
            # Закрепляем узлы на левой грани (x_min) по U
            # Устанавливаем координату x в 0 для указания закрепления
            if on_left_edge:
                x_bc = 0.0
                u_fixed_count += 1
            
            # Закрепляем узлы на нижней грани (y_min) по V
            # Устанавливаем координату y в 0 для указания закрепления
            if on_bottom_edge:
                y_bc = 0.0
                v_fixed_count += 1
            
            # Если узел на пересечении левой и нижней границ, закрепляем по обеим осям
            if on_left_edge and on_bottom_edge:
                both_fixed_count += 1
            
            f.write(f"{x_bc} {y_bc} {z}\n")
        
        f.write(f"{len(elements)}\n")
        for elem in elements:
            f.write(f"{elem[0]} {elem[1]} {elem[2]}\n")
else:
    # Если узлов нет, записываем как есть
    with open(node_file, 'w') as f:
        f.write(f"{len(nodes)}\n")
        for x, y, z in nodes:
            f.write(f"{x} {y} {z}\n")
        f.write(f"{len(elements)}\n")
        for elem in elements:
            f.write(f"{elem[0]} {elem[1]} {elem[2]}\n")

print(f"Converted to node.txt: {len(nodes)} nodes, {len(elements)} elements")
if nodes:
    print(f"Boundary conditions: U_fixed={u_fixed_count}, V_fixed={v_fixed_count}, Both_fixed={both_fixed_count}", file=sys.stderr)

