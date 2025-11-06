#ifndef MESHVIEWER_H
#define MESHVIEWER_H

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtGui/QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>
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

signals:
    void nodeClicked(int nodeId, const QPointF &coords);
    void nodeDoubleClicked(int nodeId, const QPointF &coords);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void drawMesh();
    void drawNodes();
    void drawElements();
    void drawSelectedNodes();
    void drawGrid();  // Сетка координат
    int findNodeAtPosition(const QPointF &screenPos);
    QPointF screenToWorld(const QPointF &screenPos);
    QPointF worldToScreen(const QPointF &worldPos);
    void updateViewport();

    // Mesh data
    QMap<int, QPair<double, double>> nodeCoords;  // node_id -> (x, y)
    QList<QList<int>> elements;  // List of elements, each element is a list of node IDs
    QSet<int> selectedNodes;  // Selected node IDs
    
    // View parameters
    double zoom;
    double panX, panY;
    double rotationAngle;  // Угол вращения в градусах
    double minX, maxX, minY, maxY;
    bool isPanning;
    bool isRotating;
    QPoint lastMousePos;  // Для panning и rotation
    
    // Display options
    bool showNodes;
    bool showElements;
    bool showNodeLabels;
};

#endif // MESHVIEWER_H

