#include "nodeselectionwindow.h"
#include "meshviewer.h"
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFileDialog>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QInputDialog>
#include <cmath>
#include <QSplitter>

NodeSelectionWindow::NodeSelectionWindow(const QString &mshFile, const QString &nodeFile, QWidget *parent)
    : QDialog(parent), mshFile(mshFile), nodeFile(nodeFile) {
    setWindowTitle("Выбор узлов для граничных условий");
    setMinimumSize(1400, 900);
    resize(1600, 1000);
    
    setupUI();
    readNodeFile();
    
    // Загружаем модель в визуализатор
    if (QFileInfo::exists(mshFile)) {
        meshViewer->loadMesh(nodeFile, mshFile);
    } else {
        meshViewer->loadMesh(nodeFile);
    }
}

NodeSelectionWindow::~NodeSelectionWindow() {
}

void NodeSelectionWindow::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Информация о сетке
    QGroupBox *infoGroup = new QGroupBox("Информация о сетке", this);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);
    meshInfoText = new QTextEdit(this);
    meshInfoText->setReadOnly(true);
    meshInfoText->setMaximumHeight(100);
    infoLayout->addWidget(meshInfoText);
    mainLayout->addWidget(infoGroup);
    
    // Основной layout с разделителем
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Левая часть - список узлов
    QWidget *leftWidget = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    
    // Список узлов
    QGroupBox *nodeGroup = new QGroupBox("Список узлов", this);
    QHBoxLayout *nodeLayout = new QHBoxLayout(nodeGroup);
    
    // Список узлов с поиском
    QGroupBox *nodeListGroup = new QGroupBox("Список узлов (кликните для выбора)", this);
    QVBoxLayout *nodeListLayout = new QVBoxLayout(nodeListGroup);
    
    QLineEdit *searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Поиск узла по номеру или координатам (например: 123 или 10.5 20.3)...");
    searchEdit->setStyleSheet("QLineEdit { padding: 5px; font-size: 11px; }");
    nodeListLayout->addWidget(searchEdit);
    
    // Улучшенный поиск - поддерживает поиск по координатам
    connect(searchEdit, &QLineEdit::textChanged, [this, searchEdit](const QString &text) {
        if (text.isEmpty()) {
            // Показываем все узлы
            for (int i = 0; i < nodeListWidget->count(); i++) {
                nodeListWidget->item(i)->setHidden(false);
            }
            return;
        }
        
        // Проверяем, является ли текст координатами (содержит пробел или запятую)
        bool isCoords = text.contains(" ") || text.contains(",");
        
        if (isCoords) {
            // Поиск по координатам
            QString textCopy = text;
            QStringList parts = textCopy.replace(",", " ").split(" ", Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                bool ok1, ok2;
                double x = parts[0].toDouble(&ok1);
                double y = parts[1].toDouble(&ok2);
                if (ok1 && ok2) {
                    // Ищем узлы в радиусе 0.1 от указанных координат
                    double tolerance = 0.1;
                    for (int i = 0; i < nodeListWidget->count(); i++) {
                        QListWidgetItem *item = nodeListWidget->item(i);
                        int nodeId = item->data(Qt::UserRole).toInt();
                        if (nodeCoords.contains(nodeId)) {
                            double dx = nodeCoords[nodeId].first - x;
                            double dy = nodeCoords[nodeId].second - y;
                            double dist = std::sqrt(dx*dx + dy*dy);
                            item->setHidden(dist > tolerance);
                        } else {
                            item->setHidden(true);
                        }
                    }
                    return;
                }
            }
        }
        
        // Обычный текстовый поиск
        for (int i = 0; i < nodeListWidget->count(); i++) {
            QListWidgetItem *item = nodeListWidget->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    });
    
    nodeListWidget = new QListWidget(this);
    nodeListWidget->setMaximumWidth(350);
    nodeListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    nodeListWidget->setAlternatingRowColors(true);
    nodeListLayout->addWidget(nodeListWidget);
    
    // Кнопка для выбора выделенного узла
    QPushButton *selectFromListBtn = new QPushButton("Выбрать выделенный узел", this);
    selectFromListBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 8px; }");
    nodeListLayout->addWidget(selectFromListBtn);
    
    
    // Одинарный клик - выделяет узел и заполняет поле
    connect(nodeListWidget, &QListWidget::itemClicked, [this](QListWidgetItem *item) {
        int nodeId = item->data(Qt::UserRole).toInt();
        if (nodeId > 0 && nodeCoords.contains(nodeId)) {
            nodeIdEdit->setText(QString::number(nodeId));
            QPair<double, double> coords = nodeCoords[nodeId];
            meshInfoText->append(QString("✓ Выбран узел %1: (%.2f, %.2f)\n").arg(nodeId).arg(coords.first).arg(coords.second));
            // Подсвечиваем выбранный узел
            for (int i = 0; i < nodeListWidget->count(); i++) {
                nodeListWidget->item(i)->setBackground(QBrush());
            }
            item->setBackground(QBrush(QColor(200, 230, 255)));
        }
    });
    
    // Двойной клик - сразу добавляет узел
    connect(nodeListWidget, &QListWidget::itemDoubleClicked, [this](QListWidgetItem *item) {
        int nodeId = item->data(Qt::UserRole).toInt();
        if (nodeId > 0 && nodeCoords.contains(nodeId)) {
            nodeIdEdit->setText(QString::number(nodeId));
            // Автоматически добавляем в зависимости от типа
            int type = constraintTypeCombo->currentIndex();
            if (type < 2) {
                // Закрепление
                addFixedNode();
            } else {
                // Нагрузка - нужно задать значения сил
                if (loadFxEdit->value() != 0.0 || loadFyEdit->value() != 0.0) {
                    addLoadNode();
                } else {
                    meshInfoText->append(QString("Узел %1 выбран. Задайте значения сил Fx и Fy, затем нажмите 'Добавить нагрузку'\n").arg(nodeId));
                }
            }
        }
    });
    
    // Кнопка выбора из списка
    connect(selectFromListBtn, &QPushButton::clicked, [this]() {
        QListWidgetItem *item = nodeListWidget->currentItem();
        if (item) {
            int nodeId = item->data(Qt::UserRole).toInt();
            if (nodeId > 0 && nodeCoords.contains(nodeId)) {
                nodeIdEdit->setText(QString::number(nodeId));
                QPair<double, double> coords = nodeCoords[nodeId];
                meshInfoText->append(QString("Выбран узел %1: (%.2f, %.2f)\n").arg(nodeId).arg(coords.first).arg(coords.second));
            }
        } else {
            QMessageBox::information(this, "Информация", "Выберите узел из списка (кликните на него)");
        }
    });
    
    nodeLayout->addWidget(nodeListGroup);
    
    // Правая часть - форма для добавления
    QGroupBox *formGroup = new QGroupBox("Добавление узлов", this);
    QVBoxLayout *formGroupLayout = new QVBoxLayout(formGroup);
    
    // Кнопка для открытия Gmsh (опционально, для дополнительной визуализации)
    QPushButton *openGmshBtn = new QPushButton("Открыть Gmsh (опционально)", this);
    openGmshBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px; font-size: 12px; }");
    formGroupLayout->addWidget(openGmshBtn);
    connect(openGmshBtn, &QPushButton::clicked, this, &NodeSelectionWindow::loadMesh);
    
    // Форма для добавления узлов
    QFormLayout *formLayout = new QFormLayout();
    
    nodeIdEdit = new QLineEdit(this);
    nodeIdEdit->setPlaceholderText("Номер узла (введите или выберите из списка)");
    nodeIdEdit->setStyleSheet("QLineEdit { padding: 5px; font-size: 12px; }");
    formLayout->addRow("Номер узла:", nodeIdEdit);
    
    // Кнопка для поиска узла по координатам (из Gmsh)
    QPushButton *findByCoordsBtn = new QPushButton("Найти узел по координатам (из Gmsh)", this);
    findByCoordsBtn->setStyleSheet("QPushButton { background-color: #FF9800; color: white; padding: 5px; }");
    formLayout->addRow("", findByCoordsBtn);
    connect(findByCoordsBtn, &QPushButton::clicked, [this]() {
        bool ok;
        QString text = QInputDialog::getText(this, "Поиск узла по координатам", 
            "Введите координаты из Gmsh (формат: x y или x,y):", 
            QLineEdit::Normal, "", &ok);
        if (ok && !text.isEmpty()) {
            QStringList parts = text.replace(",", " ").split(" ", Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                double x = parts[0].toDouble();
                double y = parts[1].toDouble();
                double minDist = 1e10;
                int nearestNode = -1;
                for (auto it = nodeCoords.begin(); it != nodeCoords.end(); ++it) {
                    double dx = it.value().first - x;
                    double dy = it.value().second - y;
                    double dist = std::sqrt(dx*dx + dy*dy);
                    if (dist < minDist) {
                        minDist = dist;
                        nearestNode = it.key();
                    }
                }
                if (nearestNode > 0 && minDist < 0.1) {
                    nodeIdEdit->setText(QString::number(nearestNode));
                    meshInfoText->append(QString("✓ Найден ближайший узел %1: (%.2f, %.2f), расстояние: %.4f\n")
                        .arg(nearestNode).arg(nodeCoords[nearestNode].first).arg(nodeCoords[nearestNode].second).arg(minDist));
                } else {
                    QMessageBox::warning(this, "Узел не найден", 
                        QString("Ближайший узел находится на расстоянии %.4f. Попробуйте другой способ выбора.").arg(minDist));
                }
            }
        }
    });
    
    constraintTypeCombo = new QComboBox(this);
    constraintTypeCombo->addItem("Закрепление по U (x)");
    constraintTypeCombo->addItem("Закрепление по V (y)");
    constraintTypeCombo->addItem("Сосредоточенная сила");
    constraintTypeCombo->addItem("Распределенная нагрузка");
    formLayout->addRow("Тип условия:", constraintTypeCombo);
    
    loadFxEdit = new QDoubleSpinBox(this);
    loadFxEdit->setRange(-1e10, 1e10);
    loadFxEdit->setValue(0.0);
    loadFxEdit->setSuffix(" Н");
    loadFxEdit->setDecimals(2);
    formLayout->addRow("Сила Fx:", loadFxEdit);
    
    loadFyEdit = new QDoubleSpinBox(this);
    loadFyEdit->setRange(-1e10, 1e10);
    loadFyEdit->setValue(0.0);
    loadFyEdit->setSuffix(" Н");
    loadFyEdit->setDecimals(2);
    formLayout->addRow("Сила Fy:", loadFyEdit);
    
    addFixedBtn = new QPushButton("Добавить закрепление", this);
    addLoadBtn = new QPushButton("Добавить нагрузку", this);
    formLayout->addRow(addFixedBtn);
    formLayout->addRow(addLoadBtn);
    
    connect(addFixedBtn, &QPushButton::clicked, this, &NodeSelectionWindow::addFixedNode);
    connect(addLoadBtn, &QPushButton::clicked, this, &NodeSelectionWindow::addLoadNode);
    connect(constraintTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                bool isLoad = (index == 2 || index == 3);
                loadFxEdit->setEnabled(isLoad);
                loadFyEdit->setEnabled(isLoad);
                addFixedBtn->setEnabled(!isLoad);
                addLoadBtn->setEnabled(isLoad);
            });
    
    formGroupLayout->addLayout(formLayout);
    nodeLayout->addWidget(formGroup);
    leftLayout->addWidget(nodeGroup);
    
    // Правая часть - визуализатор модели
    QGroupBox *viewerGroup = new QGroupBox("Визуализация модели (кликните на узел для выбора)", this);
    QVBoxLayout *viewerLayout = new QVBoxLayout(viewerGroup);
    
    meshViewer = new MeshViewer(this);
    meshViewer->setMinimumSize(900, 700);
    meshViewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    viewerLayout->addWidget(meshViewer);
    
    // Подключаем сигналы от визуализатора
    connect(meshViewer, &MeshViewer::nodeClicked, [this](int nodeId, const QPointF &coords) {
        if (nodeId > 0 && nodeCoords.contains(nodeId)) {
            nodeIdEdit->setText(QString::number(nodeId));
            meshInfoText->append(QString("✓ Выбран узел %1: (%.2f, %.2f)\n").arg(nodeId).arg(coords.x()).arg(coords.y()));
            
            // Подсвечиваем узел в визуализаторе
            selectedNodesInViewer.insert(nodeId);
            meshViewer->setSelectedNodes(selectedNodesInViewer);
        }
    });
    
    connect(meshViewer, &MeshViewer::nodeDoubleClicked, [this](int nodeId, const QPointF &coords) {
        if (nodeId > 0 && nodeCoords.contains(nodeId)) {
            nodeIdEdit->setText(QString::number(nodeId));
            // Автоматически добавляем в зависимости от типа
            int type = constraintTypeCombo->currentIndex();
            if (type < 2) {
                addFixedNode();
            } else {
                if (loadFxEdit->value() != 0.0 || loadFyEdit->value() != 0.0) {
                    addLoadNode();
                } else {
                    meshInfoText->append(QString("Узел %1 выбран. Задайте значения сил Fx и Fy, затем нажмите 'Добавить нагрузку'\n").arg(nodeId));
                }
            }
        }
    });
    
    // Добавляем виджеты в splitter
    mainSplitter->addWidget(leftWidget);
    mainSplitter->addWidget(viewerGroup);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);  // Увеличиваем долю визуализатора
    mainSplitter->setSizes(QList<int>() << 400 << 1200);  // Устанавливаем начальные размеры
    
    mainLayout->addWidget(mainSplitter);
    
    // Списки выбранных узлов
    QGroupBox *selectedGroup = new QGroupBox("Выбранные узлы", this);
    QHBoxLayout *selectedLayout = new QHBoxLayout(selectedGroup);
    
    fixedUListWidget = new QListWidget(this);
    fixedUListWidget->setMaximumWidth(200);
    selectedLayout->addWidget(new QLabel("Закрепления U:", this));
    selectedLayout->addWidget(fixedUListWidget);
    
    fixedVListWidget = new QListWidget(this);
    fixedVListWidget->setMaximumWidth(200);
    selectedLayout->addWidget(new QLabel("Закрепления V:", this));
    selectedLayout->addWidget(fixedVListWidget);
    
    loadedListWidget = new QListWidget(this);
    loadedListWidget->setMaximumWidth(200);
    selectedLayout->addWidget(new QLabel("Нагрузки:", this));
    selectedLayout->addWidget(loadedListWidget);
    
    removeFixedBtn = new QPushButton("Удалить закрепление", this);
    removeLoadBtn = new QPushButton("Удалить нагрузку", this);
    QVBoxLayout *removeLayout = new QVBoxLayout();
    removeLayout->addWidget(removeFixedBtn);
    removeLayout->addWidget(removeLoadBtn);
    selectedLayout->addLayout(removeLayout);
    
    connect(removeFixedBtn, &QPushButton::clicked, this, &NodeSelectionWindow::removeFixedNode);
    connect(removeLoadBtn, &QPushButton::clicked, this, &NodeSelectionWindow::removeLoadNode);
    
    mainLayout->addWidget(selectedGroup);
    
    // Кнопки
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    saveBtn = new QPushButton("Сохранить", this);
    cancelBtn = new QPushButton("Отмена", this);
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);
    
    connect(saveBtn, &QPushButton::clicked, this, &NodeSelectionWindow::saveAndClose);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void NodeSelectionWindow::readNodeFile() {
    if (!QFileInfo::exists(nodeFile)) {
        meshInfoText->append("Ошибка: Файл узлов не найден!");
        return;
    }
    
    QFile file(nodeFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        meshInfoText->append("Ошибка: Не удалось открыть файл узлов!");
        return;
    }
    
    QTextStream in(&file);
    int numNodes = in.readLine().toInt();
    
    meshInfoText->append(QString("Всего узлов: %1\n").arg(numNodes));
    meshInfoText->append(QString("Файл: %1\n").arg(nodeFile));
    
    for (int i = 1; i <= numNodes && !in.atEnd(); i++) {
        QString line = in.readLine();
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            double x = parts[0].toDouble();
            double y = parts[1].toDouble();
            nodeCoords[i] = QPair<double, double>(x, y);
            QListWidgetItem *item = new QListWidgetItem(QString("Узел %1: (%.2f, %.2f)").arg(i).arg(x).arg(y));
            item->setData(Qt::UserRole, i);  // Сохраняем номер узла
            nodeListWidget->addItem(item);
        }
    }
    
    meshInfoText->append(QString("Загружено %1 узлов в список\n").arg(nodeListWidget->count()));
    
    file.close();
}

void NodeSelectionWindow::loadMesh() {
    // Открываем Gmsh для визуализации
    QString gmshPath = "gmsh";
    QStringList searchPaths = {"/usr/local/bin/gmsh", "/opt/homebrew/bin/gmsh", "/usr/bin/gmsh"};
    
    bool found = false;
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            gmshPath = path;
            found = true;
            break;
        }
    }
    
    if (!found) {
        QProcess whichProc;
        whichProc.start("which", QStringList() << "gmsh");
        whichProc.waitForFinished();
        if (whichProc.exitCode() == 0) {
            gmshPath = QString(whichProc.readAllStandardOutput()).trimmed();
            found = true;
        }
    }
    
    if (found && QFileInfo::exists(mshFile)) {
        QProcess *gmshProcess = new QProcess(this);
        gmshProcess->startDetached(gmshPath, QStringList() << mshFile);
        meshInfoText->append("✓ Gmsh открыт для визуализации модели\n");
    } else {
        meshInfoText->append("⚠ Предупреждение: Gmsh не найден. Установите Gmsh для визуализации.\n");
    }
}

void NodeSelectionWindow::selectNodesVisual() {
    // Эта функция больше не используется, но оставляем для совместимости
    loadMesh();
}

void NodeSelectionWindow::addFixedNode() {
    bool ok;
    int nodeId = nodeIdEdit->text().toInt(&ok);
    
    if (!ok || nodeId < 1 || !nodeCoords.contains(nodeId)) {
        QMessageBox::warning(this, "Ошибка", "Введите корректный номер узла!");
        return;
    }
    
    // Обновляем визуализатор
    selectedNodesInViewer.insert(nodeId);
    meshViewer->setSelectedNodes(selectedNodesInViewer);
    
    int type = constraintTypeCombo->currentIndex();
    QString nodeStr = QString::number(nodeId);
    
    if (type == 0) {  // Закрепление по U
        if (!fixedNodesU.contains(nodeStr)) {
            fixedNodesU.append(nodeStr);
            fixedUListWidget->addItem(QString("Узел %1").arg(nodeId));
        }
    } else if (type == 1) {  // Закрепление по V
        if (!fixedNodesV.contains(nodeStr)) {
            fixedNodesV.append(nodeStr);
            fixedVListWidget->addItem(QString("Узел %1").arg(nodeId));
        }
    }
    
    nodeIdEdit->clear();
    emit boundaryConditionsChanged();
}

void NodeSelectionWindow::addLoadNode() {
    bool ok;
    int nodeId = nodeIdEdit->text().toInt(&ok);
    
    if (!ok || nodeId < 1 || !nodeCoords.contains(nodeId)) {
        QMessageBox::warning(this, "Ошибка", "Введите корректный номер узла!");
        return;
    }
    
    // Обновляем визуализатор
    selectedNodesInViewer.insert(nodeId);
    meshViewer->setSelectedNodes(selectedNodesInViewer);
    
    double fx = loadFxEdit->value();
    double fy = loadFyEdit->value();
    
    if (fx == 0.0 && fy == 0.0) {
        QMessageBox::warning(this, "Ошибка", "Задайте ненулевые значения сил!");
        return;
    }
    
    QString nodeStr = QString::number(nodeId);
    if (!loadedNodes.contains(nodeStr)) {
        loadedNodes.append(nodeStr);
        nodeLoads[nodeStr] = QPair<double, double>(fx, fy);
        loadedListWidget->addItem(QString("Узел %1: Fx=%.2f, Fy=%.2f").arg(nodeId).arg(fx).arg(fy));
    } else {
        // Обновляем нагрузку
        nodeLoads[nodeStr] = QPair<double, double>(fx, fy);
        // Обновляем отображение
        for (int i = 0; i < loadedListWidget->count(); i++) {
            if (loadedListWidget->item(i)->text().startsWith(QString("Узел %1:").arg(nodeId))) {
                loadedListWidget->item(i)->setText(QString("Узел %1: Fx=%.2f, Fy=%.2f").arg(nodeId).arg(fx).arg(fy));
                break;
            }
        }
    }
    
    nodeIdEdit->clear();
    loadFxEdit->setValue(0.0);
    loadFyEdit->setValue(0.0);
    emit boundaryConditionsChanged();
}

void NodeSelectionWindow::removeFixedNode() {
    QListWidgetItem *item = fixedUListWidget->currentItem();
    if (item) {
        QString text = item->text();
        int nodeId = text.split(" ")[1].toInt();
        fixedNodesU.removeAll(QString::number(nodeId));
        selectedNodesInViewer.remove(nodeId);
        delete item;
    }
    
    item = fixedVListWidget->currentItem();
    if (item) {
        QString text = item->text();
        int nodeId = text.split(" ")[1].toInt();
        fixedNodesV.removeAll(QString::number(nodeId));
        selectedNodesInViewer.remove(nodeId);
        delete item;
    }
    
    meshViewer->setSelectedNodes(selectedNodesInViewer);
    emit boundaryConditionsChanged();
}

void NodeSelectionWindow::removeLoadNode() {
    QListWidgetItem *item = loadedListWidget->currentItem();
    if (item) {
        QString text = item->text();
        int nodeId = text.split(" ")[1].toInt();
        QString nodeStr = QString::number(nodeId);
        loadedNodes.removeAll(nodeStr);
        nodeLoads.remove(nodeStr);
        selectedNodesInViewer.remove(nodeId);
        meshViewer->setSelectedNodes(selectedNodesInViewer);
        delete item;
    }
    
    emit boundaryConditionsChanged();
}

void NodeSelectionWindow::saveAndClose() {
    if (fixedNodesU.isEmpty() && fixedNodesV.isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "Не задано ни одного закрепления! Это может привести к ошибке расчета.");
    }
    
    accept();
}

