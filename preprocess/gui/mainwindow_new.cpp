#include "mainwindow.h"
#include "nodeselectionwindow.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QFile>
#include <QTabWidget>
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), process(nullptr), nodeSelectionWindow(nullptr) {
    // Определяем корень проекта
    QString appPath = QApplication::applicationFilePath();
    QFileInfo appInfo(appPath);
    QDir appDir = appInfo.absoluteDir();
    
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    while (!appDir.isRoot() && !QDir(appDir.absolutePath() + "/HyperMesh").exists()) {
        if (appDir.absolutePath() == desktopPath || appDir.absolutePath().count("/") <= 2) {
            break;
        }
        appDir.cdUp();
    }
    
    projectRoot = appDir.absolutePath();
    if (projectRoot.isEmpty() || !QDir(projectRoot + "/HyperMesh").exists()) {
        projectRoot = "/Users/matvej/Desktop/SmartFEM";
    }
    
    if (projectRoot == desktopPath || projectRoot.contains(desktopPath + "/build")) {
        projectRoot = "/Users/matvej/Desktop/SmartFEM";
    }
    
    setupUI();
    
    // Установить значения по умолчанию
    stepFileEdit->setText(projectRoot + "/materials/data-sample/52.stp");
    outputDirEdit->setText(projectRoot + "/build");
    resultFileEdit->setText("result.txt");
    
    eEdit->setValue(2.1e5);
    nuEdit->setValue(0.3);
    rhoEdit->setValue(7850.0);
    hEdit->setValue(1.0);
    
    numElementsEdit->setValue(500);
    clminEdit->setValue(0.8);
    clmaxEdit->setValue(6.0);
    
    numModesEdit->setValue(5);
    
    outputText->append(QString("Project root: %1\n").arg(projectRoot));
}

MainWindow::~MainWindow() {
    if (process) {
        process->kill();
        process->deleteLater();
    }
    if (nodeSelectionWindow) {
        nodeSelectionWindow->deleteLater();
    }
}

void MainWindow::setupUI() {
    setWindowTitle("SmartFEM - Параметры расчета");
    setMinimumSize(1000, 700);
    
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Создаем вкладки для 5 разделов
    QTabWidget *tabWidget = new QTabWidget(this);
    
    // Раздел 1: Выбор файла и места сохранения
    QWidget *fileTab = new QWidget();
    QVBoxLayout *fileLayout = new QVBoxLayout(fileTab);
    setupFileGroup();
    fileLayout->addWidget(createFileGroup());
    tabWidget->addTab(fileTab, "1. Файлы");
    
    // Раздел 2: Материал
    QWidget *materialTab = new QWidget();
    QVBoxLayout *materialLayout = new QVBoxLayout(materialTab);
    setupMaterialGroup();
    materialLayout->addWidget(createMaterialGroup());
    tabWidget->addTab(materialTab, "2. Материал");
    
    // Раздел 3: Параметры сетки
    QWidget *meshTab = new QWidget();
    QVBoxLayout *meshLayout = new QVBoxLayout(meshTab);
    setupMeshGroup();
    meshLayout->addWidget(createMeshGroup());
    tabWidget->addTab(meshTab, "3. Сетка");
    
    // Раздел 4: Граничные условия и нагрузки
    QWidget *boundaryTab = new QWidget();
    QVBoxLayout *boundaryLayout = new QVBoxLayout(boundaryTab);
    setupBoundaryGroup();
    boundaryLayout->addWidget(createBoundaryGroup());
    tabWidget->addTab(boundaryTab, "4. Граничные условия");
    
    // Раздел 5: Выбор расчета
    QWidget *analysisTab = new QWidget();
    QVBoxLayout *analysisLayout = new QVBoxLayout(analysisTab);
    setupAnalysisGroup();
    analysisLayout->addWidget(createAnalysisGroup());
    tabWidget->addTab(analysisTab, "5. Расчет");
    
    mainLayout->addWidget(tabWidget);
    
    // Вывод
    QGroupBox *outputGroup = new QGroupBox("Вывод", this);
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);
    outputText = new QTextEdit(this);
    outputText->setReadOnly(true);
    outputText->setFont(QFont("Courier", 10));
    outputText->setMaximumHeight(200);
    outputLayout->addWidget(outputText);
    mainLayout->addWidget(outputGroup);
    
    // Кнопка запуска
    runBtn = new QPushButton("Запустить расчет", this);
    runBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-size: 16px; padding: 15px; font-weight: bold; }");
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::runAnalysis);
    mainLayout->addWidget(runBtn);
}

QGroupBox* MainWindow::createFileGroup() {
    QGroupBox *group = new QGroupBox("Выбор файла и места сохранения", this);
    QFormLayout *layout = new QFormLayout(group);
    
    stepFileEdit = new QLineEdit(this);
    browseStepBtn = new QPushButton("Обзор...", this);
    QHBoxLayout *stepLayout = new QHBoxLayout();
    stepLayout->addWidget(stepFileEdit);
    stepLayout->addWidget(browseStepBtn);
    layout->addRow("STEP файл:", stepLayout);
    connect(browseStepBtn, &QPushButton::clicked, this, &MainWindow::browseStepFile);
    
    outputDirEdit = new QLineEdit(this);
    browseOutputBtn = new QPushButton("Обзор...", this);
    QHBoxLayout *outputDirLayout = new QHBoxLayout();
    outputDirLayout->addWidget(outputDirEdit);
    outputDirLayout->addWidget(browseOutputBtn);
    layout->addRow("Выходная директория:", outputDirLayout);
    connect(browseOutputBtn, &QPushButton::clicked, this, &MainWindow::browseOutputDir);
    
    resultFileEdit = new QLineEdit(this);
    resultFileEdit->setPlaceholderText("result.txt");
    layout->addRow("Имя файла результатов:", resultFileEdit);
    
    return group;
}

QGroupBox* MainWindow::createMaterialGroup() {
    QGroupBox *group = new QGroupBox("Параметры материала", this);
    QFormLayout *layout = new QFormLayout(group);
    
    eEdit = new QDoubleSpinBox(this);
    eEdit->setRange(1.0, 1.0e12);
    eEdit->setValue(2.1e5);
    eEdit->setSuffix(" МПа");
    eEdit->setDecimals(0);
    layout->addRow("Модуль упругости E:", eEdit);
    
    nuEdit = new QDoubleSpinBox(this);
    nuEdit->setRange(0.0, 0.5);
    nuEdit->setValue(0.3);
    nuEdit->setDecimals(3);
    nuEdit->setSingleStep(0.01);
    layout->addRow("Коэффициент Пуассона ν:", nuEdit);
    
    rhoEdit = new QDoubleSpinBox(this);
    rhoEdit->setRange(1.0, 50000.0);
    rhoEdit->setValue(7850.0);
    rhoEdit->setSuffix(" кг/м³");
    rhoEdit->setDecimals(1);
    layout->addRow("Плотность ρ:", rhoEdit);
    
    hEdit = new QDoubleSpinBox(this);
    hEdit->setRange(0.001, 100.0);
    hEdit->setValue(1.0);
    hEdit->setSuffix(" м");
    hEdit->setDecimals(3);
    layout->addRow("Толщина h:", hEdit);
    
    return group;
}

QGroupBox* MainWindow::createMeshGroup() {
    QGroupBox *group = new QGroupBox("Параметры сетки", this);
    QFormLayout *layout = new QFormLayout(group);
    
    numElementsEdit = new QSpinBox(this);
    numElementsEdit->setRange(10, 100000);
    numElementsEdit->setValue(500);
    calcMeshParamsBtn = new QPushButton("Рассчитать параметры", this);
    QHBoxLayout *numElementsLayout = new QHBoxLayout();
    numElementsLayout->addWidget(numElementsEdit);
    numElementsLayout->addWidget(calcMeshParamsBtn);
    layout->addRow("Желаемое количество элементов:", numElementsLayout);
    connect(calcMeshParamsBtn, &QPushButton::clicked, this, &MainWindow::calculateMeshParams);
    
    clminEdit = new QDoubleSpinBox(this);
    clminEdit->setRange(0.001, 100.0);
    clminEdit->setValue(0.8);
    clminEdit->setDecimals(3);
    layout->addRow("Минимальный размер элемента (clmin):", clminEdit);
    
    clmaxEdit = new QDoubleSpinBox(this);
    clmaxEdit->setRange(0.01, 100.0);
    clmaxEdit->setValue(6.0);
    clmaxEdit->setDecimals(3);
    layout->addRow("Максимальный размер элемента (clmax):", clmaxEdit);
    
    return group;
}

QGroupBox* MainWindow::createBoundaryGroup() {
    QGroupBox *group = new QGroupBox("Граничные условия и нагрузки", this);
    QVBoxLayout *layout = new QVBoxLayout(group);
    
    QLabel *infoLabel = new QLabel(
        "Нажмите кнопку ниже, чтобы открыть окно визуализации модели.\n"
        "В этом окне вы сможете выбрать узлы для задания:\n"
        "- Закреплений (по U или V)\n"
        "- Сосредоточенных сил\n"
        "- Распределенных нагрузок", this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);
    
    openNodeSelectionBtn = new QPushButton("Открыть окно выбора узлов", this);
    openNodeSelectionBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 10px; font-size: 14px; }");
    connect(openNodeSelectionBtn, &QPushButton::clicked, this, &MainWindow::openNodeSelectionWindow);
    layout->addWidget(openNodeSelectionBtn);
    
    boundaryConditionsLabel = new QLabel("Закрепления: не заданы", this);
    loadsLabel = new QLabel("Нагрузки: не заданы", this);
    layout->addWidget(boundaryConditionsLabel);
    layout->addWidget(loadsLabel);
    
    return group;
}

QGroupBox* MainWindow::createAnalysisGroup() {
    QGroupBox *group = new QGroupBox("Выбор расчета", this);
    QFormLayout *layout = new QFormLayout(group);
    
    analysisTypeCombo = new QComboBox(this);
    analysisTypeCombo->addItem("Статический FEM анализ");
    analysisTypeCombo->addItem("Модальный анализ (колебания)");
    layout->addRow("Тип анализа:", analysisTypeCombo);
    
    numModesEdit = new QSpinBox(this);
    numModesEdit->setRange(1, 100);
    numModesEdit->setValue(5);
    numModesEdit->setEnabled(false);
    layout->addRow("Количество мод:", numModesEdit);
    
    connect(analysisTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                numModesEdit->setEnabled(index == 1);
            });
    
    return group;
}

void MainWindow::setupFileGroup() {
    // Уже создано в createFileGroup()
}

void MainWindow::setupMaterialGroup() {
    // Уже создано в createMaterialGroup()
}

void MainWindow::setupMeshGroup() {
    // Уже создано в createMeshGroup()
}

void MainWindow::setupBoundaryGroup() {
    // Уже создано в createBoundaryGroup()
}

void MainWindow::setupAnalysisGroup() {
    // Уже создано в createAnalysisGroup()
}

void MainWindow::browseStepFile() {
    QString fileName = QFileDialog::getOpenFileName(this,
        "Выберите STEP файл",
        projectRoot + "/materials/data-sample",
        "STEP Files (*.stp *.step);;All Files (*)");
    if (!fileName.isEmpty()) {
        stepFileEdit->setText(fileName);
    }
}

void MainWindow::browseOutputDir() {
    QString dirName = QFileDialog::getExistingDirectory(this,
        "Выберите выходную директорию",
        projectRoot + "/build");
    if (!dirName.isEmpty()) {
        outputDirEdit->setText(dirName);
    }
}

void MainWindow::calculateMeshParams() {
    int desiredElements = numElementsEdit->value();
    
    double estimatedArea = 470.0;
    double factor = 82.8;
    
    double clmax = sqrt(estimatedArea * factor / desiredElements);
    double clminRatio = (desiredElements < 1000) ? 0.2 : 0.15;
    double clmin = clmax * clminRatio;
    
    if (clmin < 0.05) clmin = 0.05;
    if (clmax < 0.1) clmax = 0.1;
    if (clmax > 50.0) clmax = 50.0;
    
    clminEdit->setValue(clmin);
    clmaxEdit->setValue(clmax);
    
    double estimatedElements = estimatedArea * factor / (clmax * clmax);
    outputText->append(QString("Рассчитаны параметры сетки:\n")
                      .append(QString("  Желаемое: %1, Ожидаемое: ~%2\n")
                      .arg(desiredElements).arg((int)estimatedElements))
                      .append(QString("  clmin=%.3f, clmax=%.3f\n").arg(clmin).arg(clmax)));
}

void MainWindow::openNodeSelectionWindow() {
    // Сначала нужно сгенерировать сетку, если её еще нет
    QString stepFile = stepFileEdit->text();
    if (stepFile.isEmpty() || !QFileInfo::exists(stepFile)) {
        QMessageBox::warning(this, "Ошибка", "Сначала выберите STEP файл!");
        return;
    }
    
    // Генерируем сетку с текущими параметрами
    QString clmin = QString::number(clminEdit->value(), 'g', 6);
    QString clmax = QString::number(clmaxEdit->value(), 'g', 6);
    
    outputText->append("Генерация сетки для визуализации...\n");
    
    // Запускаем генерацию сетки синхронно
    QProcess meshProcess;
    QString meshScript = projectRoot + "/HyperMesh/generate_mesh.sh";
    meshProcess.setWorkingDirectory(projectRoot + "/HyperMesh");
    meshProcess.start("/bin/bash", QStringList() << meshScript << stepFile << clmin << clmax);
    
    if (!meshProcess.waitForFinished(30000)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось сгенерировать сетку!");
        return;
    }
    
    if (meshProcess.exitCode() != 0) {
        QMessageBox::critical(this, "Ошибка", "Ошибка генерации сетки!");
        return;
    }
    
    QString mshFile = projectRoot + "/build/HyperMesh.msh";
    QString nodeFile = projectRoot + "/build/node.txt";
    
    if (!QFileInfo::exists(nodeFile)) {
        QMessageBox::critical(this, "Ошибка", "Файл сетки не найден!");
        return;
    }
    
    // Открываем окно выбора узлов
    if (nodeSelectionWindow) {
        nodeSelectionWindow->deleteLater();
    }
    
    nodeSelectionWindow = new NodeSelectionWindow(mshFile, nodeFile, this);
    connect(nodeSelectionWindow, &NodeSelectionWindow::boundaryConditionsChanged,
            this, &MainWindow::onBoundaryConditionsChanged);
    
    if (nodeSelectionWindow->exec() == QDialog::Accepted) {
        fixedNodesU = nodeSelectionWindow->getFixedNodesU();
        fixedNodesV = nodeSelectionWindow->getFixedNodesV();
        loadedNodes = nodeSelectionWindow->getLoadedNodes();
        nodeLoads = nodeSelectionWindow->getNodeLoads();
        
        onBoundaryConditionsChanged();
    }
}

void MainWindow::onBoundaryConditionsChanged() {
    QString bcText = QString("Закрепления: U_fixed=%1, V_fixed=%2")
                     .arg(fixedNodesU.size()).arg(fixedNodesV.size());
    boundaryConditionsLabel->setText(bcText);
    
    QString loadsText = QString("Нагрузки: %1 узлов")
                       .arg(loadedNodes.size());
    loadsLabel->setText(loadsText);
}

// Продолжение в следующем файле из-за размера...

