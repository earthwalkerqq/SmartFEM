#!/usr/bin/env python3
"""
Скрипт для интерактивного выбора узлов в Gmsh
Использует Gmsh Python API для перехвата кликов и выбора узлов
"""
import sys
import os

try:
    import gmsh
except ImportError:
    print("Gmsh Python API не установлен. Используйте альтернативный метод выбора узлов.")
    sys.exit(1)

def find_nearest_node(click_x, click_y, click_z, node_file, tolerance=0.5):
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
            if len(parts) >= 2:
                node_id = i  # Нумерация с 1
                x = float(parts[0])
                y = float(parts[1])
                z = float(parts[2]) if len(parts) > 2 else 0.0
                
                # Расстояние в 2D (x, y)
                dist = ((x - click_x)**2 + (y - click_y)**2)**0.5
                
                if dist < min_dist and dist < tolerance:
                    min_dist = dist
                    nearest_node = (node_id, x, y, z, dist)
    
    return nearest_node

def interactive_node_selection(msh_file, node_file, output_file):
    """Интерактивный выбор узлов в Gmsh"""
    gmsh.initialize()
    gmsh.open(msh_file)
    
    selected_nodes = []
    
    # Включаем отображение узлов
    gmsh.option.setNumber("Mesh.NodeLabels", 1)
    gmsh.option.setNumber("Mesh.Nodes", 1)
    
    def on_mouse_click(event_type, x, y, z, button):
        """Обработчик клика мыши"""
        if event_type == "mouse_down" and button == 1:  # Левая кнопка мыши
            # Получаем координаты клика в модели
            # В Gmsh нужно использовать gmsh.view.getCoordinates для получения координат в модели
            # Но проще использовать координаты из окна
            nearest = find_nearest_node(x, y, z, node_file, tolerance=1.0)
            if nearest:
                node_id, nx, ny, nz, dist = nearest
                if node_id not in selected_nodes:
                    selected_nodes.append(node_id)
                    print(f"Выбран узел {node_id} в точке ({nx:.2f}, {ny:.2f}, {nz:.2f}), расстояние: {dist:.3f}")
                    # Сохраняем сразу
                    with open(output_file, 'w') as f:
                        for nid in selected_nodes:
                            f.write(f"{nid}\n")
                    return True
        return False
    
    # Регистрируем обработчик
    gmsh.fltk.set_event_callback(on_mouse_click)
    
    # Открываем GUI
    gmsh.fltk.initialize()
    print("Gmsh открыт. Кликайте на узлы для выбора.")
    print("Выбранные узлы будут автоматически сохраняться.")
    print("Закройте Gmsh для завершения выбора.")
    
    gmsh.fltk.run()
    
    # Сохраняем выбранные узлы
    with open(output_file, 'w') as f:
        for node_id in selected_nodes:
            f.write(f"{node_id}\n")
    
    gmsh.finalize()
    return selected_nodes

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: gmsh_select_nodes.py <msh_file> <node_file> <output_file>")
        sys.exit(1)
    
    msh_file = sys.argv[1]
    node_file = sys.argv[2]
    output_file = sys.argv[3]
    
    try:
        selected = interactive_node_selection(msh_file, node_file, output_file)
        print(f"\nВыбрано узлов: {len(selected)}")
        print(f"Список узлов сохранен в: {output_file}")
    except Exception as e:
        print(f"Ошибка: {e}")
        sys.exit(1)

