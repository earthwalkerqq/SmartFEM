#ifndef NODESELECTIONWINDOW_H
#define NODESELECTIONWINDOW_H

#include <QDialog>
#include <QPushButton>
#include <QTextEdit>
#include <QListWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QProcess>
#include <QMap>
#include <QSet>

// Forward declarations
class MeshViewerWindow;

class NodeSelectionWindow : public QDialog {
    Q_OBJECT

public:
    explicit NodeSelectionWindow(const QString &mshFile, const QString &nodeFile, QWidget *parent = nullptr);
    ~NodeSelectionWindow();

    // Получить выбранные узлы
    QStringList getFixedNodesU() const { return fixedNodesU; }
    QStringList getFixedNodesV() const { return fixedNodesV; }
    QStringList getLoadedNodes() const { return loadedNodes; }
    QMap<QString, QPair<double, double>> getNodeLoads() const { return nodeLoads; }

signals:
    void boundaryConditionsChanged();

private slots:
    void loadMesh();
    void selectNodesVisual();
    void addFixedNode();
    void addLoadNode();
    void removeFixedNode();
    void removeLoadNode();
    void saveAndClose();

private:
    void setupUI();
    void readNodeFile();
    bool saveBoundaryConditionsToFile();
    
    QString mshFile;
    QString nodeFile;
    
    // UI элементы
    QTextEdit *meshInfoText;
    QListWidget *nodeListWidget;
    QListWidget *fixedUListWidget;
    QListWidget *fixedVListWidget;
    QListWidget *loadedListWidget;
    QLineEdit *searchEdit;
    
    QLineEdit *nodeIdEdit;
    QComboBox *constraintTypeCombo;
    QDoubleSpinBox *loadFxEdit;
    QDoubleSpinBox *loadFyEdit;
    
    QPushButton *addFixedBtn;
    QPushButton *addLoadBtn;
    QPushButton *removeFixedBtn;
    QPushButton *removeLoadBtn;
    QPushButton *saveBtn;
    QPushButton *cancelBtn;
    
    // Отдельное окно для визуализации модели
    MeshViewerWindow *meshViewerWindow;
    
    // Данные
    QMap<int, QPair<double, double>> nodeCoords;  // node_id -> (x, y)
    QStringList fixedNodesU;
    QStringList fixedNodesV;
    QStringList loadedNodes;
    QMap<QString, QPair<double, double>> nodeLoads;  // node_id -> (Fx, Fy)
    QSet<int> selectedNodesInViewer;  // Nodes selected in the viewer
};

#endif // NODESELECTIONWINDOW_H

