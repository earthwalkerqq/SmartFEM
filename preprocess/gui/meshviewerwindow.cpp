#include "meshviewerwindow.h"
#include <QMessageBox>
#include <QFileInfo>

MeshViewerWindow::MeshViewerWindow(const QString &nodeFile, const QString &mshFile, QWidget *parent)
    : QDialog(parent), nodeFile(nodeFile), mshFile(mshFile) {
    setWindowTitle("Визуализация модели (кликните на узел для выбора)");
    setMinimumSize(800, 600);  // Уменьшили минимальный размер
    resize(1000, 750);  // Уменьшили начальный размер окна
    
    setupUI();
    
    // Загружаем модель в визуализатор
    if (QFileInfo::exists(mshFile)) {
        meshViewer->loadMesh(nodeFile, mshFile);
    } else {
        meshViewer->loadMesh(nodeFile);
    }
}

MeshViewerWindow::~MeshViewerWindow() {
}

void MeshViewerWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // Убираем все отступы
    mainLayout->setSpacing(0);                    // Убираем промежутки между элементами
    
    // Создаем визуализатор модели
    meshViewer = new MeshViewer(this);
    meshViewer->setMinimumSize(1, 1);
    meshViewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // Добавляем с растяжением (stretch factor = 1), чтобы занимал всё доступное пространство
    mainLayout->addWidget(meshViewer, 1);
    
    // Подключаем сигналы от визуализатора
    connect(meshViewer, &MeshViewer::nodeClicked, this, &MeshViewerWindow::onNodeClicked);
    connect(meshViewer, &MeshViewer::nodeDoubleClicked, this, &MeshViewerWindow::onNodeDoubleClicked);
    connect(meshViewer, &MeshViewer::nodesSelected, this, &MeshViewerWindow::onNodesSelected);
    
    // Статусная строка внизу (минимальная высота, не растягивается)
    statusLabel = new QLabel("Готово к выбору узлов", this);
    statusLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 5px; }");
    statusLabel->setMaximumHeight(30);  // Ограничиваем высоту статусной строки
    statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);  // Не растягивается по вертикали
    mainLayout->addWidget(statusLabel, 0);  // stretch factor = 0, не растягивается
}

void MeshViewerWindow::onNodeClicked(int nodeId, const QPointF &coords) {
    statusLabel->setText(QString("Выбран узел %1: (%.2f, %.2f)").arg(nodeId).arg(coords.x()).arg(coords.y()));
    emit nodeClicked(nodeId, coords);
}

void MeshViewerWindow::onNodeDoubleClicked(int nodeId, const QPointF &coords) {
    statusLabel->setText(QString("Двойной клик на узел %1: (%.2f, %.2f)").arg(nodeId).arg(coords.x()).arg(coords.y()));
    emit nodeDoubleClicked(nodeId, coords);
}

void MeshViewerWindow::onNodesSelected(const QSet<int> &nodeIds) {
    statusLabel->setText(QString("Выбрано узлов: %1").arg(nodeIds.size()));
    emit nodesSelected(nodeIds);
}

