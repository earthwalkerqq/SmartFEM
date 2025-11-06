#ifndef MESHVIEWER_H
#define MESHVIEWER_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtGui/QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMap>
#include <QPair>
#include <QPointF>
#include <QSet>

class MeshViewer : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit MeshViewer(QWidget *parent = nullptr);
    ~MeshViewer();

    void loadMesh(const QString &nodeFile, const QString &mshFile = QString());
    void setSelectedNodes(const QSet<int> &nodes);
    QSet<int> getSelectedNodes() const { return selectedNodes; }
    void setFixedNodesU(const QSet<int> &nodes);
    void setFixedNodesV(const QSet<int> &nodes);
    void setLoadNodes(const QMap<int, QPair<double, double>> &loads);  // nodeId -> (Fx, Fy)

signals:
    void nodeClicked(int nodeId, const QPointF &coords);
    void nodeDoubleClicked(int nodeId, const QPointF &coords);
    void nodesSelected(const QSet<int> &nodeIds);  // Сигнал для множественного выбора

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void drawMesh();
    void drawNodes();
    void drawElements();
    void drawSelectedNodes();
    void drawFixedNodes();  // Рисует закрепленные узлы
    void drawLoadArrows();  // Рисует стрелки сил
    void drawGrid();  // Сетка координат
    void drawSelectionRect();  // Рисует прямоугольную область выбора
    int findNodeAtPosition(const QPointF &screenPos);
    QPointF screenToWorld(const QPointF &screenPos);
    QPointF worldToScreen(const QPointF &worldPos);
    void updateViewport();

    // Mesh data
    QMap<int, QPair<double, double>> nodeCoords;  // node_id -> (x, y) - для 2D моделей
    QMap<int, QVector3D> nodeCoords3D;  // node_id -> (x, y, z) - для 3D моделей
    QList<QList<int>> elements;  // List of elements, each element is a list of node IDs
    QSet<int> selectedNodes;  // Selected node IDs
    QSet<int> fixedNodesU;  // Закрепленные узлы по U (x)
    QSet<int> fixedNodesV;  // Закрепленные узлы по V (y)
    QMap<int, QPair<double, double>> loadNodes;  // Узлы с силами: nodeId -> (Fx, Fy)
    bool is3D;  // Флаг для определения 3D модели
    
    // View parameters
    double zoom;
    double panX, panY;
    double rotationX;  // Угол вращения вокруг оси X (в градусах)
    double rotationY;  // Угол вращения вокруг оси Y (в градусах)
    double rotationZ;  // Угол вращения вокруг оси Z (в градусах)
    double minX, maxX, minY, maxY, minZ, maxZ;
    bool isPanning;
    bool isRotating;
    bool isSelecting;  // Флаг для выбора узлов по области
    QPointF lastMousePos;  // Для panning и rotation
    QPoint selectionStart;  // Начальная точка области выбора
    QPoint selectionEnd;    // Конечная точка области выбора
    
    // Сохраняем матрицы для правильного преобразования координат
    GLdouble savedModelview[16];
    GLdouble savedProjection[16];
    GLint savedViewport[4];
    bool matricesValid;  // Флаг валидности сохраненных матриц
    
    // Display options
    bool showNodes;
    bool showElements;
    bool showNodeLabels;
};

#endif // MESHVIEWER_H

