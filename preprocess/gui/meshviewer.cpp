#include "meshviewer.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTimer>
#include <QtGui/QOpenGLContext>
#include <QKeyEvent>
#include <QRect>
#include <cmath>
#include <QtMath>
#include <OpenGL/glu.h>  // Для gluPerspective
#include <QVector3D>

MeshViewer::MeshViewer(QWidget *parent)
    : QOpenGLWidget(parent),
      is3D(false),
      zoom(1.0),
      panX(0.0),
      panY(0.0),
      rotationX(0.0),
      rotationY(0.0),
      rotationZ(0.0),
      minX(0.0),
      maxX(1.0),
      minY(0.0),
      maxY(1.0),
      minZ(0.0),
      maxZ(1.0),
      isPanning(false),
      isRotating(false),
      isSelecting(false),
      matricesValid(false),
      showNodes(true),
      showElements(true),
      showNodeLabels(false) {
    setMinimumSize(1, 1);  // Минимальный размер 1x1, чтобы виджет мог сжиматься
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);  // Растягивается во все стороны
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);  // Включаем фокус для обработки клавиатуры
}

MeshViewer::~MeshViewer() {
    makeCurrent();
    doneCurrent();
}

void MeshViewer::initializeGL() {
    initializeOpenGLFunctions();
    
    // Настройка OpenGL для 3D визуализации
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);  // Темный фон
    glEnable(GL_DEPTH_TEST);  // Включаем тест глубины для 3D
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    
    // Отключаем освещение для простоты (можно включить позже)
    glDisable(GL_LIGHTING);
}

void MeshViewer::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    // Пересчитываем zoom при изменении размера, чтобы модель заполняла окно и была по центру
    if (minX < maxX && minY < maxY && w > 0 && h > 0) {
        double modelWidth = maxX - minX;
        double modelHeight = maxY - minY;
        double modelSize = is3D ? qMax(qMax(modelWidth, modelHeight), maxZ - minZ) : qMax(modelWidth, modelHeight);
        
        // Вычисляем zoom для 3D перспективы с учетом aspect ratio
        double widgetAspect = (double)w / (double)h;
        double modelAspect = modelWidth / modelHeight;
        
        // Используем эффективный размер модели для правильного масштабирования
        double effectiveModelSize = modelSize;
        if (widgetAspect > modelAspect) {
            effectiveModelSize = modelHeight;
        } else {
            effectiveModelSize = modelWidth;
        }
        
        // Вычисляем zoom для перспективной проекции
        double fov = 45.0;
        double padding = 0.1;  // 10% padding для центрирования и лучшей видимости
        double targetSize = effectiveModelSize * (1.0 + padding);
        double targetDistance = (targetSize / 2.0) / tan((fov * M_PI / 180.0) / 2.0);
        double effectiveBaseDistance = (effectiveModelSize / 2.0) / tan((fov * M_PI / 180.0) / 2.0);
        zoom = effectiveBaseDistance / targetDistance;
        
        // Ограничиваем zoom разумными значениями для предотвращения "улетания" модели
        zoom = qBound(0.5, zoom, 15.0);  // Уменьшили диапазон для более контролируемого масштабирования
    }
    updateViewport();
}

void MeshViewer::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Устанавливаем viewport на весь виджет
    // width() и height() возвращают размеры виджета в пикселях
    // Если они маленькие, значит виджет занимает мало места в layout
    int viewportWidth = width();
    int viewportHeight = height();
    
    // Убеждаемся, что viewport не меньше минимального размера
    if (viewportWidth < 100) viewportWidth = 100;
    if (viewportHeight < 100) viewportHeight = 100;
    
    glViewport(0, 0, viewportWidth, viewportHeight);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // Вычисляем размеры модели
    double modelWidth = maxX - minX;
    double modelHeight = maxY - minY;
    
    if (modelWidth <= 0 || modelHeight <= 0) {
        // Если модель не загружена, используем значения по умолчанию
        glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
        return;
    }
    
    // Вычисляем aspect ratio
    double widgetAspect = (double)width() / (double)height();
    double modelAspect = modelWidth / modelHeight;
    
    // Центр модели (геометрический центр)
    double modelCenterX = (minX + maxX) / 2.0;
    double modelCenterY = (minY + maxY) / 2.0;
    double modelCenterZ = is3D ? (minZ + maxZ) / 2.0 : 0.0;
    
    // Отладочный вывод для проверки центрирования
    static int debugCount = 0;
    if (debugCount++ < 5) {
        qDebug() << "=== DEBUG CENTERING ===";
        qDebug() << "Viewport size:" << width() << "x" << height();
        qDebug() << "Model bounds: X=[" << minX << "," << maxX << "], Y=[" << minY << "," << maxY << "]";
        if (is3D) qDebug() << "Model bounds Z: [" << minZ << "," << maxZ << "]";
        qDebug() << "Model center: (" << modelCenterX << "," << modelCenterY << "," << modelCenterZ << ")";
        qDebug() << "Model size: " << modelWidth << "x" << modelHeight;
    }
    
    // Вычисляем размеры модели для 3D визуализации
    double modelSize = is3D ? qMax(qMax(modelWidth, modelHeight), maxZ - minZ) : qMax(modelWidth, modelHeight);
    
    // Используем перспективную проекцию для 3D визуализации
    double fov = 45.0;  // Поле зрения в градусах
    double nearPlane = modelSize * 0.01;
    double farPlane = modelSize * 100.0;
    
    // Вычисляем расстояние камеры так, чтобы модель заполняла весь экран и была по центру
    // Используем zoom для масштабирования
    // Для перспективной проекции: distance = (modelSize / 2) / tan(fov/2) / zoom
    double baseDistance = (modelSize / 2.0) / tan((fov * M_PI / 180.0) / 2.0);
    
    // Учитываем aspect ratio для правильного заполнения экрана
    // Корректируем расстояние так, чтобы модель заполняла весь viewport и была симметричной
    // Используем больший размер модели (ширина или высота) для правильного масштабирования
    double effectiveModelSize = modelSize;
    if (widgetAspect > modelAspect) {
        // Виджет шире модели - используем высоту модели для правильного заполнения
        effectiveModelSize = modelHeight;
    } else {
        // Виджет выше модели - используем ширину модели для правильного заполнения
        effectiveModelSize = modelWidth;
    }
    
    // Пересчитываем расстояние с учетом эффективного размера модели
    double effectiveDistance = (effectiveModelSize / 2.0) / tan((fov * M_PI / 180.0) / 2.0);
    double distance = effectiveDistance / zoom;
    
    // Добавляем небольшой padding для лучшей видимости и центрирования (5%)
    distance *= 1.05;
    
    // Настраиваем перспективную проекцию
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fov, widgetAspect, nearPlane, farPlane);
    
    // Настраиваем камеру и 3D трансформации (как в CAD)
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // ПРОСТОЙ И ПРАВИЛЬНЫЙ ПОДХОД: камера всегда смотрит на центр модели
    // Используем стандартный trackball подход - камера вращается вокруг центра модели
    
    // Нормализуем углы для непрерывного вращения
    double normX = fmod(rotationX, 360.0);
    if (normX < 0.0) normX += 360.0;
    double normY = fmod(rotationY, 360.0);
    if (normY < 0.0) normY += 360.0;
    double normZ = fmod(rotationZ, 360.0);
    if (normZ < 0.0) normZ += 360.0;
    
    // Вычисляем позицию камеры на сфере вокруг центра модели
    // Используем сферические координаты для простоты
    // theta - угол вокруг Y (горизонтальное вращение)
    // phi - угол от вертикали (вертикальное вращение)
    // Начальная позиция: камера смотрит на модель спереди-сверху
    double theta = normY * M_PI / 180.0;  // Горизонтальное вращение
    double phi = (normX + 45.0) * M_PI / 180.0;  // Вертикальное вращение (45 градусов для вида сверху-спереди)
    
    // Позиция камеры на сфере вокруг центра модели
    // Используем стандартные сферические координаты:
    // x = r * sin(phi) * cos(theta)
    // y = r * cos(phi)
    // z = r * sin(phi) * sin(theta)
    double eyeX = modelCenterX + distance * sin(phi) * cos(theta);
    double eyeY = modelCenterY + distance * cos(phi);
    double eyeZ = modelCenterZ + distance * sin(phi) * sin(theta);
    
    // Камера всегда смотрит на центр модели
    double centerX = modelCenterX;
    double centerY = modelCenterY;
    double centerZ = modelCenterZ;
    
    // Вектор "вверх" для камеры (всегда направлен вверх в мировых координатах)
    // Но нужно его повернуть с учетом вращения вокруг Z
    double upX = 0.0;
    double upY = 1.0;
    double upZ = 0.0;
    
    // Применяем вращение вокруг Z к вектору "вверх"
    double radZ = normZ * M_PI / 180.0;
    double tempX = upX * cos(radZ) - upY * sin(radZ);
    double tempY = upX * sin(radZ) + upY * cos(radZ);
    upX = tempX;
    upY = tempY;
    
    // Используем gluLookAt для правильной настройки камеры
    // Камера смотрит на центр модели из позиции eyeX, eyeY, eyeZ
    gluLookAt(eyeX, eyeY, eyeZ,  // Позиция камеры
              centerX, centerY, centerZ,  // Точка, на которую смотрит камера (центр модели)
              upX, upY, upZ);  // Вектор "вверх" для камеры
    
    // Рисуем сетку координат для лучшей ориентации
    drawGrid();
    
    // Рисуем элементы, узлы и выделенные узлы
    drawElements();
    drawNodes();
    drawSelectedNodes();
    drawFixedNodes();  // Рисуем закрепленные узлы
    drawLoadArrows();  // Рисуем стрелки сил
    
    // Сохраняем матрицы для правильного преобразования координат в событиях мыши
    glGetDoublev(GL_MODELVIEW_MATRIX, savedModelview);
    glGetDoublev(GL_PROJECTION_MATRIX, savedProjection);
    glGetIntegerv(GL_VIEWPORT, savedViewport);
    matricesValid = true;
    
    // Рисуем область выбора, если идет выбор узлов
    if (isSelecting) {
        drawSelectionRect();
    }
}

void MeshViewer::drawGrid() {
    // Рисуем координатные оси для 3D ориентации (как на изображении)
    if (is3D) {
        // Рисуем координатные оси в правом нижнем углу
        glLineWidth(3.0f);
        
        // Вычисляем размер модели для масштабирования осей
        double modelSize = qMax(qMax(maxX - minX, maxY - minY), maxZ - minZ);
        double axisLength = modelSize * 0.15;  // 15% от размера модели
        
        // Позиция начала осей (правый нижний угол)
        double axisX = maxX - modelSize * 0.1;
        double axisY = minY + modelSize * 0.1;
        double axisZ = minZ;
        
        // Ось X (красная)
        glColor3f(1.0f, 0.0f, 0.0f);
        glBegin(GL_LINES);
        glVertex3d(axisX, axisY, axisZ);
        glVertex3d(axisX + axisLength, axisY, axisZ);
        glEnd();
        
        // Ось Y (зеленая)
        glColor3f(0.0f, 1.0f, 0.0f);
        glBegin(GL_LINES);
        glVertex3d(axisX, axisY, axisZ);
        glVertex3d(axisX, axisY + axisLength, axisZ);
        glEnd();
        
        // Ось Z (синяя)
        glColor3f(0.0f, 0.0f, 1.0f);
        glBegin(GL_LINES);
        glVertex3d(axisX, axisY, axisZ);
        glVertex3d(axisX, axisY, axisZ + axisLength);
        glEnd();
    } else {
        // Для 2D моделей рисуем обычную сетку
        if (minX >= maxX || minY >= maxY) return;
        
        glColor3f(0.15f, 0.15f, 0.15f);  // Темно-серая сетка
        glLineWidth(0.5f);
        glBegin(GL_LINES);
        
        // Вертикальные линии
        int numLines = 20;  // Больше линий для лучшей видимости
        for (int i = 0; i <= numLines; i++) {
            double x = minX + (maxX - minX) * i / numLines;
            glVertex3d(x, minY, 0.0);
            glVertex3d(x, maxY, 0.0);
        }
        
        // Горизонтальные линии
        for (int i = 0; i <= numLines; i++) {
            double y = minY + (maxY - minY) * i / numLines;
            glVertex3d(minX, y, 0.0);
            glVertex3d(maxX, y, 0.0);
        }
        
        glEnd();
    }
}

void MeshViewer::drawElements() {
    if (!showElements) return;
    
    // Если элементов нет, рисуем линии между соседними узлами
    if (elements.isEmpty()) {
        // Рисуем простые линии между узлами (для отладки)
        glColor3f(0.4f, 0.6f, 0.9f);  // Светло-синий цвет
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        
        // Рисуем линии между узлами, которые близко друг к другу
        QList<int> nodeIds = nodeCoords.keys();
        for (int i = 0; i < nodeIds.size(); i++) {
            for (int j = i + 1; j < nodeIds.size(); j++) {
                int nodeId1 = nodeIds[i];
                int nodeId2 = nodeIds[j];
                QPair<double, double> p1 = nodeCoords[nodeId1];
                QPair<double, double> p2 = nodeCoords[nodeId2];
                
                double dx = p2.first - p1.first;
                double dy = p2.second - p1.second;
                double dist = std::sqrt(dx*dx + dy*dy);
                double maxDist = qMin(maxX - minX, maxY - minY) * 0.05;  // 5% от размера модели
                
                if (dist < maxDist) {
                    glVertex3d(p1.first, p1.second, 0.0);
                    glVertex3d(p2.first, p2.second, 0.0);
                }
            }
        }
        
        glEnd();
        return;
    }
    
    // Рисуем элементы более ярким цветом и толще для лучшей видимости
    glColor3f(0.3f, 0.5f, 0.8f);  // Светло-синий цвет для элементов (улучшенный контраст)
    glLineWidth(3.0f);  // Увеличиваем толщину линий (было 2.5)
    glBegin(GL_LINES);
    
    for (const QList<int> &elem : elements) {
        if (elem.size() >= 3) {
            // Draw triangle
            for (int i = 0; i < 3; i++) {
                int nodeId1 = elem[i];
                int nodeId2 = elem[(i + 1) % 3];
                
                // Проверяем наличие узлов в 3D или 2D формате
                bool hasNode1_3D = is3D && nodeCoords3D.contains(nodeId1);
                bool hasNode2_3D = is3D && nodeCoords3D.contains(nodeId2);
                bool hasNode1_2D = nodeCoords.contains(nodeId1);
                bool hasNode2_2D = nodeCoords.contains(nodeId2);
                
                if (hasNode1_3D && hasNode2_3D) {
                    QVector3D p1 = nodeCoords3D[nodeId1];
                    QVector3D p2 = nodeCoords3D[nodeId2];
                    glVertex3d(p1.x(), p1.y(), p1.z());
                    glVertex3d(p2.x(), p2.y(), p2.z());
                } else if (hasNode1_2D && hasNode2_2D) {
                    QPair<double, double> p1 = nodeCoords[nodeId1];
                    QPair<double, double> p2 = nodeCoords[nodeId2];
                    glVertex3d(p1.first, p1.second, 0.0);
                    glVertex3d(p2.first, p2.second, 0.0);
                }
            }
        }
    }
    
    glEnd();
}

void MeshViewer::drawNodes() {
    if (!showNodes || (nodeCoords.isEmpty() && nodeCoords3D.isEmpty())) return;
    
    // Рисуем узлы более ярким цветом и больше для лучшей видимости
    glColor3f(0.2f, 0.7f, 1.0f);  // Яркий голубой цвет для узлов (улучшенный контраст)
    glPointSize(16.0f);  // Увеличенный размер узлов для лучшей видимости и выбора
    glBegin(GL_POINTS);
    
    if (is3D && !nodeCoords3D.isEmpty()) {
        // Рисуем 3D узлы
        for (auto it = nodeCoords3D.begin(); it != nodeCoords3D.end(); ++it) {
            if (!selectedNodes.contains(it.key())) {
                QVector3D pos = it.value();
                glVertex3d(pos.x(), pos.y(), pos.z());
            }
        }
    } else {
        // Рисуем 2D узлы
        for (auto it = nodeCoords.begin(); it != nodeCoords.end(); ++it) {
            if (!selectedNodes.contains(it.key())) {
                glVertex3d(it.value().first, it.value().second, 0.0);
            }
        }
    }
    
    glEnd();
}

void MeshViewer::drawSelectedNodes() {
    if (selectedNodes.isEmpty()) return;
    
    // Выделенные узлы - ярко-красные и крупнее для лучшей видимости
    glColor3f(1.0f, 0.2f, 0.2f);  // Яркий красный (улучшенный контраст)
    glPointSize(24.0f);  // Увеличенный размер выделенных узлов для лучшей видимости
    glBegin(GL_POINTS);
    
    if (is3D && !nodeCoords3D.isEmpty()) {
        // Рисуем 3D выделенные узлы
        for (int nodeId : selectedNodes) {
            if (nodeCoords3D.contains(nodeId)) {
                QVector3D pos = nodeCoords3D[nodeId];
                glVertex3d(pos.x(), pos.y(), pos.z());
            }
        }
    } else {
        // Рисуем 2D выделенные узлы
        for (int nodeId : selectedNodes) {
            if (nodeCoords.contains(nodeId)) {
                QPair<double, double> coords = nodeCoords[nodeId];
                glVertex3d(coords.first, coords.second, 0.0);
            }
        }
    }
    
    glEnd();
    
    // Рисуем обводку вокруг выделенных узлов для еще лучшей видимости
    glColor3f(1.0f, 1.0f, 1.0f);  // Белая обводка
    glPointSize(28.0f);  // Еще больше для обводки
    glBegin(GL_POINTS);
    
    if (is3D && !nodeCoords3D.isEmpty()) {
        // Рисуем 3D обводку
        for (int nodeId : selectedNodes) {
            if (nodeCoords3D.contains(nodeId)) {
                QVector3D pos = nodeCoords3D[nodeId];
                glVertex3d(pos.x(), pos.y(), pos.z());
            }
        }
    } else {
        // Рисуем 2D обводку
        for (int nodeId : selectedNodes) {
            if (nodeCoords.contains(nodeId)) {
                QPair<double, double> coords = nodeCoords[nodeId];
                glVertex3d(coords.first, coords.second, 0.0);
            }
        }
    }
    
    glEnd();
}

void MeshViewer::drawFixedNodes() {
    // Рисуем закрепленные узлы зеленым цветом
    if (fixedNodesU.isEmpty() && fixedNodesV.isEmpty()) return;
    
    // Закрепленные узлы - зеленые
    glColor3f(0.0f, 1.0f, 0.0f);  // Зеленый цвет
    glPointSize(20.0f);  // Размер закрепленных узлов
    glBegin(GL_POINTS);
    
    if (is3D && !nodeCoords3D.isEmpty()) {
        // Рисуем 3D закрепленные узлы
        for (int nodeId : fixedNodesU) {
            if (nodeCoords3D.contains(nodeId)) {
                QVector3D pos = nodeCoords3D[nodeId];
                glVertex3d(pos.x(), pos.y(), pos.z());
            }
        }
        for (int nodeId : fixedNodesV) {
            if (nodeCoords3D.contains(nodeId)) {
                QVector3D pos = nodeCoords3D[nodeId];
                glVertex3d(pos.x(), pos.y(), pos.z());
            }
        }
    } else {
        // Рисуем 2D закрепленные узлы
        for (int nodeId : fixedNodesU) {
            if (nodeCoords.contains(nodeId)) {
                QPair<double, double> coords = nodeCoords[nodeId];
                glVertex3d(coords.first, coords.second, 0.0);
            }
        }
        for (int nodeId : fixedNodesV) {
            if (nodeCoords.contains(nodeId)) {
                QPair<double, double> coords = nodeCoords[nodeId];
                glVertex3d(coords.first, coords.second, 0.0);
            }
        }
    }
    
    glEnd();
}

void MeshViewer::drawLoadArrows() {
    // Рисуем красные стрелки для сил
    if (loadNodes.isEmpty()) return;
    
    glColor3f(1.0f, 0.0f, 0.0f);  // Красный цвет для стрелок
    glLineWidth(3.0f);  // Толщина стрелок
    
    // Вычисляем масштаб для стрелок (примерно 10% от размера модели)
    double modelSize = qMax(maxX - minX, maxY - minY);
    double arrowScale = modelSize * 0.1;  // Масштаб стрелок
    
    if (is3D && !nodeCoords3D.isEmpty()) {
        // Рисуем 3D стрелки
        for (auto it = loadNodes.begin(); it != loadNodes.end(); ++it) {
            int nodeId = it.key();
            if (!nodeCoords3D.contains(nodeId)) continue;
            
            QVector3D nodePos = nodeCoords3D[nodeId];
            double fx = it.value().first;
            double fy = it.value().second;
            
            // Вычисляем длину и направление силы
            double forceLength = sqrt(fx * fx + fy * fy);
            if (forceLength < 1e-10) continue;  // Пропускаем нулевые силы
            
            // Нормализуем направление
            double dirX = fx / forceLength;
            double dirY = fy / forceLength;
            
            // Масштабируем длину стрелки
            double arrowLength = arrowScale * (forceLength / 1000.0);  // Нормализуем силу
            arrowLength = qBound(arrowScale * 0.1, arrowLength, arrowScale * 2.0);  // Ограничиваем длину
            
            // Конец стрелки
            double endX = nodePos.x() + dirX * arrowLength;
            double endY = nodePos.y() + dirY * arrowLength;
            double endZ = nodePos.z();
            
            // Рисуем стрелку
            glBegin(GL_LINES);
            glVertex3d(nodePos.x(), nodePos.y(), nodePos.z());
            glVertex3d(endX, endY, endZ);
            glEnd();
            
            // Рисуем наконечник стрелки (треугольник)
            double arrowHeadSize = arrowLength * 0.2;
            double perpX = -dirY;
            double perpY = dirX;
            
            glBegin(GL_TRIANGLES);
            glVertex3d(endX, endY, endZ);
            glVertex3d(endX - dirX * arrowHeadSize + perpX * arrowHeadSize * 0.5,
                       endY - dirY * arrowHeadSize + perpY * arrowHeadSize * 0.5, endZ);
            glVertex3d(endX - dirX * arrowHeadSize - perpX * arrowHeadSize * 0.5,
                       endY - dirY * arrowHeadSize - perpY * arrowHeadSize * 0.5, endZ);
            glEnd();
        }
    } else {
        // Рисуем 2D стрелки
        for (auto it = loadNodes.begin(); it != loadNodes.end(); ++it) {
            int nodeId = it.key();
            if (!nodeCoords.contains(nodeId)) continue;
            
            QPair<double, double> nodePos = nodeCoords[nodeId];
            double fx = it.value().first;
            double fy = it.value().second;
            
            // Вычисляем длину и направление силы
            double forceLength = sqrt(fx * fx + fy * fy);
            if (forceLength < 1e-10) continue;  // Пропускаем нулевые силы
            
            // Нормализуем направление
            double dirX = fx / forceLength;
            double dirY = fy / forceLength;
            
            // Масштабируем длину стрелки
            double arrowLength = arrowScale * (forceLength / 1000.0);  // Нормализуем силу
            arrowLength = qBound(arrowScale * 0.1, arrowLength, arrowScale * 2.0);  // Ограничиваем длину
            
            // Конец стрелки
            double endX = nodePos.first + dirX * arrowLength;
            double endY = nodePos.second + dirY * arrowLength;
            
            // Рисуем стрелку
            glBegin(GL_LINES);
            glVertex3d(nodePos.first, nodePos.second, 0.0);
            glVertex3d(endX, endY, 0.0);
            glEnd();
            
            // Рисуем наконечник стрелки (треугольник)
            double arrowHeadSize = arrowLength * 0.2;
            double perpX = -dirY;
            double perpY = dirX;
            
            glBegin(GL_TRIANGLES);
            glVertex3d(endX, endY, 0.0);
            glVertex3d(endX - dirX * arrowHeadSize + perpX * arrowHeadSize * 0.5,
                       endY - dirY * arrowHeadSize + perpY * arrowHeadSize * 0.5, 0.0);
            glVertex3d(endX - dirX * arrowHeadSize - perpX * arrowHeadSize * 0.5,
                       endY - dirY * arrowHeadSize - perpY * arrowHeadSize * 0.5, 0.0);
            glEnd();
        }
    }
}

void MeshViewer::drawSelectionRect() {
    // Рисуем прямоугольную область выбора узлов
    if (!isSelecting) return;
    
    // Вычисляем прямоугольную область
    QRect selectionRect = QRect(selectionStart, selectionEnd).normalized();
    
    if (selectionRect.width() < 2 || selectionRect.height() < 2) {
        return;  // Слишком маленькая область
    }
    
    // Переключаемся в режим 2D для рисования прямоугольника
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    // OpenGL координаты: (0,0) внизу слева, (width, height) вверху справа
    // Qt координаты: (0,0) вверху слева, (width, height) внизу справа
    // Поэтому инвертируем Y: glOrtho(0, width(), 0, height(), -1, 1) не работает
    // Нужно использовать glOrtho(0, width(), height(), 0, -1, 1) для соответствия Qt координатам
    glOrtho(0, width(), height(), 0, -1, 1);  // Экранные координаты (Qt стиль: Y сверху вниз)
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Отключаем глубину для рисования поверх всего
    glDisable(GL_DEPTH_TEST);
    
    // Рисуем полупрозрачный прямоугольник
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Заливка прямоугольника (полупрозрачная)
    glColor4f(0.3f, 0.5f, 1.0f, 0.2f);  // Голубой с прозрачностью
    glBegin(GL_QUADS);
    glVertex2i(selectionRect.left(), selectionRect.top());
    glVertex2i(selectionRect.right(), selectionRect.top());
    glVertex2i(selectionRect.right(), selectionRect.bottom());
    glVertex2i(selectionRect.left(), selectionRect.bottom());
    glEnd();
    
    // Контур прямоугольника
    glColor4f(0.2f, 0.4f, 1.0f, 0.8f);  // Более яркий голубой
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(selectionRect.left(), selectionRect.top());
    glVertex2i(selectionRect.right(), selectionRect.top());
    glVertex2i(selectionRect.right(), selectionRect.bottom());
    glVertex2i(selectionRect.left(), selectionRect.bottom());
    glEnd();
    
    // Восстанавливаем настройки
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void MeshViewer::loadMesh(const QString &nodeFile, const QString &mshFile) {
    nodeCoords.clear();
    nodeCoords3D.clear();
    elements.clear();
    is3D = false;  // По умолчанию 2D
    
    // Read node.txt file
    QFile file(nodeFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open node file:" << nodeFile;
        return;
    }
    
    QTextStream in(&file);
    int numNodes = in.readLine().toInt();
    
    minX = 1e10;
    maxX = -1e10;
    minY = 1e10;
    maxY = -1e10;
    minZ = 1e10;
    maxZ = -1e10;
    
    for (int i = 1; i <= numNodes && !in.atEnd(); i++) {
        QString line = in.readLine();
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            double x = parts[0].toDouble();
            double y = parts[1].toDouble();
            double z = (parts.size() >= 3) ? parts[2].toDouble() : 0.0;
            
            // Сохраняем в 2D формате для совместимости
            nodeCoords[i] = QPair<double, double>(x, y);
            
            // Если есть Z-координата, сохраняем в 3D формате
            if (parts.size() >= 3 && fabs(z) > 1e-10) {
                nodeCoords3D[i] = QVector3D(x, y, z);
                is3D = true;
                minZ = qMin(minZ, z);
                maxZ = qMax(maxZ, z);
            } else {
                nodeCoords3D[i] = QVector3D(x, y, 0.0);
            }
            
            minX = qMin(minX, x);
            maxX = qMax(maxX, x);
            minY = qMin(minY, y);
            maxY = qMax(maxY, y);
        }
    }
    
    file.close();
    
    // Read nodes and elements from msh file if provided (для получения 3D координат)
    if (!mshFile.isEmpty()) {
        QFile mshFileHandle(mshFile);
        if (mshFileHandle.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream mshIn(&mshFileHandle);
            QString line;
            bool inNodesSection = false;
            bool inElementsSection = false;
            
            while (!mshIn.atEnd()) {
                line = mshIn.readLine().trimmed();
                
                // Читаем узлы из MSH файла для получения 3D координат
                if (line.contains("$Nodes")) {
                    inNodesSection = true;
                    line = mshIn.readLine().trimmed();
                    int numMshNodes = line.split(" ", Qt::SkipEmptyParts)[0].toInt();
                    qDebug() << "Reading" << numMshNodes << "nodes from MSH file for 3D coordinates";
                    
                    // Читаем узлы из MSH (формат: node_id x y z)
                    for (int i = 0; i < numMshNodes && !mshIn.atEnd(); i++) {
                        line = mshIn.readLine().trimmed();
                        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
                        if (parts.size() >= 4) {
                            int nodeId = parts[0].toInt();
                            double x = parts[1].toDouble();
                            double y = parts[2].toDouble();
                            double z = parts[3].toDouble();
                            
                            // Обновляем 3D координаты
                            nodeCoords3D[nodeId] = QVector3D(x, y, z);
                            
                            // Проверяем, является ли модель 3D
                            if (fabs(z) > 1e-10) {
                                is3D = true;
                                minZ = qMin(minZ, z);
                                maxZ = qMax(maxZ, z);
                            }
                        }
                    }
                    inNodesSection = false;
                    continue;
                }
                
                if (line.contains("$Elements")) {
                    inElementsSection = true;
                    // Следующая строка - количество элементов
                    line = mshIn.readLine().trimmed();
                    int numElements = line.toInt();
                    qDebug() << "Reading" << numElements << "elements from MSH file";
                    continue;
                }
                
                if (line.contains("$EndElements")) {
                    break;
                }
                
                if (inElementsSection && !line.isEmpty()) {
                    QStringList parts = line.split(" ", Qt::SkipEmptyParts);
                    if (parts.size() >= 4) {
                        // Format: element_id element_type num_tags node1 node2 node3 ...
                        bool ok;
                        int elemId = parts[0].toInt(&ok);
                        if (!ok) continue;
                        
                        int elemType = parts[1].toInt(&ok);
                        if (!ok) continue;
                        
                        // Типы элементов в Gmsh MSH 2.2:
                        // 1 = Line (2 nodes)
                        // 2 = Triangle (3 nodes)
                        // 3 = Quadrangle (4 nodes)
                        // 4 = Tetrahedron (4 nodes)
                        // 15 = Point (1 node)
                        
                        if (elemType == 2) {  // Triangle (2D) - 3 узла
                            int numTags = parts[2].toInt(&ok);
                            if (!ok) continue;
                            
                            // В MSH 2.2 формат: element_id element_type num_tags tag1 tag2 ... node1 node2 node3
                            int numNodes = 3;  // Для треугольника всегда 3 узла
                            int nodeStartIdx = 3 + numTags;  // После element_id, element_type, num_tags и тегов
                            
                            if (parts.size() >= nodeStartIdx + numNodes) {
                                QList<int> elemNodes;
                                for (int i = nodeStartIdx; i < nodeStartIdx + numNodes && i < parts.size(); i++) {
                                    int nodeId = parts[i].toInt(&ok);
                                    if (ok && nodeId > 0 && nodeCoords.contains(nodeId)) {
                                        elemNodes.append(nodeId);
                                    }
                                }
                                if (elemNodes.size() == 3) {
                                    elements.append(elemNodes);
                                }
                            }
                        } else if (elemType == 3) {  // Quadrangle (2D) - 4 узла, разбиваем на 2 треугольника
                            int numTags = parts[2].toInt(&ok);
                            if (!ok) continue;
                            
                            int numNodes = 4;  // Для четырехугольника 4 узла
                            int nodeStartIdx = 3 + numTags;
                            
                            if (parts.size() >= nodeStartIdx + numNodes) {
                                QList<int> quadNodes;
                                for (int i = nodeStartIdx; i < nodeStartIdx + numNodes && i < parts.size(); i++) {
                                    int nodeId = parts[i].toInt(&ok);
                                    if (ok && nodeId > 0 && nodeCoords.contains(nodeId)) {
                                        quadNodes.append(nodeId);
                                    }
                                }
                                if (quadNodes.size() == 4) {
                                    // Разбиваем четырехугольник на 2 треугольника
                                    // Треугольник 1: 0, 1, 2
                                    elements.append(QList<int>() << quadNodes[0] << quadNodes[1] << quadNodes[2]);
                                    // Треугольник 2: 0, 2, 3
                                    elements.append(QList<int>() << quadNodes[0] << quadNodes[2] << quadNodes[3]);
                                }
                            }
                        }
                    }
                }
            }
            
            mshFileHandle.close();
        }
    }
    
    // If no elements from msh, try to read from adjacency.txt
    if (elements.isEmpty()) {
        QString adjFile = nodeFile;
        adjFile.replace("node.txt", "adjacency.txt");
        QFile adjFileHandle(adjFile);
        if (adjFileHandle.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream adjIn(&adjFileHandle);
            QString line = adjIn.readLine();  // Skip header
            while (!adjIn.atEnd()) {
                line = adjIn.readLine();
                QStringList parts = line.split(" ", Qt::SkipEmptyParts);
                if (parts.size() >= 4) {
                    int nodeId = parts[0].toInt();
                    QList<int> adjNodes;
                    for (int i = 1; i < parts.size() && i <= 3; i++) {
                        int adjNode = parts[i].toInt();
                        if (adjNode > 0) {
                            adjNodes.append(adjNode);
                        }
                    }
                    // Create elements from adjacent nodes (simplified)
                    // This is a basic approach - in real CAD, elements are properly defined
                }
            }
            adjFileHandle.close();
        }
    }
    
    // Не добавляем padding, чтобы модель занимала все окно
    // minX, maxX, minY, maxY остаются без изменений
    
    // Если модель не загружена, устанавливаем значения по умолчанию
    if (minX >= maxX) {
        minX = -1.0;
        maxX = 1.0;
    }
    if (minY >= maxY) {
        minY = -1.0;
        maxY = 1.0;
    }
    
    // Автоматически подстраиваем масштаб для 3D визуализации
    // Вычисляем оптимальный zoom для отображения всей модели в перспективе
    double modelWidth = maxX - minX;
    double modelHeight = maxY - minY;
    
    if (modelWidth > 0 && modelHeight > 0) {
        // Используем текущий размер виджета или минимальный
        int widgetWidth = width() > 0 ? width() : 900;
        int widgetHeight = height() > 0 ? height() : 700;
        
        // Вычисляем aspect ratio
        double widgetAspect = (double)widgetWidth / (double)widgetHeight;
        double modelAspect = modelWidth / modelHeight;
        
        // Для 3D перспективы вычисляем zoom так, чтобы модель заполняла окно
        // Используем размер модели по большей стороне
        double modelSize = qMax(modelWidth, modelHeight);
        
        // Вычисляем zoom для перспективной проекции
        // Учитываем FOV и aspect ratio
        double fov = 45.0;
        double baseDistance = (modelSize / 2.0) / tan((fov * M_PI / 180.0) / 2.0);
        
        // Вычисляем требуемое расстояние для заполнения окна
        // Используем padding для заполнения всего окна и центрирования
        double padding = 0.1;  // 10% padding для вращения и центрирования
        
        // Учитываем aspect ratio для правильного масштабирования
        double effectiveModelSize = modelSize;
        if (widgetAspect > modelAspect) {
            // Виджет шире модели - используем высоту модели
            effectiveModelSize = modelHeight;
        } else {
            // Виджет выше модели - используем ширину модели
            effectiveModelSize = modelWidth;
        }
        
        double targetSize = effectiveModelSize * (1.0 + padding);
        double targetDistance = (targetSize / 2.0) / tan((fov * M_PI / 180.0) / 2.0);
        
        // Zoom - это отношение базового расстояния к целевому
        // Используем эффективный размер модели для правильного zoom
        double effectiveBaseDistance = (effectiveModelSize / 2.0) / tan((fov * M_PI / 180.0) / 2.0);
        zoom = effectiveBaseDistance / targetDistance;
        
        // Ограничиваем zoom разумными значениями для предотвращения "улетания" модели
        zoom = qBound(0.5, zoom, 15.0);  // Уменьшили диапазон для более контролируемого масштабирования
        
        qDebug() << "Auto-zoom (3D): widget=" << widgetWidth << "x" << widgetHeight 
                 << ", model=" << modelWidth << "x" << modelHeight 
                 << ", zoom=" << zoom;
    } else {
        zoom = 1.0;
    }
    
    // Сбрасываем все параметры вида при загрузке новой модели
    panX = 0.0;
    panY = 0.0;
    rotationX = 0.0;
    rotationY = 0.0;
    rotationZ = 0.0;
    
    // Zoom будет пересчитан автоматически в resizeGL или loadMesh
    // Принудительно обновляем viewport после загрузки для правильного центрирования
    QTimer::singleShot(100, this, [this]() {
        resizeGL(width(), height());
        update();
    });
    
    qDebug() << "Mesh loaded: " << nodeCoords.size() << " nodes, " << elements.size() << " elements";
    qDebug() << "Bounds: X=[" << minX << "," << maxX << "], Y=[" << minY << "," << maxY << "]";
    if (is3D) {
        qDebug() << "3D Model bounds: Z=[" << minZ << "," << maxZ << "]";
    }
    qDebug() << "Model size: " << (maxX - minX) << " x " << (maxY - minY);
    qDebug() << "Initial zoom: " << zoom;
    
    updateViewport();
    update();
}

void MeshViewer::setSelectedNodes(const QSet<int> &nodes) {
    selectedNodes = nodes;
    update();
}

void MeshViewer::setFixedNodesU(const QSet<int> &nodes) {
    fixedNodesU = nodes;
    update();
}

void MeshViewer::setFixedNodesV(const QSet<int> &nodes) {
    fixedNodesV = nodes;
    update();
}

void MeshViewer::setLoadNodes(const QMap<int, QPair<double, double>> &loads) {
    loadNodes = loads;
    update();
}

int MeshViewer::findNodeAtPosition(const QPointF &screenPos) {
    // Для 3D перспективы используем более точный метод выбора узлов
    // Проверяем узлы в экранных координатах (в пикселях) для более точного выбора
    
    // Вычисляем размер узла на экране в пикселях
    double nodeSizePixels = 20.0;  // Увеличенный размер узла для выбора (в пикселях)
    
    // Увеличиваем область выбора для более легкого клика
    double pickRadius = nodeSizePixels * 2.0;  // Радиус выбора в пикселях (увеличен в 2 раза)
    
    int nearestNode = -1;
    double minDistSq = pickRadius * pickRadius;
    
    // Используем сохраненные матрицы для правильного преобразования координат
    // Если матрицы не валидны, используем упрощенный метод
    if (!matricesValid) {
        // Если матрицы не сохранены, используем упрощенный метод для 2D
        for (auto it = nodeCoords.begin(); it != nodeCoords.end(); ++it) {
            int nodeId = it.key();
            QPointF nodeWorldPos(it.value().first, it.value().second);
            QPointF nodeScreenPos = worldToScreen(nodeWorldPos);
            double dx = nodeScreenPos.x() - screenPos.x();
            double dy = nodeScreenPos.y() - screenPos.y();
            double distSq = dx*dx + dy*dy;
            if (distSq < minDistSq) {
                minDistSq = distSq;
                nearestNode = nodeId;
            }
        }
        return nearestNode;
    }
    
    // Преобразуем все узлы в экранные координаты и находим ближайший
    if (is3D && !nodeCoords3D.isEmpty()) {
        // Для 3D моделей используем 3D координаты
        makeCurrent();  // Активируем OpenGL контекст для gluProject
        for (auto it = nodeCoords3D.begin(); it != nodeCoords3D.end(); ++it) {
            int nodeId = it.key();
            QVector3D nodeWorldPos = it.value();
            
            // Преобразуем 3D координаты в экранные с помощью gluProject
            GLdouble winX, winY, winZ;
            gluProject(nodeWorldPos.x(), nodeWorldPos.y(), nodeWorldPos.z(),
                       savedModelview, savedProjection, savedViewport,
                       &winX, &winY, &winZ);
            
            // winY в OpenGL отсчитывается снизу, а в Qt сверху, поэтому инвертируем
            QPointF nodeScreenPos(winX, height() - winY);
            
            // Вычисляем расстояние в экранных координатах (пикселях)
            double dx = nodeScreenPos.x() - screenPos.x();
            double dy = nodeScreenPos.y() - screenPos.y();
            double distSq = dx*dx + dy*dy;
            
            if (distSq < minDistSq) {
                minDistSq = distSq;
                nearestNode = nodeId;
            }
        }
        doneCurrent();  // Деактивируем контекст
    } else {
        // Для 2D моделей используем 2D координаты
        for (auto it = nodeCoords.begin(); it != nodeCoords.end(); ++it) {
            int nodeId = it.key();
            QPointF nodeWorldPos(it.value().first, it.value().second);
            
            // Преобразуем мировые координаты узла в экранные координаты
            QPointF nodeScreenPos = worldToScreen(nodeWorldPos);
            
            // Вычисляем расстояние в экранных координатах (пикселях)
            double dx = nodeScreenPos.x() - screenPos.x();
            double dy = nodeScreenPos.y() - screenPos.y();
            double distSq = dx*dx + dy*dy;
            
            if (distSq < minDistSq) {
                minDistSq = distSq;
                nearestNode = nodeId;
            }
        }
    }
    
    return nearestNode;
}

QPointF MeshViewer::screenToWorld(const QPointF &screenPos) {
    double aspect = (double)width() / (double)height();
    double viewWidth = (maxX - minX) * zoom;
    double viewHeight = (maxY - minY) * zoom;
    
    if (aspect > 1.0) {
        viewWidth *= aspect;
    } else {
        viewHeight /= aspect;
    }
    
    double centerX = (minX + maxX) / 2.0 + panX;
    double centerY = (minY + maxY) / 2.0 + panY;
    
    double x = centerX + (screenPos.x() / width() - 0.5) * viewWidth;
    double y = centerY - (screenPos.y() / height() - 0.5) * viewHeight;
    
    return QPointF(x, y);
}

QPointF MeshViewer::worldToScreen(const QPointF &worldPos) {
    // Преобразуем мировые координаты в экранные координаты для 3D перспективы
    // Используем OpenGL функции для правильного преобразования с учетом перспективной проекции
    
    if (width() <= 0 || height() <= 0) {
        return QPointF();
    }
    
    // Получаем текущие матрицы проекции и моделирования
    GLdouble modelview[16];
    GLdouble projection[16];
    GLint viewport[4];
    
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    // Используем Z-координату из модели (если 3D) или 0 для 2D
    double worldZ = 0.0;
    if (is3D && nodeCoords3D.size() > 0) {
        // Используем среднюю Z-координату для 3D моделей
        worldZ = (minZ + maxZ) / 2.0;
    }
    
    // Преобразуем мировые координаты в экранные с помощью gluProject
    GLdouble winX, winY, winZ;
    GLdouble objX = worldPos.x();
    GLdouble objY = worldPos.y();
    GLdouble objZ = worldZ;
    
    // Используем gluProject для правильного преобразования
    gluProject(objX, objY, objZ,
               modelview, projection, viewport,
               &winX, &winY, &winZ);
    
    // winY в OpenGL отсчитывается снизу, а в Qt сверху, поэтому инвертируем
    double screenX = winX;
    double screenY = height() - winY;
    
    return QPointF(screenX, screenY);
}

void MeshViewer::updateViewport() {
    // Viewport is updated in resizeGL and paintGL
}

void MeshViewer::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // ЛКМ - выбор узлов (только область выбора, одиночный клик не подсвечивает)
        int nodeId = findNodeAtPosition(event->localPos());
        if (nodeId > 0) {
            // Если кликнули на узел - не подсвечиваем его при одиночном клике
            // Подсветка будет только при двойном клике
            // Просто начинаем выбор области, чтобы можно было перетаскивать
            isSelecting = true;
            selectionStart = event->localPos().toPoint();
            selectionEnd = event->localPos().toPoint();
            setCursor(Qt::CrossCursor);  // Показываем курсор "крестик" для выбора
        } else {
            // Если не кликнули на узел - начинаем выбор области
            isSelecting = true;
            selectionStart = event->localPos().toPoint();
            selectionEnd = event->localPos().toPoint();
            setCursor(Qt::CrossCursor);  // Показываем курсор "крестик" для выбора
        }
    } else if (event->button() == Qt::RightButton) {
        // ПКМ - проверяем, нажат ли Shift
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift + ПКМ - перетаскивание объекта (panning)
            isPanning = true;
            lastMousePos = event->localPos();
            setCursor(Qt::ClosedHandCursor);  // Показываем курсор "рука"
        } else {
            // ПКМ без Shift - вращение модели
            isRotating = true;
            lastMousePos = event->localPos();
            setCursor(Qt::SizeAllCursor);  // Показываем курсор для вращения
        }
    }
}

void MeshViewer::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        int nodeId = findNodeAtPosition(event->localPos());
        if (nodeId > 0) {
            // Двойной клик - подсвечиваем узел и отправляем сигнал
            selectedNodes.insert(nodeId);
            update();  // Обновляем визуализацию
            
            if (is3D && nodeCoords3D.contains(nodeId)) {
                QVector3D coords = nodeCoords3D[nodeId];
                emit nodeDoubleClicked(nodeId, QPointF(coords.x(), coords.y()));
            } else if (nodeCoords.contains(nodeId)) {
                QPair<double, double> coords = nodeCoords[nodeId];
                emit nodeDoubleClicked(nodeId, QPointF(coords.first, coords.second));
            }
        }
    }
}

void MeshViewer::wheelEvent(QWheelEvent *event) {
    // Масштабирование на колесико мыши отключено
    // Используйте клавиши "+" и "-" для масштабирования
    event->ignore();  // Игнорируем событие, передаем дальше
}

void MeshViewer::mouseMoveEvent(QMouseEvent *event) {
    // Проверяем, находится ли курсор над узлом (для визуальной обратной связи)
    if (!isPanning && !isRotating && !isSelecting) {
        int hoveredNode = findNodeAtPosition(event->localPos());
        if (hoveredNode > 0) {
            setCursor(Qt::PointingHandCursor);  // Показываем курсор "рука с указателем"
        } else {
            setCursor(Qt::ArrowCursor);  // Обычный курсор
        }
    }
    
    if (isSelecting && (event->buttons() & Qt::LeftButton)) {
        // Обновляем конечную точку области выбора
        selectionEnd = event->localPos().toPoint();
        update();  // Перерисовываем для отображения области выбора
    } else if (isPanning && (event->buttons() & Qt::RightButton)) {
        // Улучшенное панорамирование - используем правильный расчет viewport
        QPointF delta = event->localPos() - lastMousePos;
        
        // Вычисляем размеры viewport с учетом текущего zoom и aspect ratio
        double modelWidth = maxX - minX;
        double modelHeight = maxY - minY;
        double widgetAspect = (double)width() / (double)height();
        double modelAspect = modelWidth / modelHeight;
        
        double viewWidth = modelWidth * zoom;
        double viewHeight = modelHeight * zoom;
        
        if (widgetAspect > modelAspect) {
            viewWidth = viewHeight * widgetAspect;
        } else {
            viewHeight = viewWidth / widgetAspect;
        }
        
        // Добавляем margin для расчета границ (соответствует margin в paintGL)
        double margin = 0.05;
        double totalViewWidth = viewWidth * (1.0 + 2.0 * margin);
        double totalViewHeight = viewHeight * (1.0 + 2.0 * margin);
        
        // Панорамирование в мировых координатах
        double newPanX = panX - delta.x() / width() * totalViewWidth;
        double newPanY = panY + delta.y() / height() * totalViewHeight;  // Y инвертирован
        
        // Ограничиваем панорамирование, чтобы модель оставалась в центре
        // Модель должна быть центрирована, поэтому ограничиваем смещение небольшими значениями
        // Максимальное смещение - 5% от размера модели (уменьшили для лучшего центрирования)
        double maxPanOffset = qMin(modelWidth, modelHeight) * zoom * 0.05;
        
        // Ограничиваем panX и panY, чтобы модель оставалась в центре
        panX = qBound(-maxPanOffset, newPanX, maxPanOffset);
        panY = qBound(-maxPanOffset, newPanY, maxPanOffset);
        
        lastMousePos = event->localPos();
        update();
    } else if (isRotating) {
        // 3D вращение модели как в CAD (простой trackball)
        // Движение мыши преобразуется в вращение вокруг осей
        
        QPointF delta = event->localPos() - lastMousePos;
        
        // Чувствительность вращения
        double sensitivity = 0.8;
        
        // Вычисляем размеры виджета для нормализации
        int widgetWidth = width();
        int widgetHeight = height();
        
        if (widgetWidth <= 0 || widgetHeight <= 0) {
            lastMousePos = event->localPos();
            return;
        }
        
        // Нормализуем движение мыши относительно размера виджета
        double normalizedDeltaX = delta.x() / widgetWidth;
        double normalizedDeltaY = delta.y() / widgetHeight;
        
        // Вращение вокруг оси Y (горизонтальное движение мыши)
        // Положительное движение вправо = положительное вращение вокруг Y
        rotationY += normalizedDeltaX * 360.0 * sensitivity;
        
        // Вращение вокруг оси X (вертикальное движение мыши)
        // Положительное движение вниз = положительное вращение вокруг X
        rotationX += normalizedDeltaY * 360.0 * sensitivity;
        
        // Нормализуем углы для непрерывного вращения
        rotationX = fmod(rotationX, 360.0);
        rotationY = fmod(rotationY, 360.0);
        rotationZ = fmod(rotationZ, 360.0);
        
        lastMousePos = event->localPos();
        update();
    }
}

void MeshViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // ЛКМ - завершаем выбор области
        if (isSelecting) {
            selectionEnd = event->pos();
            
            // Вычисляем прямоугольную область выбора
            QRect selectionRect = QRect(selectionStart, selectionEnd).normalized();
            
            // Находим все узлы в области выбора
            QSet<int> nodesInSelection;
            
            // Используем сохраненные матрицы для правильного преобразования координат
            if (matricesValid && is3D && !nodeCoords3D.isEmpty()) {
                // Для 3D моделей используем 3D координаты
                makeCurrent();  // Активируем OpenGL контекст для gluProject
                for (auto it = nodeCoords3D.begin(); it != nodeCoords3D.end(); ++it) {
                    int nodeId = it.key();
                    QVector3D nodeWorldPos = it.value();
                    
                    // Преобразуем 3D координаты в экранные с помощью gluProject
                    GLdouble winX, winY, winZ;
                    gluProject(nodeWorldPos.x(), nodeWorldPos.y(), nodeWorldPos.z(),
                               savedModelview, savedProjection, savedViewport,
                               &winX, &winY, &winZ);
                    
                    // winY в OpenGL отсчитывается снизу, а в Qt сверху, поэтому инвертируем
                    QPointF nodeScreenPos(winX, height() - winY);
                    
                    // Проверяем, находится ли узел в области выбора
                    if (selectionRect.contains(nodeScreenPos.toPoint())) {
                        nodesInSelection.insert(nodeId);
                    }
                }
                doneCurrent();  // Деактивируем контекст
            } else {
                // Для 2D моделей используем 2D координаты
                for (auto it = nodeCoords.begin(); it != nodeCoords.end(); ++it) {
                    int nodeId = it.key();
                    QPointF nodeWorldPos(it.value().first, it.value().second);
                    
                    // Преобразуем мировые координаты в экранные
                    QPointF nodeScreenPos = worldToScreen(nodeWorldPos);
                    
                    // Проверяем, находится ли узел в области выбора
                    if (selectionRect.contains(nodeScreenPos.toPoint())) {
                        nodesInSelection.insert(nodeId);
                    }
                }
            }
            
            // Если область выбора очень маленькая (меньше 5x5 пикселей), считаем это одиночным кликом
            if (selectionRect.width() < 5 && selectionRect.height() < 5) {
                // Одиночный клик - не подсвечиваем узел, только отправляем сигнал для обновления поля ввода
                int nodeId = findNodeAtPosition(event->localPos());
                if (nodeId > 0) {
                    // Не добавляем в selectedNodes при одиночном клике
                    // Отправляем сигнал только для обновления поля ввода (не подсвечиваем)
                    if (is3D && nodeCoords3D.contains(nodeId)) {
                        QVector3D coords = nodeCoords3D[nodeId];
                        emit nodeClicked(nodeId, QPointF(coords.x(), coords.y()));
                    } else if (nodeCoords.contains(nodeId)) {
                        QPair<double, double> coords = nodeCoords[nodeId];
                        emit nodeClicked(nodeId, QPointF(coords.first, coords.second));
                    }
                }
                // Не отправляем сигнал nodesSelected для одиночного клика
            } else {
                // Добавляем выбранные узлы к текущему набору
                selectedNodes.unite(nodesInSelection);
                
                // Отправляем сигнал о множественном выборе
                if (!nodesInSelection.isEmpty()) {
                    emit nodesSelected(nodesInSelection);
                }
            }
            
            // Обновляем визуализацию
            update();
            
            isSelecting = false;
            setCursor(Qt::ArrowCursor);
        }
    } else if (event->button() == Qt::RightButton) {
        // ПКМ - проверяем, был ли нажат Shift
        if (isPanning) {
            isPanning = false;
            setCursor(Qt::ArrowCursor);
        } else if (isRotating) {
            isRotating = false;
            setCursor(Qt::ArrowCursor);
        }
    }
}

void MeshViewer::keyPressEvent(QKeyEvent *event) {
    // Обработка клавиш "+" и "-" для масштабирования
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        // Увеличиваем масштаб
        double zoomFactor = 1.1;
        double oldZoom = zoom;
        zoom *= zoomFactor;
        zoom = qBound(0.5, zoom, 15.0);
        
        // Ограничиваем изменение
        double maxChange = 0.2;
        if (qAbs(zoom - oldZoom) / oldZoom > maxChange) {
            zoom = oldZoom * (1.0 + maxChange);
        }
        
        update();
        event->accept();
    } else if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore) {
        // Уменьшаем масштаб
        double zoomFactor = 1.0 / 1.1;
        double oldZoom = zoom;
        zoom *= zoomFactor;
        zoom = qBound(0.5, zoom, 15.0);
        
        // Ограничиваем изменение
        double maxChange = 0.2;
        if (qAbs(zoom - oldZoom) / oldZoom > maxChange) {
            zoom = oldZoom * (1.0 - maxChange);
        }
        
        update();
        event->accept();
    } else {
        // Передаем событие дальше, если это не наши клавиши
        QOpenGLWidget::keyPressEvent(event);
    }
}


