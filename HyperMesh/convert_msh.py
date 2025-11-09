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
nodes = []  # Список кортежей (tag, x, y, z)
node_tag_to_index = {}  # Словарь: tag -> index (1-based) в node.txt
i = 0
while i < len(lines):
    line = lines[i].strip()
    if line == '$Nodes':
        i += 1
        if i < len(lines):
            num_nodes = int(lines[i].strip().split()[0])
            i += 1
            node_index = 1  # Индекс в node.txt (1-based)
            for j in range(num_nodes):
                if i < len(lines):
                    parts = lines[i].strip().split()
                    if len(parts) >= 4:
                        # Формат: номер_узла x y z
                        node_tag = int(parts[0])
                        x = float(parts[1])
                        y = float(parts[2])
                        z = float(parts[3])
                        nodes.append((node_tag, x, y, z))
                        node_tag_to_index[node_tag] = node_index
                        node_index += 1
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
                                    # Получаем теги узлов из MSH файла
                                    node_tag1 = int(parts[idx])
                                    node_tag2 = int(parts[idx+1])
                                    node_tag3 = int(parts[idx+2])
                                    
                                    # Преобразуем теги в индексы (1-based) в node.txt
                                    if node_tag1 in node_tag_to_index and node_tag2 in node_tag_to_index and node_tag3 in node_tag_to_index:
                                        node_idx1 = node_tag_to_index[node_tag1]
                                        node_idx2 = node_tag_to_index[node_tag2]
                                        node_idx3 = node_tag_to_index[node_tag3]
                                        elements.append([node_idx1, node_idx2, node_idx3])
                                    else:
                                        print(f"Warning: Node tags not found in node list: {node_tag1}, {node_tag2}, {node_tag3}", file=sys.stderr)
                        except (ValueError, IndexError) as e:
                            print(f"Warning: Error parsing element: {e}", file=sys.stderr)
                    i += 1
        break
    i += 1

if not found:
    print("Warning: $Elements section not found in MSH file", file=sys.stderr)

# Автоматическое определение граничных условий
# Находим минимальные и максимальные координаты
# nodes содержит кортежи (node_tag, x, y, z)
if nodes:
    x_coords = [n[1] for n in nodes]  # x - индекс 1
    y_coords = [n[2] for n in nodes]  # y - индекс 2
    z_coords = [n[3] for n in nodes]  # z - индекс 3
    
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
    # Формат: x y z u_flag v_flag load_flag
    # где u_flag=0 означает закрепление по U (x), v_flag=0 означает закрепление по V (y)
    # Координаты сохраняются как есть (не изменяются)
    with open(node_file, 'w') as f:
        f.write(f"{len(nodes)}\n")
        for i, (node_tag, x, y, z) in enumerate(nodes):
            # Определяем граничные условия
            # u_flag = 0.0 означает закрепление по U (x направление)
            # v_flag = 0.0 означает закрепление по V (y направление)
            # load_flag = 0.0 означает отсутствие нагрузки (по умолчанию)
            
            u_flag = 1.0  # По умолчанию узел не закреплен по U
            v_flag = 1.0  # По умолчанию узел не закреплен по V
            load_flag = 0.0  # По умолчанию нет нагрузки
            
            # Определяем, на каких границах находится узел
            dx_left = abs(x - x_min)
            dx_right = abs(x - x_max)
            dy_bottom = abs(y - y_min)
            dy_top = abs(y - y_max)
            
            on_left_edge = dx_left < tolerance_x
            on_right_edge = dx_right < tolerance_x
            on_bottom_edge = dy_bottom < tolerance_y
            on_top_edge = dy_top < tolerance_y
            
            # Закрепляем узлы на левой грани (x_min) по U
            if on_left_edge:
                u_flag = 0.0
                u_fixed_count += 1
            
            # Закрепляем узлы на нижней грани (y_min) по V
            if on_bottom_edge:
                v_flag = 0.0
                v_fixed_count += 1
            
            # Если узел на пересечении левой и нижней границ, закрепляем по обеим осям
            if on_left_edge and on_bottom_edge:
                both_fixed_count += 1
            
            # Записываем координаты (как есть) и флаги граничных условий
            f.write(f"{x} {y} {z} {u_flag} {v_flag} {load_flag}\n")
        
        f.write(f"{len(elements)}\n")
        for elem in elements:
            f.write(f"{elem[0]} {elem[1]} {elem[2]}\n")
else:
    # Если узлов нет, записываем как есть (без граничных условий)
    with open(node_file, 'w') as f:
        f.write(f"{len(nodes)}\n")
        for node_tag, x, y, z in nodes:
            # Записываем координаты и граничные условия по умолчанию (все узлы свободны)
            f.write(f"{x} {y} {z} 1.0 1.0 0.0\n")
        f.write(f"{len(elements)}\n")
        for elem in elements:
            f.write(f"{elem[0]} {elem[1]} {elem[2]}\n")

print(f"Converted to node.txt: {len(nodes)} nodes, {len(elements)} elements")
if nodes:
    print(f"Boundary conditions: U_fixed={u_fixed_count}, V_fixed={v_fixed_count}, Both_fixed={both_fixed_count}", file=sys.stderr)

