#!/usr/bin/env python3
"""
Скрипт для записи результатов FEM расчета в формат Gmsh для визуализации
Создает .pos файл с перемещениями и напряжениями
"""
import sys
import os

def read_result_file(result_file):
    """Читает файл результатов FEM расчета"""
    results = {
        'nodes': 0,
        'elements': 0,
        'displacements': [],  # [node_id, u, v]
        'stresses': []        # [element_id, stress_xx, stress_yy, stress_xy, stress_vm]
    }
    
    with open(result_file, 'r') as f:
        lines = f.readlines()
    
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        
        # Найти количество узлов и элементов
        if 'Число узлов' in line:
            parts = line.split()
            if len(parts) >= 3:
                results['nodes'] = int(parts[2])
        elif 'Число элементов' in line:
            parts = line.split()
            if len(parts) >= 3:
                results['elements'] = int(parts[2])
        
        # Найти перемещения
        elif 'Результат расчета перемещений' in line:
            i += 1
            while i < len(lines) and lines[i].strip() and not lines[i].strip().startswith('Результат расчета деформаций'):
                parts = lines[i].strip().split()
                if len(parts) >= 6:
                    try:
                        node_id = int(parts[0][1:])  # u1 -> 1
                        u = float(parts[1])
                        v = float(parts[3])
                        results['displacements'].append((node_id, u, v))
                    except (ValueError, IndexError):
                        pass
                i += 1
            continue
        
        # Найти напряжения
        elif 'Результат расчета деформаций, напряжений' in line:
            i += 1
            while i < len(lines) and lines[i].strip():
                parts = lines[i].strip().split('|')
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
                                
                                results['stresses'].append((elem_id, stress_xx, stress_yy, stress_xy, stress_vm))
                    except (ValueError, IndexError):
                        pass
                i += 1
            break
        
        i += 1
    
    return results

def write_gmsh_pos_file(pos_file, msh_file, results):
    """Записывает результаты в формат Gmsh .pos файл"""
    with open(pos_file, 'w') as f:
        f.write("View \"Displacements\" {\n")
        
        # Записать перемещения узлов
        if results['displacements']:
            f.write("  VP(")
            for node_id, u, v in results['displacements']:
                # Формат: VP(номер_узла, u, v, 0)
                # Но нужно найти координаты узла из MSH файла
                # Пока используем упрощенный формат
                pass  # Пропустим пока, так как нужно читать MSH файл
        
        # Записать напряжения на элементах
        if results['stresses']:
            f.write("  ST(")
            for elem_id, stress_xx, stress_yy, stress_xy, stress_vm in results['stresses']:
                # Формат для напряжений на элементах
                # ST(номер_элемента, stress_xx, stress_yy, stress_xy)
                pass  # Пропустим пока
        
        f.write("};\n")
    
    # Пока просто создаем простой файл для проверки
    print(f"Created pos file: {pos_file}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: write_gmsh_results.py <result_file> <msh_file> <output_pos_file>")
        sys.exit(1)
    
    result_file = sys.argv[1]
    msh_file = sys.argv[2]
    pos_file = sys.argv[3]
    
    if not os.path.exists(result_file):
        print(f"Error: Result file not found: {result_file}")
        sys.exit(1)
    
    results = read_result_file(result_file)
    print(f"Read results: {results['nodes']} nodes, {results['elements']} elements")
    print(f"Displacements: {len(results['displacements'])}")
    print(f"Stresses: {len(results['stresses'])}")
    
    write_gmsh_pos_file(pos_file, msh_file, results)

