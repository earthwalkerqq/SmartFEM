#!/usr/bin/env python3
"""
Скрипт для интерактивного выбора узлов в Gmsh
Позволяет кликать на узлы в модели и получать их номера
"""
import sys
import os
import gmsh

def find_nearest_node(click_x, click_y, click_z, node_file):
    """Находит ближайший узел к точке клика"""
    if not os.path.exists(node_file):
        return None
    
    min_dist = float('inf')
    nearest_node = None
    
    with open(node_file, 'r') as f:
        lines = f.readlines()
        num_nodes = int(lines[0].strip())
        
        for i in range(1, min(num_nodes + 1, len(lines))):
            parts = lines[i].strip().split()
            if len(parts) >= 3:
                node_id = i  # Нумерация с 1
                x = float(parts[0])
                y = float(parts[1])
                z = float(parts[2]) if len(parts) > 2 else 0.0
                
                # Расстояние в 2D (x, y)
                dist = ((x - click_x)**2 + (y - click_y)**2)**0.5
                
                if dist < min_dist:
                    min_dist = dist
                    nearest_node = (node_id, x, y, z, dist)
    
    return nearest_node

def interactive_node_selection(msh_file, node_file, output_file):
    """Интерактивный выбор узлов в Gmsh"""
    gmsh.initialize()
    gmsh.open(msh_file)
    
    selected_nodes = []
    
    def on_mouse_click(event_type, x, y, z, button):
        """Обработчик клика мыши"""
        if event_type == "mouse_down" and button == 1:  # Левая кнопка мыши
            # Получаем координаты клика
            # В Gmsh нужно использовать gmsh.view.getCoordinates для получения координат в модели
            nearest = find_nearest_node(x, y, z, node_file)
            if nearest:
                node_id, nx, ny, nz, dist = nearest
                if dist < 1.0:  # Максимальное расстояние для выбора (настраивается)
                    if node_id not in selected_nodes:
                        selected_nodes.append(node_id)
                        print(f"Выбран узел {node_id} в точке ({nx:.2f}, {ny:.2f}, {nz:.2f})")
                        return True
        return False
    
    # Регистрируем обработчик
    gmsh.fltk.set_event_callback(on_mouse_click)
    
    # Открываем GUI
    gmsh.fltk.initialize()
    gmsh.fltk.run()
    
    # Сохраняем выбранные узлы
    with open(output_file, 'w') as f:
        for node_id in selected_nodes:
            f.write(f"{node_id}\n")
    
    gmsh.finalize()
    return selected_nodes

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: select_nodes_gmsh.py <msh_file> <node_file> <output_file>")
        sys.exit(1)
    
    msh_file = sys.argv[1]
    node_file = sys.argv[2]
    output_file = sys.argv[3]
    
    selected = interactive_node_selection(msh_file, node_file, output_file)
    print(f"Выбрано узлов: {len(selected)}")
    print(f"Список узлов сохранен в: {output_file}")

