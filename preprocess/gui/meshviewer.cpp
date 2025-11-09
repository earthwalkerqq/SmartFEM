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
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

MeshViewer::MeshViewer(QWidget *parent)
    : QOpenGLWidget(parent),
      is3D(false),
      zoom(1.0),
      panX(0.0),
      panY(0.0),
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
      showNodeLabels(false),
      panStartX(0.0),
      panStartY(0.0) {
    // Инициализируем кватернион как единичный (без вращения): {1, 0, 0, 0}
    rotationQuat[0] = 1.0f;  // w
    rotationQuat[1] = 0.0f;  // x
    rotationQuat[2] = 0.0f;  // y
    rotationQuat[3] = 0.0f;  // z
    
    // Размер устанавливается извне через setFixedSize, поэтому не устанавливаем минимальный размер
    // setMinimumSize(1, 1);  // Закомментировано, т.к. размер устанавливается из mainwindow.cpp
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);  // Фиксированный размер (не растягивается)
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
    // Включаем заливку граней для цветной визуализации
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);  // Темно-серый фон для темной темы
    glEnable(GL_DEPTH_TEST);  // Включаем тест глубины для 3D
    glDepthFunc(GL_LEQUAL);
    // НЕ включаем GL_CULL_FACE, чтобы видеть все грани с обеих сторон
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
    
    // При изменении размера виджета пересчитываем zoom, чтобы модель заполняла весь viewport
    if (minX < maxX && minY < maxY && w > 0 && h > 0) {
        double modelWidth = maxX - minX;
        double modelHeight = maxY - minY;
        if (modelWidth <= 0.0 || modelHeight <= 0.0) {
            return;
        }

        // Для 3D моделей zoom должен быть рассчитан так, чтобы модель заполняла viewport
        // Это достигается правильным расчетом distance в paintGL()
        // Значение 1.0 означает базовое масштабирование (модель заполняет viewport)
        if (!is3D) {
            // Для 2D моделей zoom рассчитывается автоматически через glOrtho
            zoom = 1.0;
        } else {
            // Для 3D моделей zoom также остается 1.0, distance рассчитывается в paintGL()
            // с учетом aspect ratio и размеров модели для заполнения всего viewport
            zoom = 1.0;
        }
        zoom = qBound(0.01, zoom, 5000.0);
    }
    updateViewport();
}

void MeshViewer::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Viewport для 3D модели - занимает весь размер виджета
    // QOpenGLWidget автоматически обрабатывает devicePixelRatio для рендеринга,
    // но мы должны использовать размеры в логических пикселях для viewport
    int widgetWidth = width();
    int widgetHeight = height();
    
    // ВАЖНО: Для QOpenGLWidget glViewport должен быть установлен в логических пикселях
    // QOpenGLWidget автоматически обрабатывает devicePixelRatio для рендеринга
    // Фактический framebuffer имеет размеры widgetWidth * devicePixelRatio() x widgetHeight * devicePixelRatio()
    // но glViewport должен быть установлен в логических пикселях (размеры виджета)
    // Устанавливаем viewport на весь размер виджета (0,0 - верхний левый угол в Qt координатах)
    glViewport(0, 0, widgetWidth * 2 , widgetHeight * 2);
    
    // Используем логические размеры для aspect ratio
    double widgetAspect = (double)widgetWidth / (double)widgetHeight;
    
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
    
    // widgetAspect уже рассчитан выше с учетом физических пикселей
    // double widgetAspect = (double)widgetWidth / (double)widgetHeight;  // Удалено, используется widgetAspect из выше
    double modelCenterX = (minX + maxX) / 2.0;
    double modelCenterY = (minY + maxY) / 2.0;
    double modelCenterZ = is3D ? (minZ + maxZ) / 2.0 : 0.0;
    
    if (!is3D) {
        // 2D режим: ортографическая проекция по фактическим границам модели
        // Учитываем aspect ratio виджета, чтобы модель заполняла весь viewport
        double padding = 0.05 * qMax(modelWidth, modelHeight);  // небольшой отступ (5%)
        double modelAspect = modelWidth / modelHeight;
        
        // Центр модели
        double centerX = modelCenterX + panX;
        double centerY = modelCenterY + panY;
        
        // Рассчитываем halfWidth и halfHeight так, чтобы модель заполняла весь viewport
        // Используем максимальный размер модели с padding для базового размера
        double modelSize = qMax(modelWidth, modelHeight);
        double baseHalfSize = (modelSize + 2.0 * padding) / 2.0;
        
        // Учитываем aspect ratio виджета и модели
        // Цель: модель должна заполнять весь viewport
        // Если виджет шире модели (widgetAspect > modelAspect), нужно увеличить halfWidth
        // Если виджет выше модели (widgetAspect < modelAspect), нужно увеличить halfHeight
        double halfWidth, halfHeight;
        if (widgetAspect > modelAspect) {
            // Виджет шире модели - модель будет заполнять по высоте, увеличиваем halfWidth
            // halfHeight = baseHalfSize (модель заполняет по высоте)
            // halfWidth должен быть таким, чтобы aspect ratio соответствовал виджету
            halfHeight = baseHalfSize;
            halfWidth = halfHeight * widgetAspect;
        } else {
            // Виджет выше модели - модель будет заполнять по ширине, увеличиваем halfHeight
            // halfWidth = baseHalfSize (модель заполняет по ширине)
            // halfHeight должен быть таким, чтобы aspect ratio соответствовал виджету
            halfWidth = baseHalfSize;
            halfHeight = halfWidth / widgetAspect;
        }
        
        // Применяем zoom
        halfWidth /= zoom;
        halfHeight /= zoom;
        
        // Убеждаемся, что значения положительные
        if (halfWidth <= 0.0) halfWidth = 0.5;
        if (halfHeight <= 0.0) halfHeight = 0.5;

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(centerX - halfWidth, centerX + halfWidth,
                centerY - halfHeight, centerY + halfHeight,
                -1.0, 1.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    } else {
        // 3D режим: перспективная проекция с правильным расчетом для заполнения всего viewport
        double modelDepth = maxZ - minZ;
        if (modelDepth < 0.0) modelDepth = 0.0;
        
        double fov = 45.0;  // Поле зрения в градусах
        double fovRad = fov * M_PI / 180.0;
        double tanHalfFovY = tan(fovRad / 2.0);
        double tanHalfFovX = tanHalfFovY * widgetAspect;
        
        // halfWidth и halfHeight - это половина ширины и высоты модели
        // Это нужно для правильного расчета расстояния камеры
        double halfWidth = modelWidth / 2.0;
        double halfHeight = modelHeight / 2.0;
        double halfDepth = modelDepth / 2.0;
        
        if (halfWidth <= 0.0) halfWidth = 0.5;
        if (halfHeight <= 0.0) halfHeight = 0.5;
        if (halfDepth < 0.0) halfDepth = 0.0;
        
        // Вычисляем расстояние камеры, необходимое для того, чтобы модель заполняла весь viewport
        // Используем bounding sphere для более точного расчета
        // Радиус сферы, описанной вокруг bounding box модели
        double sphereRadius = sqrt(halfWidth * halfWidth + halfHeight * halfHeight + halfDepth * halfDepth);
        if (sphereRadius <= 0.0) {
            sphereRadius = qMax(qMax(halfWidth, halfHeight), halfDepth);
            if (sphereRadius <= 0.0) {
                sphereRadius = 1.0;
            }
        }
        
        // Вычисляем расстояние, необходимое для того, чтобы модель заполняла viewport
        // Учитываем aspect ratio виджета: модель должна заполнять и по ширине, и по высоте
        double distanceForWidth = halfWidth / tanHalfFovX;
        double distanceForHeight = halfHeight / tanHalfFovY;
        
        // Используем МАКСИМАЛЬНОЕ из distanceForWidth и distanceForHeight
        // Это гарантирует, что модель точно поместится в viewport по обеим сторонам
        double baseDistance = qMax(distanceForWidth, distanceForHeight);
        
        // Применяем padding factor для небольшого отступа от краев
        // Используем padding factor для регулировки масштаба модели
        double paddingFactor = 4.0;  // НЕ ТРОГАТЬ - установлено пользователем
        baseDistance = baseDistance * paddingFactor;
        
        // Применяем zoom
        double distance = baseDistance / zoom;
        
        // Убеждаемся, что расстояние не слишком мало для видимости всей модели
        // Используем радиус сферы как минимальное расстояние, но с запасом для видимости
        // НЕ ограничиваем слишком сильно, чтобы модель могла заполнить viewport
        double minDistance = sphereRadius * 0.5;  // Минимальное расстояние для видимости модели (уменьшено для максимального масштаба)
        // Применяем ограничение только если baseDistance слишком маленький
        if (distance < minDistance) {
            distance = minDistance;
        }
        
        // Вычисляем сцену radius для near/far planes
        double sceneRadius = sphereRadius * 2.5;  // Увеличиваем для безопасности
        
        // Устанавливаем nearPlane и farPlane с большим запасом
        // nearPlane должен быть достаточно далеко, чтобы избежать проблем с отображением
        double nearPlane = qMax(0.1, distance - sceneRadius);
        double farPlane = distance + sceneRadius;
        
        // Убеждаемся, что farPlane достаточно далеко
        if (farPlane < nearPlane + 1.0) {
            farPlane = nearPlane + 100.0;
        }
        
        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov, widgetAspect, nearPlane, farPlane);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        double eyeX = panX;
        double eyeY = panY;
        double eyeZ = distance;
        double centerX = panX;
        double centerY = panY;
        double centerZ = 0.0;
        double upX = 0.0;
        double upY = 1.0;
        double upZ = 0.0;
        
        gluLookAt(eyeX, eyeY, eyeZ,
                  centerX, centerY, centerZ,
                  upX, upY, upZ);
        
        // Рисуем статическую сетку на заднем фоне ДО применения трансформаций модели
        // Сетка не будет вращаться вместе с моделью и будет находиться на заднем плане
        // Используем glPushMatrix/glPopMatrix чтобы изолировать сетку от трансформаций модели
        glPushMatrix();  // Сохраняем матрицу после gluLookAt
        drawStaticGrid(distance, nearPlane, farPlane);  // Рисуем статическую сетку на заднем плане
        glPopMatrix();  // Восстанавливаем матрицу
        
        glTranslatef(-modelCenterX, -modelCenterY, -modelCenterZ);
    }
    
    // Применяем вращение через кватернион
    // Создаем кватернион из массива [w, x, y, z]
    glm::quat quat(rotationQuat[0], rotationQuat[1], rotationQuat[2], rotationQuat[3]);
    
    // Нормализуем кватернион для избежания дрифта
    quat = glm::normalize(quat);
    
    // Преобразуем кватернион в матрицу вращения
    glm::mat4 rotationMatrix = glm::mat4_cast(quat);
    
    if (is3D) {
        // Применяем матрицу вращения через OpenGL только для 3D моделей
        GLfloat rotationGLMatrix[16];
        const float* source = glm::value_ptr(rotationMatrix);
        for (int i = 0; i < 16; i++) {
            rotationGLMatrix[i] = source[i];
        }
        glMultMatrixf(rotationGLMatrix);
    }
    
    // ВАЖНО: Сохраняем матрицы ПОСЛЕ всех трансформаций, но ДО рисования модели
    // Это нужно для правильного преобразования координат мыши в 3D пространство
    glGetDoublev(GL_MODELVIEW_MATRIX, savedModelview);
    glGetDoublev(GL_PROJECTION_MATRIX, savedProjection);
    
    // Сохраняем viewport - используем glGetIntegerv чтобы получить реальные координаты viewport
    // QOpenGLWidget автоматически масштабирует viewport для devicePixelRatio,
    // поэтому мы получаем физические пиксели, но для координат мыши используем логические
    GLint actualViewport[4];
    glGetIntegerv(GL_VIEWPORT, actualViewport);
    savedViewport[0] = actualViewport[0];
    savedViewport[1] = actualViewport[1];
    savedViewport[2] = actualViewport[2];
    savedViewport[3] = actualViewport[3];
    matricesValid = true;
    
    // ВАЖНО: Для правильного преобразования координат мыши нужно учитывать,
    // что gluProject возвращает координаты в физических пикселях viewport,
    // а event->localPos() возвращает координаты в логических пикселях виджета
    // QOpenGLWidget автоматически масштабирует, но мы должны использовать widgetWidth/Height
    // для преобразования координат мыши
    
    // Рисуем координатные оси СНАРУЖИ модели
    // Оси рисуются ПОСЛЕ перемещения модели в начало координат и после применения вращения
    // Это позволяет правильно позиционировать оси относительно модели
    // Оси будут вращаться вместе с моделью, но находиться снаружи bounding box
    if (is3D) {
        glPushMatrix();  // Сохраняем текущую матрицу (модель уже перемещена и повернута)
        drawCoordinateAxes();  // Рисуем оси в координатах относительно центра модели (0,0,0)
        glPopMatrix();  // Восстанавливаем матрицу
    }
    
    // Рисуем элементы, узлы и выделенные узлы
    drawElements();
    drawNodes();
    drawSelectedNodes();
    drawFixedNodes();  // Рисуем закрепленные узлы
    drawLoadArrows();  // Рисуем стрелки сил
    
    // Рисуем область выбора, если идет выбор узлов
    if (isSelecting) {
        drawSelectionRect();
    }
}

void MeshViewer::drawGrid() {
    if (is3D) {
        // Для 3D моделей рисуем сетку на плоскости XY (z=0)
        // Вычисляем размер сетки на основе размера модели и видимого объема
        double modelWidth = maxX - minX;
        double modelHeight = maxY - minY;
        
        if (modelWidth <= 0 || modelHeight <= 0) return;
        
        // Используем максимальный размер модели для определения размера сетки
        double modelSizeXY = qMax(modelWidth, modelHeight);
        
        // Сетка должна быть достаточно большой, чтобы заполнить весь видимый объем
        // Для перспективной проекции видимый объем зависит от расстояния камеры и FOV
        // Используем очень большой размер сетки (20x больше модели) чтобы гарантировать заполнение экрана
        // даже при поворотах и панорамировании
        double gridSize = modelSizeXY * 20.0;  // Очень большая сетка для заполнения всего экрана
        double gridStep = qMax(modelSizeXY * 0.5, 1.0);  // Шаг сетки - достаточно крупный для видимости
        
        // Настройки для рисования сетки - отключаем тест глубины для гарантированной видимости
        glDisable(GL_DEPTH_TEST);  // Отключаем тест глубины, чтобы сетка всегда была видна
        glColor3f(0.3f, 0.3f, 0.3f);  // Светло-серая сетка для темного фона (более заметная)
        glLineWidth(1.0f);  // Толщина линии для хорошей видимости
        glBegin(GL_LINES);
        
        // Рисуем сетку на плоскости XY (z = 0, центр по Z)
        // После трансформаций центр модели находится в (0,0,0)
        double gridZ = 0.0;  // Плоскость сетки
        
        // Вертикальные линии (параллельны Y)
        for (double x = -gridSize; x <= gridSize; x += gridStep) {
            glVertex3d(x, -gridSize, gridZ);
            glVertex3d(x, gridSize, gridZ);
        }
        
        // Горизонтальные линии (параллельны X)
        for (double y = -gridSize; y <= gridSize; y += gridStep) {
            glVertex3d(-gridSize, y, gridZ);
            glVertex3d(gridSize, y, gridZ);
        }
        
        glEnd();
        glEnable(GL_DEPTH_TEST);  // Включаем тест глубины обратно для элементов модели
    } else {
        // Для 2D моделей рисуем сетку вокруг центра (после трансформации центр в 0,0,0)
        double modelWidth = maxX - minX;
        double modelHeight = maxY - minY;
        if (modelWidth <= 0 || modelHeight <= 0) return;
        
        double modelSize = qMax(modelWidth, modelHeight);
        double gridSize = modelSize * 1.5;  // Сетка немного больше модели
        double gridStep = modelSize / 20.0;  // Шаг сетки
        
        glColor3f(0.4f, 0.4f, 0.4f);  // Серая сетка для темного фона
        glLineWidth(0.5f);
        glBegin(GL_LINES);
        
        // Вертикальные линии (относительно центра)
        for (double x = -gridSize; x <= gridSize; x += gridStep) {
            glVertex3d(x, -gridSize, 0.0);
            glVertex3d(x, gridSize, 0.0);
        }
        
        // Горизонтальные линии (относительно центра)
        for (double y = -gridSize; y <= gridSize; y += gridStep) {
            glVertex3d(-gridSize, y, 0.0);
            glVertex3d(gridSize, y, 0.0);
        }
        
        glEnd();
    }
}

void MeshViewer::drawStaticGrid(double cameraDistance, double nearPlane, double farPlane) {
    // Статическая сетка для 3D моделей - рисуется на заднем фоне и не вращается с моделью
    if (!is3D) return;
    
    // Вычисляем размер сетки на основе размера модели и видимого объема
    double modelWidth = maxX - minX;
    double modelHeight = maxY - minY;
    
    if (modelWidth <= 0 || modelHeight <= 0) return;
    
    // Используем максимальный размер модели для определения размера сетки
    double modelSizeXY = qMax(modelWidth, modelHeight);
    
    // Для статической сетки на заднем фоне используем очень большой размер
    // чтобы сетка всегда была видна, даже при поворотах камеры
    // Размер сетки должен покрывать видимый объем на основе расстояния камеры
    double visibleSize = cameraDistance * 2.5;  // Приблизительный видимый размер на расстоянии камеры (увеличен для покрытия)
    double gridSize = qMax(modelSizeXY * 40.0, visibleSize);  // Большая сетка для покрытия всего видимого объема
    double gridStep = qMax(modelSizeXY * 0.8, visibleSize * 0.15);  // Шаг сетки - адаптивный к видимому размеру
    
    // Настройки для рисования статической сетки на заднем фоне
    // Отключаем тест глубины, чтобы сетка всегда была видна на заднем плане
    // Это гарантирует, что сетка не будет перекрываться моделью
    glDisable(GL_DEPTH_TEST);  // Отключаем тест глубины для гарантированной видимости сетки на фоне
    glDepthMask(GL_FALSE);  // Отключаем запись в буфер глубины, чтобы сетка не влияла на глубину модели
    
    // Рисуем сетку на фиксированной плоскости XY в мировых координатах (z=0)
    // Сетка не будет трансформироваться (не будет вращаться с моделью)
    // Так как тест глубины отключен, сетка всегда будет видна на фоне
    double gridZ = 0.0;  // Плоскость сетки в мировых координатах
    
    glColor3f(0.25f, 0.25f, 0.25f);  // Темно-серая сетка для темного фона (заметная, но не слишком яркая)
    glLineWidth(1.0f);  // Толщина линии
    
    glBegin(GL_LINES);
    
    // Вертикальные линии (параллельны Y)
    for (double x = -gridSize; x <= gridSize; x += gridStep) {
        glVertex3d(x, -gridSize, gridZ);
        glVertex3d(x, gridSize, gridZ);
    }
    
    // Горизонтальные линии (параллельны X)
    for (double y = -gridSize; y <= gridSize; y += gridStep) {
        glVertex3d(-gridSize, y, gridZ);
        glVertex3d(gridSize, y, gridZ);
    }
    
    glEnd();
    
    // Восстанавливаем настройки глубины для правильного отображения модели
    glDepthMask(GL_TRUE);  // Включаем запись в буфер глубины обратно
    glEnable(GL_DEPTH_TEST);  // Включаем тест глубины обратно для элементов модели
}

void MeshViewer::drawElements() {
    if (!showElements) return;
    
    // Если элементов нет, рисуем линии между соседними узлами
    if (elements.isEmpty()) {
        // Рисуем простые линии между узлами (для отладки)
        glColor3f(0.5f, 0.7f, 0.9f);  // Светло-синий цвет для темного фона
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        
        // Рисуем линии между узлами, которые близко друг к другу
        QList<int> nodeIds = nodeCoords.keys();
        double maxDist = qMin(maxX - minX, maxY - minY) * 0.05;  // 5% от размера модели
        
        for (int i = 0; i < nodeIds.size(); i++) {
            for (int j = i + 1; j < nodeIds.size(); j++) {
                int nodeId1 = nodeIds[i];
                int nodeId2 = nodeIds[j];
                QPair<double, double> p1 = nodeCoords[nodeId1];
                QPair<double, double> p2 = nodeCoords[nodeId2];
                
                double dx = p2.first - p1.first;
                double dy = p2.second - p1.second;
                double dist = std::sqrt(dx*dx + dy*dy);
                
                if (dist < maxDist) {
                    glVertex3d(p1.first, p1.second, 0.0);
                    glVertex3d(p2.first, p2.second, 0.0);
                }
            }
        }
        
        glEnd();
        return;
    }
    
    // Для 3D моделей рисуем закрашенные грани с цветной раскраской
    if (is3D && !nodeCoords3D.isEmpty()) {
        // Включаем заливку и отключаем линии для закрашенных граней
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
        
        // Рисуем закрашенные треугольники с цветной раскраской по ориентации грани
        glBegin(GL_TRIANGLES);
        
        for (const QList<int> &elem : elements) {
            if (elem.size() >= 3) {
                // Получаем координаты трех вершин треугольника
                QVector3D v0, v1, v2;
                bool hasAllNodes = true;
                
                if (nodeCoords3D.contains(elem[0]) && 
                    nodeCoords3D.contains(elem[1]) && 
                    nodeCoords3D.contains(elem[2])) {
                    v0 = nodeCoords3D[elem[0]];
                    v1 = nodeCoords3D[elem[1]];
                    v2 = nodeCoords3D[elem[2]];
                } else {
                    hasAllNodes = false;
                }
                
                if (hasAllNodes) {
                    // Вычисляем нормаль треугольника
                    QVector3D edge1 = v1 - v0;
                    QVector3D edge2 = v2 - v0;
                    QVector3D normal = QVector3D::crossProduct(edge1, edge2);
                    double normalLength = normal.length();
                    if (normalLength > 1e-10) {
                        normal = normal / normalLength;
                    } else {
                        normal = QVector3D(0, 0, 1);  // Значение по умолчанию
                    }
                    
                    // Единый болотно-зеленый цвет для всех граней
                    glColor3f(0.4f, 0.5f, 0.3f);  // Болотно-зеленый
                    
                    // Рисуем треугольник
                    glVertex3d(v0.x(), v0.y(), v0.z());
                    glVertex3d(v1.x(), v1.y(), v1.z());
                    glVertex3d(v2.x(), v2.y(), v2.z());
                }
            }
        }
        
        glEnd();
        
        // Рисуем контуры треугольников для лучшей видимости структуры
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1.0f);
        glColor3f(0.2f, 0.2f, 0.3f);  // Темные линии для контуров
    glBegin(GL_LINES);
    
    for (const QList<int> &elem : elements) {
        if (elem.size() >= 3) {
            for (int i = 0; i < 3; i++) {
                int nodeId1 = elem[i];
                int nodeId2 = elem[(i + 1) % 3];
                
                    if (nodeCoords3D.contains(nodeId1) && nodeCoords3D.contains(nodeId2)) {
                    QVector3D p1 = nodeCoords3D[nodeId1];
                    QVector3D p2 = nodeCoords3D[nodeId2];
                    glVertex3d(p1.x(), p1.y(), p1.z());
                    glVertex3d(p2.x(), p2.y(), p2.z());
                    }
                }
            }
        }
        
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  // Возвращаем режим заливки
    } else {
        // Для 2D моделей рисуем только линии
        glColor3f(0.5f, 0.7f, 0.9f);  // Светло-синий цвет для элементов на темном фоне
        glLineWidth(3.0f);
        glBegin(GL_LINES);
        
        for (const QList<int> &elem : elements) {
            if (elem.size() >= 3) {
                for (int i = 0; i < 3; i++) {
                    int nodeId1 = elem[i];
                    int nodeId2 = elem[(i + 1) % 3];
                    
                    bool hasNode1_2D = nodeCoords.contains(nodeId1);
                    bool hasNode2_2D = nodeCoords.contains(nodeId2);
                    
                    if (hasNode1_2D && hasNode2_2D) {
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
}

void MeshViewer::drawNodes() {
    if (!showNodes || (nodeCoords.isEmpty() && nodeCoords3D.isEmpty())) return;
    
    // Рисуем узлы более ярким цветом и больше для лучшей видимости
    glColor3f(0.4f, 0.7f, 1.0f);  // Светло-синий цвет для узлов на темном фоне
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
        // Отладочный вывод для проверки координат узлов (только первые несколько раз)
        
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
    glColor3f(1.0f, 1.0f, 1.0f);  // Белая обводка для темного фона
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
    
    // Закрепленные узлы - ярко-зеленые для темного фона
    glColor3f(0.0f, 1.0f, 0.5f);  // Ярко-зеленый цвет
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

void MeshViewer::drawCoordinateAxes() {
    // Рисуем координатные оси СНАРУЖИ модели, в углу bounding box
    // ВАЖНО: Эта функция вызывается ПОСЛЕ того, как модель была перемещена в начало координат
    // и после применения вращения. Модель теперь находится в (0,0,0), а координаты minX/maxX и т.д.
    // являются мировыми координатами ДО перемещения. Нужно преобразовать их в координаты
    // относительно центра модели (который теперь в 0,0,0)
    
    // Вычисляем размеры модели для определения размера осей
    double modelWidth = maxX - minX;
    double modelHeight = maxY - minY;
    double modelDepth = maxZ - minZ;
    double modelSize = qMax(qMax(modelWidth, modelHeight), modelDepth);
    
    // Размер осей как процент от размера модели
    double axisLength = modelSize * 0.2;  // 20% от размера модели
    double axisThickness = modelSize * 0.015;  // 1.5% от размера модели для толщины
    
    // Вычисляем центр модели (он теперь в начале координат после glTranslatef)
    double modelCenterX = (minX + maxX) / 2.0;
    double modelCenterY = (minY + maxY) / 2.0;
    double modelCenterZ = (minZ + maxZ) / 2.0;
    
    // Правый нижний угол bounding box в координатах относительно центра модели
    // После glTranslatef(-modelCenterX, -modelCenterY, -modelCenterZ) центр модели в (0,0,0)
    // Поэтому правый нижний угол: (maxX - modelCenterX, minY - modelCenterY, modelCenterZ - modelCenterZ)
    double rightBottomX = maxX - modelCenterX;  // Правая граница относительно центра (положительное значение)
    double rightBottomY = minY - modelCenterY;  // Нижняя граница относительно центра (отрицательное значение)
    double rightBottomZ = 0.0;  // Средняя глубина (0 после перемещения)
    
    // Добавляем отступ, чтобы оси были СНАРУЖИ модели
    double offset = modelSize * 0.25;  // Отступ от границы модели (25% от размера)
    
    // Позиция начала осей: справа и внизу от правого нижнего угла модели
    // Это гарантирует, что оси будут снаружи bounding box модели
    double cornerX = rightBottomX + offset;  // Справа от правой границы модели
    double cornerY = rightBottomY - offset;  // Внизу от нижней границы модели (еще более отрицательное)
    double cornerZ = rightBottomZ;  // На средней глубине (0)
    
    glLineWidth(4.0f);
    glPointSize(10.0f);
    
    // Ось X - красная (горизонтально вправо)
    glColor3f(1.0f, 0.0f, 0.0f);  // Красный
    glBegin(GL_LINES);
    glVertex3d(cornerX, cornerY, cornerZ);
    glVertex3d(cornerX + axisLength, cornerY, cornerZ);
    glEnd();
    
    // Конус на конце оси X
    glBegin(GL_TRIANGLES);
    double tipX = cornerX + axisLength;
    glVertex3d(tipX, cornerY, cornerZ);
    glVertex3d(tipX - axisThickness, cornerY + axisThickness, cornerZ);
    glVertex3d(tipX - axisThickness, cornerY - axisThickness, cornerZ);
    glVertex3d(tipX, cornerY, cornerZ);
    glVertex3d(tipX - axisThickness, cornerY, cornerZ + axisThickness);
    glVertex3d(tipX - axisThickness, cornerY, cornerZ - axisThickness);
    glEnd();
    
    // Ось Y - зеленая (вертикально вверх)
    glColor3f(0.0f, 1.0f, 0.0f);  // Зеленый
    glBegin(GL_LINES);
    glVertex3d(cornerX, cornerY, cornerZ);
    glVertex3d(cornerX, cornerY + axisLength, cornerZ);
    glEnd();
    
    // Конус на конце оси Y
    glBegin(GL_TRIANGLES);
    double tipY = cornerY + axisLength;
    glVertex3d(cornerX, tipY, cornerZ);
    glVertex3d(cornerX + axisThickness, tipY - axisThickness, cornerZ);
    glVertex3d(cornerX - axisThickness, tipY - axisThickness, cornerZ);
    glVertex3d(cornerX, tipY, cornerZ);
    glVertex3d(cornerX, tipY - axisThickness, cornerZ + axisThickness);
    glVertex3d(cornerX, tipY - axisThickness, cornerZ - axisThickness);
    glEnd();
    
    // Ось Z - синяя (вглубь экрана)
    glColor3f(0.0f, 0.0f, 1.0f);  // Синий
    glBegin(GL_LINES);
    glVertex3d(cornerX, cornerY, cornerZ);
    glVertex3d(cornerX, cornerY, cornerZ - axisLength);
    glEnd();
    
    // Конус на конце оси Z
    glBegin(GL_TRIANGLES);
    double tipZ = cornerZ - axisLength;
    glVertex3d(cornerX, cornerY, tipZ);
    glVertex3d(cornerX + axisThickness, cornerY, tipZ + axisThickness);
    glVertex3d(cornerX - axisThickness, cornerY, tipZ + axisThickness);
    glVertex3d(cornerX, cornerY, tipZ);
    glVertex3d(cornerX, cornerY + axisThickness, tipZ + axisThickness);
    glVertex3d(cornerX, cornerY - axisThickness, tipZ + axisThickness);
    glEnd();
    
    // Точка в начале осей
    glColor3f(1.0f, 1.0f, 1.0f);  // Белый
    glBegin(GL_POINTS);
    glVertex3d(cornerX, cornerY, cornerZ);
    glEnd();
}

void MeshViewer::drawSelectionRect() {
    // Рисуем прямоугольную область выбора узлов
    if (!isSelecting) return;
    
    // Вычисляем прямоугольную область в координатах виджета
    QRect selectionRect = QRect(selectionStart, selectionEnd).normalized();
    
    // Получаем реальный размер виджета
    int widgetWidth = width();
    int widgetHeight = height();
    
    // Проверяем, что область выбора пересекается с виджетом
    QRect viewportRect(0, 0, widgetWidth, widgetHeight);
    if (!selectionRect.intersects(viewportRect)) {
        return;  // Область выбора не пересекается с виджетом
    }
    
    // Ограничиваем selectionRect границами виджета
    selectionRect = selectionRect.intersected(viewportRect);
    
    if (selectionRect.width() < 2 || selectionRect.height() < 2) {
        return;  // Слишком маленькая область
    }
    
    // Переключаемся в режим 2D для рисования прямоугольника
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    // OpenGL координаты: (0,0) внизу слева, (width, height) вверху справа
    // Qt координаты: (0,0) вверху слева, (width, height) внизу справа
    // Используем реальный размер виджета для правильных координат
    // Инвертируем Y для соответствия Qt координатам
    glOrtho(0, widgetWidth, widgetHeight, 0, -1, 1);  // Экранные координаты (Qt стиль: Y сверху вниз)
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Отключаем глубину для рисования поверх всего
    glDisable(GL_DEPTH_TEST);
    
    // Рисуем полупрозрачный прямоугольник
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // selectionRect уже в координатах виджета, поэтому используем его напрямую
    QRect viewportSelectionRect = selectionRect;
    
    // Проверяем, что прямоугольник находится внутри виджета
    if (viewportSelectionRect.left() < 0) viewportSelectionRect.setLeft(0);
    if (viewportSelectionRect.top() < 0) viewportSelectionRect.setTop(0);
    if (viewportSelectionRect.right() > widgetWidth) viewportSelectionRect.setRight(widgetWidth);
    if (viewportSelectionRect.bottom() > widgetHeight) viewportSelectionRect.setBottom(widgetHeight);
    
    // Заливка прямоугольника (полупрозрачная)
    glColor4f(0.3f, 0.5f, 1.0f, 0.2f);  // Голубой с прозрачностью
    glBegin(GL_QUADS);
    glVertex2i(viewportSelectionRect.left(), viewportSelectionRect.top());
    glVertex2i(viewportSelectionRect.right(), viewportSelectionRect.top());
    glVertex2i(viewportSelectionRect.right(), viewportSelectionRect.bottom());
    glVertex2i(viewportSelectionRect.left(), viewportSelectionRect.bottom());
    glEnd();
    
    // Контур прямоугольника
    glColor4f(0.2f, 0.4f, 1.0f, 0.8f);  // Более яркий голубой
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(viewportSelectionRect.left(), viewportSelectionRect.top());
    glVertex2i(viewportSelectionRect.right(), viewportSelectionRect.top());
    glVertex2i(viewportSelectionRect.right(), viewportSelectionRect.bottom());
    glVertex2i(viewportSelectionRect.left(), viewportSelectionRect.bottom());
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
    
    // Сбрасываем все параметры вида при загрузке новой модели
    panX = 0.0;
    panY = 0.0;
    // Сбрасываем кватернион в единичный (без вращения): {1, 0, 0, 0}
    rotationQuat[0] = 1.0f;  // w
    rotationQuat[1] = 0.0f;  // x
    rotationQuat[2] = 0.0f;  // y
    rotationQuat[3] = 0.0f;  // z
    zoom = 1.0;  // Сбрасываем zoom на начальное значение
    
    // Zoom будет пересчитан автоматически в resizeGL при первой отрисовке
    // Принудительно обновляем viewport после загрузки для правильного центрирования
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
    // Получаем реальный размер виджета
    int widgetWidth = width();
    int widgetHeight = height();
    
    // Проверяем, что точка находится внутри виджета
    if (screenPos.x() < 0 || screenPos.x() > widgetWidth || 
        screenPos.y() < 0 || screenPos.y() > widgetHeight) {
        return -1;
    }
    
    // Для 3D перспективы используем более точный метод выбора узлов
    // Проверяем узлы в экранных координатах (в пикселях) для более точного выбора
    
    // Вычисляем размер узла на экране в пикселях
    double nodeSizePixels = 20.0;  // Увеличенный размер узла для выбора (в пикселях)
    
    // Увеличиваем область выбора для более легкого клика
    // Учитываем zoom - при большом zoom out нужен больший радиус
    double zoomFactor = qMax(1.0, 1.0 / zoom);  // Больше zoom out -> больше zoomFactor
    double pickRadius = nodeSizePixels * 3.0 * zoomFactor;  // Радиус выбора в пикселях (увеличен в 3 раза с учетом zoom)
    
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
            
            // Получаем реальный размер виджета
            int widgetWidth = width();
            int widgetHeight = height();
            
            // Проверяем, что узел находится внутри виджета
            if (nodeScreenPos.x() < 0 || nodeScreenPos.x() > widgetWidth ||
                nodeScreenPos.y() < 0 || nodeScreenPos.y() > widgetHeight) {
                continue;
            }
            
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
    if (is3D && !nodeCoords3D.isEmpty() && matricesValid) {
        // Для 3D моделей используем 3D координаты
        makeCurrent();  // Активируем OpenGL контекст для gluProject
        
        // Вычисляем центр модели для проверки Z-координаты
        double modelCenterZ = (minZ + maxZ) / 2.0;
        
        // Логируем для отладки
        static int debugCounter = 0;
        bool shouldDebug = (++debugCounter % 10 == 0 || debugCounter <= 5);
        
        // ВАЖНО: gluProject возвращает winX и winY в координатах OpenGL окна
        // В OpenGL: origin (0,0) в нижнем левом углу окна, Y увеличивается вверх
        // В Qt: origin (0,0) в верхнем левом углу виджета, Y увеличивается вниз
        // 
        // Когда мы вызываем glViewport(X, Y, W, H), OpenGL устанавливает viewport внутри окна
        // Но winX и winY от gluProject все еще в координатах всего окна, а не только viewport
        //
        // Размер OpenGL окна равен размеру виджета Qt (это гарантируется Qt)
        int widgetWidth = width();
        int widgetHeight = height();
        
        // ВАЖНО: QOpenGLWidget использует FBO (framebuffer object) для рендеринга
        // Координаты от gluProject уже в координатах viewport (физические пиксели)
        // Но мы работаем с координатами виджета (логические пиксели)
        // Для правильного преобразования используем размер виджета в логических пикселях
        
        // winY отсчитывается от низа OpenGL viewport, поэтому:
        // Qt Y (от верха виджета) = widgetHeight - winY (если viewport начинается с 0,0)
        // Но нужно учесть, что savedViewport[1] может быть не 0 на некоторых системах
        double openglWindowHeight = savedViewport[3];  // Высота viewport в физических пикселях
        double widgetHeightLogical = widgetHeight;  // Высота виджета в логических пикселях
        
        
        int nodesChecked = 0;
        int nodesProjected = 0;
        int nodesInViewport = 0;
        int nodesInRange = 0;
        
        for (auto it = nodeCoords3D.begin(); it != nodeCoords3D.end(); ++it) {
            int nodeId = it.key();
            QVector3D nodeWorldPos = it.value();
            nodesChecked++;
            
            // Преобразуем 3D координаты в экранные с помощью gluProject
            // Важно: nodeWorldPos содержит исходные координаты узла (до трансформаций)
            // savedModelview уже содержит все трансформации (перемещение и вращение)
            GLdouble winX, winY, winZ;
            GLint result = gluProject(nodeWorldPos.x(), nodeWorldPos.y(), nodeWorldPos.z(),
                                     savedModelview, savedProjection, savedViewport,
                                     &winX, &winY, &winZ);
            
            // Проверяем, что преобразование прошло успешно
            if (result == GL_TRUE && winZ >= 0.0 && winZ <= 1.0) {
                nodesProjected++;
                
                // winX и winY возвращаются в координатах окна OpenGL (window coordinates)
                // В OpenGL window coordinates: origin (0,0) находится в нижнем левом углу окна
                // X увеличивается вправо, Y увеличивается вверх
                // В Qt: origin (0,0) находится в верхнем левом углу виджета
                // X увеличивается вправо, Y увеличивается вниз
                
                // winX и winY от gluProject в координатах OpenGL viewport (физические пиксели)
                // savedViewport содержит [x, y, width, height] viewport в физических пикселях
                // Координаты мыши приходят в логических пикселях виджета
                // 
                // Преобразуем из координат viewport (физические) в координаты виджета (логические):
                // 1. Вычитаем savedViewport[0] и savedViewport[1] (начало viewport)
                // 2. Масштабируем на devicePixelRatio для преобразования в логические пиксели
                // 3. Инвертируем Y (OpenGL: снизу вверх, Qt: сверху вниз)
                
                qreal dpr = devicePixelRatio();
                double nodeScreenX = (winX - savedViewport[0]) / dpr;
                double nodeScreenY = widgetHeightLogical - ((winY - savedViewport[1]) / dpr);
                
                
                // Получаем реальный размер виджета (уже получен выше)
                // Проверяем, что узел находится внутри виджета (в логических пикселях)
                if (nodeScreenX < -pickRadius || nodeScreenX > widgetWidth + pickRadius ||
                    nodeScreenY < -pickRadius || nodeScreenY > widgetHeight + pickRadius) {
                    continue;
                }
                
                nodesInViewport++;
                
                // Вычисляем расстояние в экранных координатах (пикселях)
                double dx = nodeScreenX - screenPos.x();
                double dy = nodeScreenY - screenPos.y();
                double distSq = dx*dx + dy*dy;
                double dist = sqrt(distSq);
                
                if (dist <= pickRadius) {
                    nodesInRange++;
                    
                    if (distSq < minDistSq) {
                        minDistSq = distSq;
                        nearestNode = nodeId;
                        
                    }
                }
            }
        }
        
        
        doneCurrent();  // Деактивируем контекст
    } else {
        // Для 2D моделей используем 2D координаты
        int widgetWidth = width();
        int widgetHeight = height();
        
        for (auto it = nodeCoords.begin(); it != nodeCoords.end(); ++it) {
            int nodeId = it.key();
            QPointF nodeWorldPos(it.value().first, it.value().second);
            
            // Преобразуем мировые координаты узла в экранные координаты
            QPointF nodeScreenPos = worldToScreen(nodeWorldPos);
            
            // Проверяем, что узел находится внутри виджета
            if (nodeScreenPos.x() < 0 || nodeScreenPos.x() > widgetWidth ||
                nodeScreenPos.y() < 0 || nodeScreenPos.y() > widgetHeight) {
                continue;
            }
            
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
    // Получаем реальный размер виджета
    int widgetWidth = width();
    int widgetHeight = height();
    
    
    // Проверяем, что координаты находятся внутри виджета
    if (screenPos.x() < 0 || screenPos.x() > widgetWidth || 
        screenPos.y() < 0 || screenPos.y() > widgetHeight) {
        return QPointF(0, 0);
    }
    
    // Для 3D моделей используем gluUnProject для правильного преобразования координат
    if (is3D && matricesValid) {
        makeCurrent();  // Активируем OpenGL контекст
        
        // ВАЖНО: screenPos приходит в логических пикселях виджета
        // savedViewport содержит физические пиксели (с учетом devicePixelRatio)
        // Преобразуем координаты мыши в физические пиксели для gluUnProject
        qreal dpr = devicePixelRatio();
        GLdouble winX = (screenPos.x() * dpr) + savedViewport[0];
        GLdouble winY = ((widgetHeight - screenPos.y()) * dpr) + savedViewport[1];
        // ВАЖНО: Для screenToWorld используем Z-координату центра модели (0) после glTranslatef
        // Это более надежный подход, так как центр модели всегда находится в (0,0,0) после трансформации
        // Используем Z = 0.5 (средняя глубина) для нахождения точки на плоскости модели
        GLdouble winZ = 0.5;  // Средняя глубина (центр модели после трансформации)
        
        // Преобразуем экранные координаты в мировые координаты
        GLdouble objX, objY, objZ;
        bool success = false;
        
        // Преобразуем экранные координаты в мировые координаты на плоскости модели
        if (gluUnProject(winX, winY, winZ,
                        savedModelview, savedProjection, savedViewport,
                        &objX, &objY, &objZ)) {
            // Проверяем, что точка находится в разумных пределах (не слишком далеко от модели)
            double modelSize = qMax(qMax(maxX - minX, maxY - minY), maxZ - minZ);
            double maxDistance = modelSize * 10.0;  // Очень большой margin для screenToWorld
            
            // Проверяем только расстояние от центра модели (0,0,0) после трансформации
            double distFromCenter = sqrt(objX*objX + objY*objY + objZ*objZ);
            if (distFromCenter <= maxDistance) {
                success = true;
            }
        }
        
        doneCurrent();  // Деактивируем контекст
        
        if (success) {
            return QPointF(objX, objY);
        }
    }
    
    // Для 2D моделей или если gluUnProject не сработал, используем упрощенный метод
    double aspect = (double)widgetWidth / (double)widgetHeight;
    double viewWidth = (maxX - minX) * zoom;
    double viewHeight = (maxY - minY) * zoom;
    
    if (aspect > 1.0) {
        viewWidth *= aspect;
    } else {
        viewHeight /= aspect;
    }
    
    double centerX = (minX + maxX) / 2.0 + panX;
    double centerY = (minY + maxY) / 2.0 + panY;
    
    // Преобразуем координаты относительно виджета
    double x = centerX + (screenPos.x() / widgetWidth - 0.5) * viewWidth;
    double y = centerY - (screenPos.y() / widgetHeight - 0.5) * viewHeight;
    
    return QPointF(x, y);
}

QPointF MeshViewer::worldToScreen(const QPointF &worldPos) {
    // Преобразуем мировые координаты в экранные координаты для 3D перспективы
    // Используем OpenGL функции для правильного преобразования с учетом перспективной проекции
    
    // Используем сохраненные матрицы и viewport из paintGL
    if (!matricesValid) {
        return QPointF();
    }
    
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
    
    // Используем сохраненные матрицы и viewport из paintGL
    GLint result = gluProject(objX, objY, objZ,
                              savedModelview, savedProjection, savedViewport,
                              &winX, &winY, &winZ);
    
    if (result == GL_TRUE) {
        // winX и winY от gluProject в физических пикселях viewport
        // Преобразуем в логические пиксели виджета
        qreal dpr = devicePixelRatio();
        int widgetWidth = width();
        int widgetHeight = height();
        
        
        // Преобразуем из физических пикселей viewport в логические пиксели виджета
        double screenX = (winX - savedViewport[0]) / dpr;
        // winY в OpenGL отсчитывается снизу, а в Qt сверху, поэтому инвертируем
        double screenY = widgetHeight - ((winY - savedViewport[1]) / dpr);
        
        // Логируем только периодически, чтобы не засорять вывод
        static int logCounter = 0;
        if (++logCounter % 100 == 0) {
        }
        
        return QPointF(screenX, screenY);
    } else {
        return QPointF();
    }
}

void MeshViewer::updateViewport() {
    // Viewport is updated in resizeGL and paintGL
}

void MeshViewer::mousePressEvent(QMouseEvent *event) {
    QPointF mousePos = event->localPos();
    
    // Получаем реальный размер виджета
    int widgetWidth = width();
    int widgetHeight = height();
    
    
    // Проверяем, что клик произошел внутри виджета
    bool insideViewport = (mousePos.x() >= 0 && mousePos.x() <= widgetWidth && 
                          mousePos.y() >= 0 && mousePos.y() <= widgetHeight);
    
    if (event->button() == Qt::LeftButton) {
        // ЛКМ - выбор узлов (только внутри виджета)
        if (insideViewport) {
            // Ограничиваем координаты границами виджета
            QPointF clampedPos(
                qBound(0.0, mousePos.x(), (double)widgetWidth),
                qBound(0.0, mousePos.y(), (double)widgetHeight)
            );
            
            
            
            int nodeId = findNodeAtPosition(clampedPos);
            
            // ОТЛАДКА: Выводим информацию о начале выделения
            qDebug() << "=== SELECTION START ===";
            qDebug() << "  Mouse position:" << mousePos.x() << mousePos.y();
            qDebug() << "  Clamped position:" << clampedPos.x() << clampedPos.y();
            qDebug() << "  widgetWidth:" << width() << "widgetHeight:" << height();
            qDebug() << "  Found node:" << nodeId;
            
            // Начинаем выбор области (выделение происходит при отпускании мыши)
            isSelecting = true;
            selectionStart = clampedPos.toPoint();
            selectionEnd = clampedPos.toPoint();
            setCursor(Qt::CrossCursor);  // Показываем курсор "крестик" для выбора
        }
    } else if (event->button() == Qt::RightButton) {
        // ПКМ - проверяем, нажат ли Shift
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift + ПКМ - перетаскивание объекта (panning) - только внутри viewport
            if (insideViewport) {
                isPanning = true;
                // Сохраняем начальную позицию мыши и текущие значения panX, panY
                panStartMousePos = mousePos;
                panStartX = panX;
                panStartY = panY;
                lastMousePos = mousePos;
                
                
                setCursor(Qt::ClosedHandCursor);  // Показываем курсор "рука"
            }
        } else {
            // ПКМ без Shift - вращение модели (только для 3D и только внутри viewport)
            if (is3D && insideViewport) {
            isRotating = true;
                lastMousePos = mousePos;
            setCursor(Qt::SizeAllCursor);  // Показываем курсор для вращения
            }
        }
    }
}

void MeshViewer::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPointF mousePos = event->localPos();
        
        // Проверяем, что двойной клик произошел внутри виджета
        int widgetWidth = width();
        int widgetHeight = height();
        
        
        bool insideViewport = (mousePos.x() >= 0 && mousePos.x() <= widgetWidth && 
                              mousePos.y() >= 0 && mousePos.y() <= widgetHeight);
        
        if (insideViewport) {
            // Ограничиваем координаты границами виджета
            QPointF clampedPos(
                qBound(0.0, mousePos.x(), (double)widgetWidth),
                qBound(0.0, mousePos.y(), (double)widgetHeight)
            );
            
            int nodeId = findNodeAtPosition(clampedPos);
        if (nodeId > 0) {
            // Двойной клик - переключаем выделение узла (выделяем/снимаем выделение)
            // Логика переключения будет обработана в MainWindow::onNodeDoubleClicked
            // Здесь просто отправляем сигнал, выделение обработается в обработчике
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
}

void MeshViewer::wheelEvent(QWheelEvent *event) {
    // Масштабирование на колесико мыши отключено
    // Используйте клавиши "+" и "-" для масштабирования
    event->ignore();  // Игнорируем событие, передаем дальше
}

void MeshViewer::mouseMoveEvent(QMouseEvent *event) {
    QPointF mousePos = event->localPos();
    
    // Проверяем, находится ли курсор над узлом (для визуальной обратной связи)
    if (!isPanning && !isRotating && !isSelecting) {
        // Проверяем, что курсор внутри виджета
        int widgetWidth = width();
        int widgetHeight = height();
        
        
        bool insideViewport = (mousePos.x() >= 0 && mousePos.x() <= widgetWidth && 
                              mousePos.y() >= 0 && mousePos.y() <= widgetHeight);
        
        if (insideViewport) {
            // Ограничиваем координаты границами виджета
            QPointF clampedPos(
                qBound(0.0, mousePos.x(), (double)widgetWidth),
                qBound(0.0, mousePos.y(), (double)widgetHeight)
            );
            
            int hoveredNode = findNodeAtPosition(clampedPos);
        if (hoveredNode > 0) {
            setCursor(Qt::PointingHandCursor);  // Показываем курсор "рука с указателем"
        } else {
            setCursor(Qt::ArrowCursor);  // Обычный курсор
            }
        } else {
            setCursor(Qt::ArrowCursor);  // Обычный курсор, если вне viewport
        }
    }
    
    if (isSelecting && (event->buttons() & Qt::LeftButton)) {
        // Обновляем конечную точку области выбора
        // Ограничиваем координаты границами виджета
        int widgetWidth = width();
        int widgetHeight = height();
        
        
        QPointF clampedPos(
            qBound(0.0, mousePos.x(), (double)widgetWidth),
            qBound(0.0, mousePos.y(), (double)widgetHeight)
        );
        
        selectionEnd = clampedPos.toPoint();
        update();  // Перерисовываем для отображения области выбора
    } else if (isPanning && (event->buttons() & Qt::RightButton)) {
        // Правильное панорамирование для 3D моделей с использованием gluUnProject
        // Для 3D моделей нужно учитывать Z-координату при преобразовании экранных координат в мировые
        
        if (!matricesValid) {
            // Если матрицы не валидны, ничего не делаем
            lastMousePos = mousePos;
            return;
        }
        
        // Вычисляем размеры модели
        double modelWidth = maxX - minX;
        double modelHeight = maxY - minY;
        
        if (modelWidth <= 0 || modelHeight <= 0) {
            // Если модель не загружена, ничего не делаем
            lastMousePos = mousePos;
            return;
        }
        
        if (is3D && matricesValid) {
            // Для 3D моделей используем более простой и надежный подход:
            // Преобразуем дельту движения мыши в экранных координатах в мировые координаты
            // используя параметры камеры (расстояние, FOV, размер viewport)
            
            // Вычисляем дельту движения мыши в экранных координатах
            double mouseDeltaX = mousePos.x() - panStartMousePos.x();
            double mouseDeltaY = mousePos.y() - panStartMousePos.y();
            
            // Параметры камеры (должны совпадать с paintGL)
            double fov = 45.0;  // Угол обзора в градусах
            int widgetWidth = width();
            int widgetHeight = height();
            double widgetAspect = (double)widgetWidth / (double)widgetHeight;
            
            // Вычисляем расстояние до модели (должно совпадать с paintGL)
            double modelDepth = maxZ - minZ;
            if (modelDepth < 0.0) modelDepth = 0.0;
            
            double halfWidth = modelWidth / 2.0;
            double halfHeight = modelHeight / 2.0;
            double halfDepth = modelDepth / 2.0;
            
            if (halfWidth <= 0.0) halfWidth = 0.5;
            if (halfHeight <= 0.0) halfHeight = 0.5;
            if (halfDepth < 0.0) halfDepth = 0.0;
            
            // Используем тот же расчет, что и в paintGL()
            double fovRad = fov * M_PI / 180.0;
            double tanHalfFovY = tan(fovRad / 2.0);
            double tanHalfFovX = tanHalfFovY * widgetAspect;
            
            double distanceForWidth = halfWidth / tanHalfFovX;
            double distanceForHeight = halfHeight / tanHalfFovY;
            
            double paddingFactor = 1.05;  // Должно совпадать с paintGL()
            double baseDistance = qMax(distanceForWidth, distanceForHeight) * paddingFactor;
            double distance = baseDistance / zoom;
            
            // Вычисляем размер видимой области в мировых координатах
            double visibleWorldHeight = 2.0 * distance * tan((fov * M_PI / 180.0) / 2.0);
            double visibleWorldWidth = visibleWorldHeight * widgetAspect;
            
            // Преобразуем дельту движения мыши в мировые координаты
            // X: прямое преобразование (1 пиксель = visibleWorldWidth / widgetWidth)
            // Y: инвертированное преобразование (Y в Qt сверху вниз, в OpenGL снизу вверх)
            double worldDeltaX = (mouseDeltaX / widgetWidth) * visibleWorldWidth;
            double worldDeltaY = -(mouseDeltaY / widgetHeight) * visibleWorldHeight;  // Y инвертирован
            
            // Вычисляем новые значения panX и panY от начальных значений
            double newPanX = panStartX + worldDeltaX;
            double newPanY = panStartY + worldDeltaY;
            
            // Ограничиваем панорамирование разумными пределами
            double sphereRadius = sqrt(halfWidth * halfWidth + halfHeight * halfHeight + halfDepth * halfDepth);
            if (sphereRadius <= 0.0) {
                sphereRadius = qMax(qMax(halfWidth, halfHeight), halfDepth);
                if (sphereRadius <= 0.0) {
                    sphereRadius = 1.0;
                }
            }
            double maxPanOffset = sphereRadius * 2.0;  // 200% от радиуса модели
            
            panX = qBound(-maxPanOffset, newPanX, maxPanOffset);
            panY = qBound(-maxPanOffset, newPanY, maxPanOffset);
            
            
            update();  // Обновляем отображение
        } else {
            // Для 2D моделей используем упрощенный расчет (как было раньше)
            QPointF delta = mousePos - panStartMousePos;
            
            // Вычисляем размеры видимой области с учетом текущего zoom и aspect ratio
            int widgetWidth = width();
            int widgetHeight = height();
            double widgetAspect = (double)widgetWidth / (double)widgetHeight;
            double modelAspect = modelWidth / modelHeight;
            
            double viewWorldWidth, viewWorldHeight;
            double modelSize = qMax(modelWidth, modelHeight);
            
            if (widgetAspect > modelAspect) {
                viewWorldHeight = modelHeight * zoom;
                viewWorldWidth = viewWorldHeight * widgetAspect;
            } else {
                viewWorldWidth = modelWidth * zoom;
                viewWorldHeight = viewWorldWidth / widgetAspect;
            }
            
            // Преобразуем движение мыши в пикселях в движение в мировых координатах
            double worldDeltaX = -(delta.x() / widgetWidth) * viewWorldWidth;
            double worldDeltaY = (delta.y() / widgetHeight) * viewWorldHeight;
            
            double newPanX = panStartX + worldDeltaX;
            double newPanY = panStartY + worldDeltaY;
            
            double maxPanOffset = modelSize * zoom * 1.0;
            
            panX = qBound(-maxPanOffset, newPanX, maxPanOffset);
            panY = qBound(-maxPanOffset, newPanY, maxPanOffset);
        }
        
        lastMousePos = mousePos;
        update();
    } else if (isRotating) {
        // 3D вращение модели с использованием кватернионов
        // Движение мыши преобразуется во вращение через кватернионы
        
        // Проверяем, что мышь все еще внутри виджета
        int widgetWidth = width();
        int widgetHeight = height();
        
        
        bool insideViewport = (mousePos.x() >= 0 && mousePos.x() <= widgetWidth && 
                              mousePos.y() >= 0 && mousePos.y() <= widgetHeight);
        
        if (insideViewport) {
            QPointF delta = mousePos - lastMousePos;
        
        // Чувствительность вращения
            double sensitivity = 2.0;  // Увеличена для более отзывчивого вращения
        
        if (widgetWidth <= 0 || widgetHeight <= 0) {
                lastMousePos = mousePos;
            return;
        }
        
            // Нормализуем движение мыши относительно размера viewport
        double normalizedDeltaX = delta.x() / widgetWidth;
        double normalizedDeltaY = delta.y() / widgetHeight;
        
            // Вычисляем углы вращения в радианах
            double angleX = normalizedDeltaY * sensitivity;
            double angleY = normalizedDeltaX * sensitivity;
            
            // Создаем кватернионы вращения вокруг локальных осей X и Y
        // Вращение вокруг оси X (вертикальное движение мыши)
            glm::quat rotX = glm::angleAxis((float)angleX, glm::vec3(1.0f, 0.0f, 0.0f));
            // Вращение вокруг оси Y (горизонтальное движение мыши)
            glm::quat rotY = glm::angleAxis((float)angleY, glm::vec3(0.0f, 1.0f, 0.0f));
            
            // Получаем текущий кватернион из массива
            glm::quat currentQuat(rotationQuat[0], rotationQuat[1], rotationQuat[2], rotationQuat[3]);
            
            // Умножаем кватернионы: сначала вращение вокруг Y, потом вокруг X
            // Порядок важен: rotY * rotX * currentQuat означает сначала применяем rotX, потом rotY
            currentQuat = rotY * rotX * currentQuat;
            
            // Нормализуем кватернион для избежания дрифта
            currentQuat = glm::normalize(currentQuat);
            
            // Сохраняем кватернион обратно в массив
            rotationQuat[0] = currentQuat.w;
            rotationQuat[1] = currentQuat.x;
            rotationQuat[2] = currentQuat.y;
            rotationQuat[3] = currentQuat.z;
            
            lastMousePos = mousePos;
        update();
        } else {
            // Если мышь вышла за пределы viewport, прекращаем вращение
            isRotating = false;
            setCursor(Qt::ArrowCursor);
        }
    }
}

void MeshViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // ЛКМ - завершаем выбор области
        if (isSelecting) {
            QPointF mousePos = event->localPos();
            
            // Получаем реальный размер виджета
            int widgetWidth = width();
            int widgetHeight = height();
            
            
            // Ограничиваем координаты границами виджета
            QPointF clampedEndPos(
                qBound(0.0, mousePos.x(), (double)widgetWidth),
                qBound(0.0, mousePos.y(), (double)widgetHeight)
            );
            
            selectionEnd = clampedEndPos.toPoint();
            
            // Вычисляем прямоугольную область выбора в координатах виджета
            QRect selectionRect = QRect(selectionStart, selectionEnd).normalized();
            
            // Проверяем, что область выбора пересекается с виджетом
            QRect viewportRect(0, 0, widgetWidth, widgetHeight);
            if (!selectionRect.intersects(viewportRect)) {
                // Область выбора не пересекается с виджетом - ничего не выбираем
                isSelecting = false;
                setCursor(Qt::ArrowCursor);
                update();
                return;
            }
            
            // Ограничиваем selectionRect границами виджета
            selectionRect = selectionRect.intersected(viewportRect);
            
            // ОТЛАДКА: Выводим информацию о выделении
            qDebug() << "=== SELECTION DEBUG ===";
            qDebug() << "  selectionStart:" << selectionStart;
            qDebug() << "  selectionEnd:" << selectionEnd;
            qDebug() << "  selectionRect:" << selectionRect;
            qDebug() << "  widgetWidth:" << widgetWidth << "widgetHeight:" << widgetHeight;
            qDebug() << "  viewportRect:" << viewportRect;
            
            // Находим все узлы в области выбора
            QSet<int> nodesInSelection;
            
            // Используем сохраненные матрицы для правильного преобразования координат
            if (matricesValid && is3D && !nodeCoords3D.isEmpty()) {
                // Для 3D моделей используем 3D координаты
                makeCurrent();  // Активируем OpenGL контекст для gluProject
                
                // Размер виджета для преобразования координат
                int widgetWidth = width();
                int widgetHeight = height();
                
                qDebug() << "  savedViewport [x,y,w,h]:" << savedViewport[0] << savedViewport[1] << savedViewport[2] << savedViewport[3];
                
                int nodesChecked = 0;
                int nodesInRect = 0;
                int nodesProjected = 0;
                
                for (auto it = nodeCoords3D.begin(); it != nodeCoords3D.end(); ++it) {
                    int nodeId = it.key();
                    QVector3D nodeWorldPos = it.value();
                    nodesChecked++;
                    
                    // Преобразуем 3D координаты в экранные с помощью gluProject
                    GLdouble winX, winY, winZ;
                    GLint result = gluProject(nodeWorldPos.x(), nodeWorldPos.y(), nodeWorldPos.z(),
                               savedModelview, savedProjection, savedViewport,
                               &winX, &winY, &winZ);
                    
                    if (result == GL_TRUE && winZ >= 0.0 && winZ <= 1.0) {
                        nodesProjected++;
                        // winX и winY от gluProject в координатах viewport (физические пиксели)
                        // savedViewport содержит [x, y, width, height] viewport в физических пикселях
                        // Нужно преобразовать в логические пиксели виджета
                        qreal dpr = devicePixelRatio();
                        
                        // Преобразуем из физических пикселей viewport в логические пиксели виджета
                        // 1. Вычитаем savedViewport[0] и savedViewport[1] (начало viewport)
                        // 2. Делим на devicePixelRatio для преобразования в логические пиксели
                        // 3. Инвертируем Y (OpenGL: снизу вверх, Qt: сверху вниз)
                        double nodeScreenX = (winX - savedViewport[0]) / dpr;
                        double nodeScreenY = widgetHeight - ((winY - savedViewport[1]) / dpr);
                        QPointF nodeScreenPos(nodeScreenX, nodeScreenY);
                        
                        // ОТЛАДКА: Выводим информацию о первых нескольких узлах
                        if (nodesChecked <= 10 || nodesInRect < 5) {
                            qDebug() << "  Node" << nodeId << ": world(" << nodeWorldPos.x() << "," << nodeWorldPos.y() << "," << nodeWorldPos.z() 
                                     << ") -> win(" << winX << "," << winY << ") -> screen(" << nodeScreenX << "," << nodeScreenY << ")";
                            qDebug() << "    dpr:" << dpr << "savedViewport[0]:" << savedViewport[0] << "savedViewport[1]:" << savedViewport[1];
                            qDebug() << "    selectionRect.contains(" << nodeScreenPos.toPoint() << "):" << selectionRect.contains(nodeScreenPos.toPoint());
                        }
                        
                        // Проверяем, находится ли узел в области выбора (используем координаты виджета)
                        // selectionRect уже в координатах виджета, поэтому сравниваем напрямую
                        if (selectionRect.contains(nodeScreenPos.toPoint())) {
                            nodesInSelection.insert(nodeId);
                            nodesInRect++;
                            if (nodesInRect <= 5) {
                                qDebug() << "    -> SELECTED! Node" << nodeId;
                            }
                        }
                    }
                }
                
                qDebug() << "  Summary: checked=" << nodesChecked << "projected=" << nodesProjected << "inRect=" << nodesInRect;
                doneCurrent();  // Деактивируем контекст
            } else {
                // Для 2D моделей используем 2D координаты
                for (auto it = nodeCoords.begin(); it != nodeCoords.end(); ++it) {
                    int nodeId = it.key();
                    QPointF nodeWorldPos(it.value().first, it.value().second);
                    
                    // Преобразуем мировые координаты в экранные
                    QPointF nodeScreenPos = worldToScreen(nodeWorldPos);
                    
                    // Проверяем, находится ли узел в области выбора (selectionRect уже в координатах виджета)
                    if (selectionRect.contains(nodeScreenPos.toPoint())) {
                        nodesInSelection.insert(nodeId);
                    }
                }
            }
            
            // Если область выбора очень маленькая (меньше 5x5 пикселей), считаем это одиночным кликом
            if (selectionRect.width() < 5 && selectionRect.height() < 5) {
                // Одиночный клик - не подсвечиваем узел, только отправляем сигнал для обновления поля ввода
                QPointF clampedPos(
                    qBound(0.0, mousePos.x(), (double)widgetWidth),
                    qBound(0.0, mousePos.y(), (double)widgetHeight)
                );
                int nodeId = findNodeAtPosition(clampedPos);
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
    // Формула: distance = effectiveDistance / zoom
    // Больший zoom -> меньше distance -> модель БЛИЖЕ к камере -> модель БОЛЬШЕ на экране
    // Меньший zoom -> больше distance -> модель ДАЛЬШЕ от камеры -> модель МЕНЬШЕ на экране
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        // Увеличиваем масштаб (УВЕЛИЧИВАЕМ zoom для увеличения модели)
        double oldZoom = zoom;
        zoom *= 1.2;  // Умножаем на 1.2, чтобы увеличить zoom и увеличить модель
        zoom = qBound(0.01, zoom, 5000.0);
        
        
        update();
        event->accept();
    } else if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore) {
        // Уменьшаем масштаб модели (УМЕНЬШАЕМ zoom для уменьшения модели)
        // Формула: distance = effectiveDistance / zoom
        // Меньший zoom -> больше distance -> модель ДАЛЬШЕ от камеры -> модель МЕНЬШЕ на экране
        double oldZoom = zoom;
        
        // Агрессивное уменьшение zoom для сильного уменьшения модели
        // ДЕЛИМ zoom на большое число для очень заметного уменьшения
        zoom /= 2.0;  // Делим zoom на 4.0 за каждое нажатие для очень сильного уменьшения модели
        
        // Уменьшен минимальный zoom до 0.01 для возможности экстремального уменьшения модели
        // При zoom = 0.01 модель будет в 50 раз дальше, чем при zoom = 0.5
        zoom = qBound(0.01, zoom, 5000.0);
        
        // Вычисляем effectiveDistance для отладки
        double modelWidth = maxX - minX;
        double modelHeight = maxY - minY;
        double modelSize = qMax(modelWidth, modelHeight);
        double effectiveDistance = (modelSize / 2.0) / tan((45.0 * M_PI / 180.0) / 2.0);
        
        
        update();
        event->accept();
    } else if (event->key() == Qt::Key_Q) {
        // Удаляем из выбранных узлов те, к которым еще не были применены граничные условия
        // Граничные условия - это узлы в fixedNodesU или fixedNodesV
        QSet<int> nodesToRemove;
        
        // Находим все выбранные узлы, которые НЕ имеют граничных условий
        for (int nodeId : selectedNodes) {
            // Если узел не в fixedNodesU и не в fixedNodesV, то у него нет граничных условий
            if (!fixedNodesU.contains(nodeId) && !fixedNodesV.contains(nodeId)) {
                nodesToRemove.insert(nodeId);
            }
        }
        
        // Удаляем узлы без граничных условий из выбранных
        for (int nodeId : nodesToRemove) {
            selectedNodes.remove(nodeId);
        }
        
        // Обновляем визуализацию
        update();
        
        // Отправляем сигнал об изменении выбранных узлов
        emit nodesSelected(selectedNodes);
        
        event->accept();
    } else {
        // Передаем событие дальше, если это не наши клавиши
        QOpenGLWidget::keyPressEvent(event);
    }
}


