#ifndef MESHVIEWERWINDOW_H
#define MESHVIEWERWINDOW_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSet>
#include "meshviewer.h"

class MeshViewerWindow : public QDialog {
    Q_OBJECT

public:
    explicit MeshViewerWindow(const QString &nodeFile, const QString &mshFile = QString(), QWidget *parent = nullptr);
    ~MeshViewerWindow();

    MeshViewer* getMeshViewer() const { return meshViewer; }

signals:
    void nodeClicked(int nodeId, const QPointF &coords);
    void nodeDoubleClicked(int nodeId, const QPointF &coords);
    void nodesSelected(const QSet<int> &nodeIds);  // Сигнал для множественного выбора

private slots:
    void onNodeClicked(int nodeId, const QPointF &coords);
    void onNodeDoubleClicked(int nodeId, const QPointF &coords);
    void onNodesSelected(const QSet<int> &nodeIds);

private:
    void setupUI();
    
    QString nodeFile;
    QString mshFile;
    MeshViewer *meshViewer;
    QLabel *statusLabel;
};

#endif // MESHVIEWERWINDOW_H

