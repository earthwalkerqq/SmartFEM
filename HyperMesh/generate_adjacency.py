#!/usr/bin/env python3
"""
Генерация файла с координатами узлов и смежными элементами
Формат вывода:
- Строка 1: количество узлов
- Далее для каждого узла: номер_узла x y z количество_смежных_элементов список_элементов
"""

import sys

if len(sys.argv) < 3:
    print("Usage: generate_adjacency.py <msh_file> <output_file>")
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
                        node_id = int(parts[0])
                        x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
                        nodes.append((node_id, x, y, z))
                    i += 1
        break
    i += 1

# Найти секцию элементов (треугольники)
elements = []
elem_nodes_dict = {}  # словарь: node_id -> список элементов
i = 0
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
                                num_tags = int(parts[2])
                                idx = 3 + num_tags
                                if len(parts) >= idx + 3:
                                    elem_id = int(parts[0])
                                    node1 = int(parts[idx])
                                    node2 = int(parts[idx+1])
                                    node3 = int(parts[idx+2])
                                    elem_nodes = [node1, node2, node3]
                                    elements.append((elem_id, elem_nodes))
                                    
                                    # Добавить элемент в список смежных для каждого узла
                                    for node_id in elem_nodes:
                                        if node_id not in elem_nodes_dict:
                                            elem_nodes_dict[node_id] = []
                                        elem_nodes_dict[node_id].append(elem_id)
                        except (ValueError, IndexError):
                            pass
                    i += 1
        break
    i += 1

# Создать словарь узлов по ID для быстрого доступа
nodes_dict = {node_id: (x, y, z) for node_id, x, y, z in nodes}

# Записать в файл с координатами и смежными элементами (максимум 3 элемента на узел)
with open(node_file, 'w') as f:
    f.write(f"{len(nodes)}\n")
    # Сортируем узлы по ID
    for node_id, x, y, z in sorted(nodes):
        adj_elements = elem_nodes_dict.get(node_id, [])
        # Берем только первые 3 элемента
        adj_elements_limited = sorted(adj_elements)[:3]
        num_adj = len(adj_elements_limited)
        f.write(f"{node_id} {x} {y} {z} {num_adj}")
        for elem_id in adj_elements_limited:
            f.write(f" {elem_id}")
        f.write("\n")
    
    # Записать элементы
    f.write(f"{len(elements)}\n")
    for elem_id, elem_nodes in sorted(elements):
        f.write(f"{elem_nodes[0]} {elem_nodes[1]} {elem_nodes[2]}\n")

print(f"Generated adjacency file: {len(nodes)} nodes, {len(elements)} elements")
print(f"Output: {node_file}")

