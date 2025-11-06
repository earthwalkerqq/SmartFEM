#include "meshviewer.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTimer>
#include <QtGui/QOpenGLContext>
#include <cmath>
#include <QtMath>

MeshViewer::MeshViewer(QWidget *parent)
    : QOpenGLWidget(parent),
      zoom(1.0),
      panX(0.0),
      panY(0.0),
      rotationAngle(0.0),
      minX(0.0),
      maxX(1.0),
      minY(0.0),
      maxY(1.0),
      isPanning(false),
      isRotating(false),
      showNodes(true),
      showElements(true),
      showNodeLabels(false) {
    setMinimumSize(600, 600);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

MeshViewer::~MeshViewer() {
    makeCurrent();
    doneCurrent();
}

void MeshViewer::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);  // Темный фон для лучшего контраста
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
}

void MeshViewer::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    // Пересчитываем zoom при изменении размера, чтобы модель заполняла окно
    if (minX < maxX && minY < maxY && w > 0 && h > 0) {
        double modelWidth = maxX - minX;
        double modelHeight = maxY - minY;
        
        // Вычисляем zoom так, чтобы модель заполняла окно без padding
        double padding = 0.0;  // Убираем padding, чтобы модель занимала все окно
        double targetWidth = modelWidth * (1.0 + padding);
        double targetHeight = modelHeight * (1.0 + padding);
        
        // Масштабируем так, чтобы модель заполняла окно
        double scaleX = (double)w / targetWidth;
        double scaleY = (double)h / targetHeight;
        
        // Выбираем меньший масштаб, чтобы модель полностью поместилась
        zoom = qMin(scaleX, scaleY);
        zoom = qBound(0.01, zoom, 100.0);
    }
    updateViewport();
}

void MeshViewer::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Устанавливаем viewport
    glViewport(0, 0, width(), height());
    
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
    
    // Центр модели
    double centerX = (minX + maxX) / 2.0 + panX;
    double centerY = (minY + maxY) / 2.0 + panY;
    
    // Вычисляем размеры viewport так, чтобы модель заполняла все окно
    // Используем zoom, который уже вычислен для заполнения окна
    double viewWidth = modelWidth * zoom;
    double viewHeight = modelHeight * zoom;
    
    // Корректируем viewport с учетом aspect ratio виджета
    // Это гарантирует, что модель заполнит все окно
    if (widgetAspect > modelAspect) {
        // Виджет шире модели - расширяем viewport по ширине
        viewWidth = viewHeight * widgetAspect;
    } else {
        // Виджет выше модели - расширяем viewport по высоте
        viewHeight = viewWidth / widgetAspect;
    }
    
    glOrtho(centerX - viewWidth / 2.0,
            centerX + viewWidth / 2.0,
            centerY - viewHeight / 2.0,
            centerY + viewHeight / 2.0,
            -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Применяем вращение вокруг центра модели
    glTranslated(centerX, centerY, 0.0);
    glRotated(rotationAngle, 0.0, 0.0, 1.0);  // Вращение вокруг оси Z
    glTranslated(-centerX, -centerY, 0.0);
    
    // Рисуем сетку координат для лучшей ориентации
    drawGrid();
    
    // Рисуем элементы, узлы и выделенные узлы
    drawElements();
    drawNodes();
    drawSelectedNodes();
}

void MeshViewer::drawGrid() {
    // Рисуем сетку координат (опционально, для лучшей ориентации)
    if (minX >= maxX || minY >= maxY) return;
    
    glColor3f(0.15f, 0.15f, 0.15f);  // Темно-серая сетка
    glLineWidth(0.5f);
    glBegin(GL_LINES);
    
    // Вертикальные линии
    int numLines = 20;  // Больше линий для лучшей видимости
    for (int i = 0; i <= numLines; i++) {
        double x = minX + (maxX - minX) * i / numLines;
        glVertex2d(x, minY);
        glVertex2d(x, maxY);
    }
    
    // Горизонтальные линии
    for (int i = 0; i <= numLines; i++) {
        double y = minY + (maxY - minY) * i / numLines;
        glVertex2d(minX, y);
        glVertex2d(maxX, y);
    }
    
    glEnd();
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
                    glVertex2d(p1.first, p1.second);
                    glVertex2d(p2.first, p2.second);
                }
            }
        }
        
        glEnd();
        return;
    }
    
    // Рисуем элементы более ярким цветом и толще
    glColor3f(0.4f, 0.6f, 0.9f);  // Светло-синий цвет для элементов
    glLineWidth(2.5f);  // Увеличиваем толщину линий
    glBegin(GL_LINES);
    
    for (const QList<int> &elem : elements) {
        if (elem.size() >= 3) {
            // Draw triangle
            for (int i = 0; i < 3; i++) {
                int nodeId1 = elem[i];
                int nodeId2 = elem[(i + 1) % 3];
                
                if (nodeCoords.contains(nodeId1) && nodeCoords.contains(nodeId2)) {
                    QPair<double, double> p1 = nodeCoords[nodeId1];
                    QPair<double, double> p2 = nodeCoords[nodeId2];
                    glVertex2d(p1.first, p1.second);
                    glVertex2d(p2.first, p2.second);
                }
            }
        }
    }
    
    glEnd();
}

void MeshViewer::drawNodes() {
    if (!showNodes || nodeCoords.isEmpty()) return;
    
    // Рисуем узлы более ярким цветом и больше
    glColor3f(0.4f, 0.8f, 1.0f);  // Яркий голубой цвет для узлов
    glPointSize(8.0f);  // Увеличиваем размер узлов для лучшей видимости
    glBegin(GL_POINTS);
    
    for (auto it = nodeCoords.begin(); it != nodeCoords.end(); ++it) {
        if (!selectedNodes.contains(it.key())) {
            glVertex2d(it.value().first, it.value().second);
        }
    }
    
    glEnd();
}

void MeshViewer::drawSelectedNodes() {
    if (selectedNodes.isEmpty()) return;
    
    // Выделенные узлы - ярко-красные и крупнее
    glColor3f(1.0f, 0.0f, 0.0f);  // Яркий красный
    glPointSize(14.0f);  // Увеличиваем размер выделенных узлов
    glBegin(GL_POINTS);
    
    for (int nodeId : selectedNodes) {
        if (nodeCoords.contains(nodeId)) {
            QPair<double, double> coords = nodeCoords[nodeId];
            glVertex2d(coords.first, coords.second);
        }
    }
    
    glEnd();
}

void MeshViewer::loadMesh(const QString &nodeFile, const QString &mshFile) {
    nodeCoords.clear();
    elements.clear();
    
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
    
    for (int i = 1; i <= numNodes && !in.atEnd(); i++) {
        QString line = in.readLine();
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            double x = parts[0].toDouble();
            double y = parts[1].toDouble();
            nodeCoords[i] = QPair<double, double>(x, y);
            
            minX = qMin(minX, x);
            maxX = qMax(maxX, x);
            minY = qMin(minY, y);
            maxY = qMax(maxY, y);
        }
    }
    
    file.close();
    
    // Read elements from msh file if provided
    if (!mshFile.isEmpty()) {
        QFile mshFileHandle(mshFile);
        if (mshFileHandle.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream mshIn(&mshFileHandle);
            QString line;
            bool inElementsSection = false;
            
            while (!mshIn.atEnd()) {
                line = mshIn.readLine().trimmed();
                
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
    
    // Автоматически подстраиваем масштаб, чтобы модель занимала все окно
    // Вычисляем оптимальный zoom для отображения всей модели
    double modelWidth = maxX - minX;
    double modelHeight = maxY - minY;
    
    if (modelWidth > 0 && modelHeight > 0) {
        // Используем текущий размер виджета или минимальный
        int widgetWidth = width() > 0 ? width() : 900;
        int widgetHeight = height() > 0 ? height() : 700;
        
        // Вычисляем aspect ratio
        double widgetAspect = (double)widgetWidth / (double)widgetHeight;
        double modelAspect = modelWidth / modelHeight;
        
        // Вычисляем zoom так, чтобы модель заполняла окно без padding
        double padding = 0.0;  // Убираем padding, чтобы модель занимала все окно
        double targetWidth = modelWidth * (1.0 + padding);
        double targetHeight = modelHeight * (1.0 + padding);
        
        // Масштабируем так, чтобы модель заполняла окно
        double scaleX = (double)widgetWidth / targetWidth;
        double scaleY = (double)widgetHeight / targetHeight;
        
        // Выбираем меньший масштаб, чтобы модель полностью поместилась
        zoom = qMin(scaleX, scaleY);
        
        // Ограничиваем zoom разумными значениями
        zoom = qBound(0.01, zoom, 100.0);
        
        qDebug() << "Auto-zoom: widget=" << widgetWidth << "x" << widgetHeight 
                 << ", model=" << modelWidth << "x" << modelHeight 
                 << ", zoom=" << zoom;
    } else {
        zoom = 1.0;
    }
    
    panX = 0.0;
    panY = 0.0;
    rotationAngle = 0.0;  // Сбрасываем вращение при загрузке новой модели
    
    qDebug() << "Mesh loaded: " << nodeCoords.size() << " nodes, " << elements.size() << " elements";
    qDebug() << "Bounds: X=[" << minX << "," << maxX << "], Y=[" << minY << "," << maxY << "]";
    qDebug() << "Model size: " << (maxX - minX) << " x " << (maxY - minY);
    qDebug() << "Initial zoom: " << zoom;
    
    // Принудительно обновляем viewport после загрузки
    QTimer::singleShot(100, this, [this]() {
        resizeGL(width(), height());
        update();
    });
    
    updateViewport();
    update();
}

void MeshViewer::setSelectedNodes(const QSet<int> &nodes) {
    selectedNodes = nodes;
    update();
}

int MeshViewer::findNodeAtPosition(const QPointF &screenPos) {
    QPointF worldPos = screenToWorld(screenPos);
    
    // Find nearest node within tolerance
    double tolerance = 0.05 * qMin(maxX - minX, maxY - minY) / zoom;
    double minDist = 1e10;
    int nearestNode = -1;
    
    for (auto it = nodeCoords.begin(); it != nodeCoords.end(); ++it) {
        double dx = it.value().first - worldPos.x();
        double dy = it.value().second - worldPos.y();
        double dist = std::sqrt(dx*dx + dy*dy);
        
        if (dist < tolerance && dist < minDist) {
            minDist = dist;
            nearestNode = it.key();
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
    // Not used currently, but useful for future features
    return QPointF();
}

void MeshViewer::updateViewport() {
    // Viewport is updated in resizeGL and paintGL
}

void MeshViewer::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        int nodeId = findNodeAtPosition(event->localPos());
        if (nodeId > 0 && nodeCoords.contains(nodeId)) {
            QPair<double, double> coords = nodeCoords[nodeId];
            emit nodeClicked(nodeId, QPointF(coords.first, coords.second));
        } else {
            // Start panning
            isPanning = true;
            lastMousePos = event->pos();
        }
    } else if (event->button() == Qt::RightButton) {
        // Start rotating
        isRotating = true;
        lastMousePos = event->pos();
    }
}

void MeshViewer::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        int nodeId = findNodeAtPosition(event->localPos());
        if (nodeId > 0 && nodeCoords.contains(nodeId)) {
            QPair<double, double> coords = nodeCoords[nodeId];
            emit nodeDoubleClicked(nodeId, QPointF(coords.first, coords.second));
        }
    }
}

void MeshViewer::wheelEvent(QWheelEvent *event) {
    double zoomFactor = 1.1;
    if (event->angleDelta().y() < 0) {
        zoomFactor = 1.0 / zoomFactor;
    }
    
    zoom *= zoomFactor;
    zoom = qBound(0.1, zoom, 100.0);
    
    update();
}

void MeshViewer::mouseMoveEvent(QMouseEvent *event) {
    if (isPanning && event->buttons() & Qt::LeftButton) {
        QPointF delta = event->pos() - lastMousePos;
        double aspect = (double)width() / (double)height();
        double viewWidth = (maxX - minX) * zoom;
        double viewHeight = (maxY - minY) * zoom;
        
        if (aspect > 1.0) {
            viewWidth *= aspect;
        } else {
            viewHeight /= aspect;
        }
        
        panX -= delta.x() / width() * viewWidth;
        panY += delta.y() / height() * viewHeight;
        
        lastMousePos = event->pos();
        update();
    } else if (isRotating && event->buttons() & Qt::RightButton) {
        // Вычисляем угол вращения на основе движения мыши
        // Используем центр модели как точку вращения
        double centerX = (minX + maxX) / 2.0;
        double centerY = (minY + maxY) / 2.0;
        
        // Преобразуем экранные координаты в мировые
        QPointF lastWorldPos = screenToWorld(lastMousePos);
        QPointF currentWorldPos = screenToWorld(event->pos());
        
        // Векторы от центра модели к позициям мыши
        QPointF v1 = QPointF(lastWorldPos.x() - centerX, lastWorldPos.y() - centerY);
        QPointF v2 = QPointF(currentWorldPos.x() - centerX, currentWorldPos.y() - centerY);
        
        // Вычисляем угол между векторами
        double angle1 = atan2(v1.y(), v1.x()) * 180.0 / M_PI;
        double angle2 = atan2(v2.y(), v2.x()) * 180.0 / M_PI;
        double deltaAngle = angle2 - angle1;
        
        // Нормализуем угол
        if (deltaAngle > 180.0) deltaAngle -= 360.0;
        if (deltaAngle < -180.0) deltaAngle += 360.0;
        
        rotationAngle += deltaAngle;
        
        // Ограничиваем угол в разумных пределах
        while (rotationAngle >= 360.0) rotationAngle -= 360.0;
        while (rotationAngle < 0.0) rotationAngle += 360.0;
        
        lastMousePos = event->pos();
        update();
    }
}

void MeshViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isPanning = false;
    } else if (event->button() == Qt::RightButton) {
        isRotating = false;
    }
}

