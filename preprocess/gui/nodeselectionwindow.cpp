#include "nodeselectionwindow.h"
#include "meshviewerwindow.h"
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
    : QDialog(parent), mshFile(mshFile), nodeFile(nodeFile), meshViewerWindow(nullptr) {
    setWindowTitle("Выбор узлов для граничных условий");
    setMinimumSize(600, 700);  // Уменьшили минимальный размер, так как визуализатор в отдельном окне
    resize(800, 800);
    
    setupUI();
    readNodeFile();
    
    // Создаем отдельное окно для визуализации модели
    if (QFileInfo::exists(mshFile)) {
        meshViewerWindow = new MeshViewerWindow(nodeFile, mshFile, this);
    } else {
        meshViewerWindow = new MeshViewerWindow(nodeFile, QString(), this);
    }
    
    // Подключаем сигналы от окна визуализации
    // Одиночный клик - только обновляет поле ввода, не подсвечивает узел
    connect(meshViewerWindow, &MeshViewerWindow::nodeClicked, [this](int nodeId, const QPointF &coords) {
        if (nodeId > 0 && nodeCoords.contains(nodeId)) {
            nodeIdEdit->setText(QString::number(nodeId));
            // Не подсвечиваем узел при одиночном клике
        }
    });
    
    // Подключаем сигнал для множественного выбора
    connect(meshViewerWindow, &MeshViewerWindow::nodesSelected, [this](const QSet<int> &nodeIds) {
        if (nodeIds.isEmpty()) return;
        
        // Обновляем визуализатор
        selectedNodesInViewer.unite(nodeIds);
        if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
            meshViewerWindow->getMeshViewer()->setSelectedNodes(selectedNodesInViewer);
        }
        
        // Добавляем информацию о выбранных узлах
        QStringList nodeIdList;
        for (int nodeId : nodeIds) {
            if (nodeCoords.contains(nodeId)) {
                QPair<double, double> coords = nodeCoords[nodeId];
                nodeIdList.append(QString::number(nodeId));
                meshInfoText->append(QString("✓ Выбран узел %1: (%2, %3)\n").arg(nodeId).arg(coords.first, 0, 'f', 2).arg(coords.second, 0, 'f', 2));
            }
        }
        
        // Устанавливаем все выбранные узлы в поле ввода (через запятую, если их много)
        if (!nodeIdList.isEmpty()) {
            if (nodeIdList.size() <= 10) {
                // Если узлов немного, показываем все через запятую
                nodeIdEdit->setText(nodeIdList.join(", "));
            } else {
                // Если узлов много, показываем диапазон
                QString first = nodeIdList.first();
                QString last = nodeIdList.last();
                nodeIdEdit->setText(QString("%1 ... %2 (%3 узлов)").arg(first).arg(last).arg(nodeIdList.size()));
            }
        }
        
        meshInfoText->append(QString("Всего выбрано узлов: %1\n").arg(nodeIds.size()));
    });
    
    connect(meshViewerWindow, &MeshViewerWindow::nodeDoubleClicked, [this](int nodeId, const QPointF &coords) {
        if (nodeId > 0 && nodeCoords.contains(nodeId)) {
            nodeIdEdit->setText(QString::number(nodeId));
            
            // Подсвечиваем узел в визуализаторе при двойном клике
            selectedNodesInViewer.insert(nodeId);
            if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
                meshViewerWindow->getMeshViewer()->setSelectedNodes(selectedNodesInViewer);
            }
            
            meshInfoText->append(QString("✓ Выбран узел %1: (%2, %3)\n").arg(nodeId).arg(coords.x(), 0, 'f', 2).arg(coords.y(), 0, 'f', 2));
            
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
    
    // Показываем окно визуализации
    meshViewerWindow->show();
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
    nodeListWidget->setMaximumWidth(280);  // Уменьшили с 350 до 280
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
            meshInfoText->append(QString("✓ Выбран узел %1: (%2, %3)\n").arg(nodeId).arg(coords.first, 0, 'f', 2).arg(coords.second, 0, 'f', 2));
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
                meshInfoText->append(QString("Выбран узел %1: (%2, %3)\n").arg(nodeId).arg(coords.first, 0, 'f', 2).arg(coords.second, 0, 'f', 2));
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
                    meshInfoText->append(QString("✓ Найден ближайший узел %1: (%2, %3), расстояние: %4\n")
                        .arg(nearestNode).arg(nodeCoords[nearestNode].first, 0, 'f', 2).arg(nodeCoords[nearestNode].second, 0, 'f', 2).arg(minDist, 0, 'f', 4));
                } else {
                    QMessageBox::warning(this, "Узел не найден", 
                        QString("Ближайший узел находится на расстоянии %1. Попробуйте другой способ выбора.").arg(minDist, 0, 'f', 4));
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
    
    // Кнопка для открытия окна визуализации модели
    QPushButton *openViewerBtn = new QPushButton("Открыть окно визуализации модели", this);
    openViewerBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 15px; font-size: 14px; font-weight: bold; }");
    openViewerBtn->setMinimumHeight(50);
    connect(openViewerBtn, &QPushButton::clicked, [this]() {
        if (meshViewerWindow) {
            meshViewerWindow->show();
            meshViewerWindow->raise();
            meshViewerWindow->activateWindow();
        }
    });
    
    // Добавляем виджеты в layout (без splitter, так как визуализатор в отдельном окне)
    mainLayout->addWidget(leftWidget);
    mainLayout->addWidget(openViewerBtn);
    
    // Списки выбранных узлов
    QGroupBox *selectedGroup = new QGroupBox("Выбранные узлы", this);
    QHBoxLayout *selectedLayout = new QHBoxLayout(selectedGroup);
    
    fixedUListWidget = new QListWidget(this);
    fixedUListWidget->setMaximumWidth(150);  // Уменьшили с 200 до 150
    selectedLayout->addWidget(new QLabel("Закрепления U:", this));
    selectedLayout->addWidget(fixedUListWidget);
    
    fixedVListWidget = new QListWidget(this);
    fixedVListWidget->setMaximumWidth(150);  // Уменьшили с 200 до 150
    selectedLayout->addWidget(new QLabel("Закрепления V:", this));
    selectedLayout->addWidget(fixedVListWidget);
    
    loadedListWidget = new QListWidget(this);
    loadedListWidget->setMaximumWidth(150);  // Уменьшили с 200 до 150
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
            QListWidgetItem *item = new QListWidgetItem(QString("Узел %1: (%2, %3)").arg(i).arg(x, 0, 'f', 2).arg(y, 0, 'f', 2));
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
    // Получаем узлы для добавления: либо из поля ввода, либо из выбранных узлов
    QSet<int> nodesToAdd;
    
    // Сначала пытаемся получить узлы из поля ввода
    QString nodeText = nodeIdEdit->text().trimmed();
    if (!nodeText.isEmpty()) {
        // Пытаемся распарсить как число
        bool ok;
        int nodeId = nodeText.toInt(&ok);
        if (ok && nodeId > 0 && nodeCoords.contains(nodeId)) {
            nodesToAdd.insert(nodeId);
        } else {
            // Пытаемся распарсить как список узлов (через запятую)
            QStringList parts = nodeText.split(",", Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                QString trimmed = part.trimmed();
                // Убираем возможные скобки и текст вроде "(115 узлов)"
                if (trimmed.contains("(")) {
                    trimmed = trimmed.split("(")[0].trimmed();
                }
                if (trimmed.contains("...")) {
                    // Это диапазон, пропускаем
                    continue;
                }
                int id = trimmed.toInt(&ok);
                if (ok && id > 0 && nodeCoords.contains(id)) {
                    nodesToAdd.insert(id);
                }
            }
        }
    }
    
    // Если не удалось получить узлы из поля ввода, используем выбранные узлы
    if (nodesToAdd.isEmpty() && !selectedNodesInViewer.isEmpty()) {
        nodesToAdd = selectedNodesInViewer;
    }
    
    if (nodesToAdd.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите узлы для добавления закрепления!");
        return;
    }
    
    // Обновляем визуализатор
    selectedNodesInViewer.unite(nodesToAdd);
    if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
        meshViewerWindow->getMeshViewer()->setSelectedNodes(selectedNodesInViewer);
    }
    
    int type = constraintTypeCombo->currentIndex();
    int addedCount = 0;
    
    if (type == 0) {  // Закрепление по U
        for (int nodeId : nodesToAdd) {
            QString nodeStr = QString::number(nodeId);
            // Проверяем, не является ли этот узел узлом с нагрузкой
            if (loadedNodes.contains(nodeStr)) {
                meshInfoText->append(QString("⚠ Узел %1 имеет нагрузку, пропускаем закрепление по U\n").arg(nodeId));
                continue;
            }
            if (!fixedNodesU.contains(nodeStr)) {
                fixedNodesU.append(nodeStr);
                fixedUListWidget->addItem(QString("Узел %1").arg(nodeId));
                addedCount++;
            }
        }
        
        // Обновляем визуализацию
        if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
            QSet<int> fixedU;
            for (const QString &n : fixedNodesU) {
                fixedU.insert(n.toInt());
            }
            meshViewerWindow->getMeshViewer()->setFixedNodesU(fixedU);
        }
    } else if (type == 1) {  // Закрепление по V
        for (int nodeId : nodesToAdd) {
            QString nodeStr = QString::number(nodeId);
            // Проверяем, не является ли этот узел узлом с нагрузкой
            if (loadedNodes.contains(nodeStr)) {
                meshInfoText->append(QString("⚠ Узел %1 имеет нагрузку, пропускаем закрепление по V\n").arg(nodeId));
                continue;
            }
            if (!fixedNodesV.contains(nodeStr)) {
                fixedNodesV.append(nodeStr);
                fixedVListWidget->addItem(QString("Узел %1").arg(nodeId));
                addedCount++;
            }
        }
        
        // Обновляем визуализацию
        if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
            QSet<int> fixedV;
            for (const QString &n : fixedNodesV) {
                fixedV.insert(n.toInt());
            }
            meshViewerWindow->getMeshViewer()->setFixedNodesV(fixedV);
        }
    }
    
    if (addedCount > 0) {
        meshInfoText->append(QString("✓ Добавлено закреплений: %1\n").arg(addedCount));
    }
    
    // Очищаем выбранные узлы после применения закреплений
    selectedNodesInViewer.clear();
    if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
        meshViewerWindow->getMeshViewer()->setSelectedNodes(selectedNodesInViewer);
    }
    
    nodeIdEdit->clear();
    emit boundaryConditionsChanged();
}

void NodeSelectionWindow::addLoadNode() {
    // Получаем узлы для добавления: либо из поля ввода, либо из выбранных узлов
    QSet<int> nodesToAdd;
    
    // Сначала пытаемся получить узлы из поля ввода
    QString nodeText = nodeIdEdit->text().trimmed();
    if (!nodeText.isEmpty()) {
        // Пытаемся распарсить как число
        bool ok;
        int nodeId = nodeText.toInt(&ok);
        if (ok && nodeId > 0 && nodeCoords.contains(nodeId)) {
            nodesToAdd.insert(nodeId);
        } else {
            // Пытаемся распарсить как список узлов (через запятую)
            QStringList parts = nodeText.split(",", Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                QString trimmed = part.trimmed();
                // Убираем возможные скобки и текст вроде "(115 узлов)"
                if (trimmed.contains("(")) {
                    trimmed = trimmed.split("(")[0].trimmed();
                }
                if (trimmed.contains("...")) {
                    // Это диапазон, пропускаем
                    continue;
                }
                int id = trimmed.toInt(&ok);
                if (ok && id > 0 && nodeCoords.contains(id)) {
                    nodesToAdd.insert(id);
                }
            }
        }
    }
    
    // Если не удалось получить узлы из поля ввода, используем выбранные узлы
    if (nodesToAdd.isEmpty() && !selectedNodesInViewer.isEmpty()) {
        nodesToAdd = selectedNodesInViewer;
    }
    
    if (nodesToAdd.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите узлы для добавления нагрузки!");
        return;
    }
    
    double fx = loadFxEdit->value();
    double fy = loadFyEdit->value();
    
    if (fx == 0.0 && fy == 0.0) {
        QMessageBox::warning(this, "Ошибка", "Задайте ненулевые значения сил!");
        return;
    }
    
    // Обновляем визуализатор
    selectedNodesInViewer.unite(nodesToAdd);
    if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
        meshViewerWindow->getMeshViewer()->setSelectedNodes(selectedNodesInViewer);
    }
    
    int addedCount = 0;
    int skippedCount = 0;
    for (int nodeId : nodesToAdd) {
        QString nodeStr = QString::number(nodeId);
        // Проверяем, не закреплен ли этот узел
        // Если узел закреплен, пропускаем его с предупреждением (не удаляем закрепление автоматически)
        if (fixedNodesU.contains(nodeStr)) {
            meshInfoText->append(QString("⚠ Узел %1 закреплен по U, пропускаем добавление нагрузки\n").arg(nodeId));
            skippedCount++;
            continue;
        }
        if (fixedNodesV.contains(nodeStr)) {
            meshInfoText->append(QString("⚠ Узел %1 закреплен по V, пропускаем добавление нагрузки\n").arg(nodeId));
            skippedCount++;
            continue;
        }
        if (!loadedNodes.contains(nodeStr)) {
            loadedNodes.append(nodeStr);
            nodeLoads[nodeStr] = QPair<double, double>(fx, fy);
            loadedListWidget->addItem(QString("Узел %1: Fx=%2, Fy=%3").arg(nodeId).arg(fx, 0, 'f', 2).arg(fy, 0, 'f', 2));
            addedCount++;
        } else {
            // Обновляем нагрузку
            nodeLoads[nodeStr] = QPair<double, double>(fx, fy);
            // Обновляем отображение
            for (int i = 0; i < loadedListWidget->count(); i++) {
                if (loadedListWidget->item(i)->text().startsWith(QString("Узел %1:").arg(nodeId))) {
                    loadedListWidget->item(i)->setText(QString("Узел %1: Fx=%2, Fy=%3").arg(nodeId).arg(fx, 0, 'f', 2).arg(fy, 0, 'f', 2));
                    break;
                }
            }
        }
    }
    
    // Обновляем визуализацию сил
    if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
        QMap<int, QPair<double, double>> loads;
        for (auto it = nodeLoads.begin(); it != nodeLoads.end(); ++it) {
            loads.insert(it.key().toInt(), it.value());
        }
        meshViewerWindow->getMeshViewer()->setLoadNodes(loads);
    }
    
    if (addedCount > 0) {
        meshInfoText->append(QString("✓ Добавлено нагрузок: %1\n").arg(addedCount));
    }
    if (skippedCount > 0) {
        meshInfoText->append(QString("⚠ Пропущено узлов (закреплены): %1\n").arg(skippedCount));
    }
    
    // Очищаем выбранные узлы после применения нагрузок
    selectedNodesInViewer.clear();
    if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
        meshViewerWindow->getMeshViewer()->setSelectedNodes(selectedNodesInViewer);
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
        
        // Обновляем визуализацию
        if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
            QSet<int> fixedU;
            for (const QString &n : fixedNodesU) {
                fixedU.insert(n.toInt());
            }
            meshViewerWindow->getMeshViewer()->setFixedNodesU(fixedU);
        }
    }
    
    item = fixedVListWidget->currentItem();
    if (item) {
        QString text = item->text();
        int nodeId = text.split(" ")[1].toInt();
        fixedNodesV.removeAll(QString::number(nodeId));
        selectedNodesInViewer.remove(nodeId);
        delete item;
        
        // Обновляем визуализацию
        if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
            QSet<int> fixedV;
            for (const QString &n : fixedNodesV) {
                fixedV.insert(n.toInt());
            }
            meshViewerWindow->getMeshViewer()->setFixedNodesV(fixedV);
        }
    }
    
    if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
        meshViewerWindow->getMeshViewer()->setSelectedNodes(selectedNodesInViewer);
    }
    emit boundaryConditionsChanged();
}

void NodeSelectionWindow::removeLoadNode() {
    QListWidgetItem *item = loadedListWidget->currentItem();
    if (item) {
        QString text = item->text();
        int nodeId = text.split(" ")[1].split(":")[0].toInt();
        QString nodeStr = QString::number(nodeId);
        loadedNodes.removeAll(nodeStr);
        nodeLoads.remove(nodeStr);
        selectedNodesInViewer.remove(nodeId);
        if (meshViewerWindow && meshViewerWindow->getMeshViewer()) {
            meshViewerWindow->getMeshViewer()->setSelectedNodes(selectedNodesInViewer);
            
            // Обновляем визуализацию сил
            QMap<int, QPair<double, double>> loads;
            for (auto it = nodeLoads.begin(); it != nodeLoads.end(); ++it) {
                loads.insert(it.key().toInt(), it.value());
            }
            meshViewerWindow->getMeshViewer()->setLoadNodes(loads);
        }
        delete item;
    }
    
    emit boundaryConditionsChanged();
}

void NodeSelectionWindow::saveAndClose() {
    if (fixedNodesU.isEmpty() && fixedNodesV.isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "Не задано ни одного закрепления! Это может привести к ошибке расчета.");
    }
    
    // Сохраняем граничные условия в файл node.txt
    if (!saveBoundaryConditionsToFile()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить граничные условия в файл!");
        return;
    }
    
    accept();
}

bool NodeSelectionWindow::saveBoundaryConditionsToFile() {
    QFile file(nodeFile);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        return false;
    }
    
    QTextStream in(&file);
    
    // Читаем весь файл
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    
    if (lines.isEmpty()) {
        file.close();
        return false;
    }
    
    file.resize(0);  // Очищаем файл
    file.seek(0);
    QTextStream out(&file);
    
    int numNodes = lines[0].toInt();
    out << numNodes << "\n";
    
    // Создаем множества для быстрого поиска
    QSet<int> fixedUSet, fixedVSet, loadedSet;
    for (const QString &nodeStr : fixedNodesU) {
        fixedUSet.insert(nodeStr.toInt());
    }
    for (const QString &nodeStr : fixedNodesV) {
        fixedVSet.insert(nodeStr.toInt());
    }
    for (const QString &nodeStr : loadedNodes) {
        loadedSet.insert(nodeStr.toInt());
    }
    
    // Записываем узлы с граничными условиями
    for (int i = 1; i <= numNodes && i < lines.size(); i++) {
        QString line = lines[i];
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        
        if (parts.size() >= 3) {
            double x = parts[0].toDouble();
            double y = parts[1].toDouble();
            double z = parts[2].toDouble();
            
            // Определяем флаги граничных условий
            // u_flag: 0 = закреплен по U, 1 = свободен
            // v_flag: 0 = закреплен по V, 1 = свободен
            // load_flag: 100 = есть нагрузка, 0 = нет нагрузки
            int u_flag = fixedUSet.contains(i) ? 0 : 1;
            int v_flag = fixedVSet.contains(i) ? 0 : 1;
            int load_flag = loadedSet.contains(i) ? 100 : 0;
            
            // Записываем: x y z u_flag v_flag load_flag
            out << QString::number(x, 'g', 15) << " "
                << QString::number(y, 'g', 15) << " "
                << QString::number(z, 'g', 15) << " "
                << u_flag << " "
                << v_flag << " "
                << load_flag << "\n";
        } else {
            // Если формат не соответствует ожидаемому, записываем как есть
            out << line << "\n";
        }
    }
    
    // Записываем элементы (если они были)
    int elemStartIdx = numNodes + 1;
    if (elemStartIdx < lines.size()) {
        for (int i = elemStartIdx; i < lines.size(); i++) {
            out << lines[i] << "\n";
        }
    }
    
    file.close();
    
    // Сохраняем нагрузки в отдельный файл loads.txt
    QString loadsFile = QFileInfo(nodeFile).absolutePath() + "/loads.txt";
    QFile loadsFileHandle(loadsFile);
    if (loadsFileHandle.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream loadsOut(&loadsFileHandle);
        for (auto it = nodeLoads.begin(); it != nodeLoads.end(); ++it) {
            int nodeId = it.key().toInt();
            double fx = it.value().first;
            double fy = it.value().second;
            loadsOut << nodeId << " " << QString::number(fx, 'g', 15) << " " << QString::number(fy, 'g', 15) << "\n";
        }
        loadsFileHandle.close();
    }
    
    return true;
}

