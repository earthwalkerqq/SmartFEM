#!/usr/bin/env python3
"""
Скрипт для создания файла результатов в формате Gmsh для визуализации
Читает result.txt и создает .pos файл для Gmsh
"""
import sys
import os
import re

def read_result_file(result_file):
    """Читает файл результатов FEM расчета"""
    results = {
        'nodes': 0,
        'elements': 0,
        'displacements': {},  # {node_id: (u, v)}
        'stresses': {}        # {element_id: (stress_xx, stress_yy, stress_xy, stress_vm)}
    }
    
    if not os.path.exists(result_file):
        print(f"Warning: Result file not found: {result_file}")
        return results
    
    with open(result_file, 'r') as f:
        lines = f.readlines()
    
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        
        # Найти количество узлов и элементов
        if 'Число узлов' in line:
            parts = line.split()
            if len(parts) >= 3:
                try:
                    results['nodes'] = int(parts[2])
                except ValueError:
                    pass
        elif 'Число элементов' in line:
            parts = line.split()
            if len(parts) >= 3:
                try:
                    results['elements'] = int(parts[2])
                except ValueError:
                    pass
        
        # Найти перемещения
        elif 'Результат расчета перемещений' in line:
            i += 1
            while i < len(lines) and lines[i].strip() and not 'Результат расчета деформаций' in lines[i]:
                parts = lines[i].strip().split()
                if len(parts) >= 6:
                    try:
                        # Формат: u1      0.0000e+00      v1       -0.0000e+00
                        node_str = parts[0]  # u1
                        node_id = int(re.sub(r'[^0-9]', '', node_str))
                        u = float(parts[1])
                        v = float(parts[3])
                        results['displacements'][node_id] = (u, v)
                    except (ValueError, IndexError):
                        pass
                i += 1
            continue
        
        # Найти напряжения
        elif 'Результат расчета деформаций, напряжений' in line:
            i += 1
            while i < len(lines) and lines[i].strip():
                line = lines[i].strip()
                if '|' in line:
                    parts = line.split('|')
                    if len(parts) >= 2:
                        try:
                            # Левая часть: номер элемента и деформации
                            left_parts = parts[0].strip().split()
                            if len(left_parts) >= 1:
                                elem_id = int(left_parts[0])
                                
                                # Правая часть: напряжения
                                right_parts = parts[1].strip().split()
                                if len(right_parts) >= 4:
                                    stress_xx = float(right_parts[0])
                                    stress_yy = float(right_parts[1])
                                    stress_xy = float(right_parts[2])
                                    stress_zz = float(right_parts[3]) if len(right_parts) > 3 else 0.0
                                    
                                    # Эквивалентное напряжение по Мизесу
                                    stress_vm = ((stress_xx - stress_yy)**2 + 
                                               stress_yy**2 + stress_xx**2 + 
                                               6 * stress_xy**2) ** 0.5 / (2**0.5)
                                    
                                    results['stresses'][elem_id] = (stress_xx, stress_yy, stress_xy, stress_vm)
                        except (ValueError, IndexError):
                            pass
                i += 1
            break
        
        i += 1
    
    return results

def write_gmsh_pos_file(pos_file, results, node_file):
    """Записывает результаты в формат Gmsh .pos файл"""
    
    # Читаем координаты узлов из node.txt
    node_coords = {}
    if os.path.exists(node_file):
        with open(node_file, 'r') as f:
            lines = f.readlines()
            if len(lines) > 0:
                try:
                    num_nodes = int(lines[0].strip())
                    for i in range(1, min(num_nodes + 1, len(lines))):
                        parts = lines[i].strip().split()
                        if len(parts) >= 3:
                            node_id = i  # Нумерация с 1
                            x = float(parts[0])
                            y = float(parts[1])
                            z = float(parts[2]) if len(parts) > 2 else 0.0
                            node_coords[node_id] = (x, y, z)
                except ValueError:
                    pass
    
    with open(pos_file, 'w') as f:
        # Записываем перемещения узлов
        if results['displacements']:
            f.write("View \"Displacements\" {\n")
            f.write("  VP(")
            
            first = True
            for node_id in sorted(results['displacements'].keys()):
                if node_id in node_coords:
                    u, v = results['displacements'][node_id]
                    x, y, z = node_coords[node_id]
                    
                    if not first:
                        f.write(",\n    ")
                    else:
                        f.write("\n    ")
                        first = False
                    
                    # Формат: VP(номер_узла, u, v, 0)
                    f.write(f"{node_id}, {u:.6e}, {v:.6e}, 0.0")
            
            f.write("\n  );\n")
            f.write("};\n")
        
        # Записываем напряжения на элементах
        if results['stresses']:
            f.write("View \"Von Mises Stress\" {\n")
            f.write("  ST(")
            
            first = True
            for elem_id in sorted(results['stresses'].keys()):
                stress_xx, stress_yy, stress_xy, stress_vm = results['stresses'][elem_id]
                
                if not first:
                    f.write(",\n    ")
                else:
                    f.write("\n    ")
                    first = False
                
                # Формат: ST(номер_элемента, stress_vm)
                f.write(f"{elem_id}, {stress_vm:.6e}")
            
            f.write("\n  );\n")
            f.write("};\n")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: create_gmsh_results.py <result_file> <node_file> <output_pos_file>")
        sys.exit(1)
    
    result_file = sys.argv[1]
    node_file = sys.argv[2]
    pos_file = sys.argv[3]
    
    results = read_result_file(result_file)
    
    if results['nodes'] == 0 or results['elements'] == 0:
        print(f"Warning: No valid results found in {result_file}")
        print(f"  Nodes: {results['nodes']}, Elements: {results['elements']}")
        print(f"  Displacements: {len(results['displacements'])}, Stresses: {len(results['stresses'])}")
    
    write_gmsh_pos_file(pos_file, results, node_file)
    print(f"Created Gmsh post-processing file: {pos_file}")
    print(f"  Nodes with displacements: {len(results['displacements'])}")
    print(f"  Elements with stresses: {len(results['stresses'])}")

