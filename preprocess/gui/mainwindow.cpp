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
    connect(nodeSelectionWindow, SIGNAL(boundaryConditionsChanged()),
            this, SLOT(onBoundaryConditionsChanged()));
    
    int result = static_cast<QDialog*>(nodeSelectionWindow)->exec();
    if (result == QDialog::Accepted) {
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

// Продолжение mainwindow.cpp - добавление недостающих методов

void MainWindow::runAnalysis() {
    if (process && process->state() == QProcess::Running) {
        QMessageBox::warning(this, "Предупреждение", "Расчет уже выполняется!");
        return;
    }
    
    // Проверка параметров
    if (stepFileEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Укажите STEP файл!");
        return;
    }
    
    if (!QFileInfo::exists(stepFileEdit->text())) {
        QMessageBox::warning(this, "Ошибка", "STEP файл не найден!");
        return;
    }
    
    outputText->clear();
    outputText->append("=== Запуск расчета ===\n");
    runBtn->setEnabled(false);
    
    if (!process) {
        process = new QProcess(this);
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &MainWindow::processFinished);
        connect(process, &QProcess::errorOccurred, this, &MainWindow::processError);
        connect(process, &QProcess::readyReadStandardOutput, [this]() {
            outputText->append(process->readAllStandardOutput());
        });
        connect(process, &QProcess::readyReadStandardError, [this]() {
            outputText->append("<font color='red'>" + process->readAllStandardError() + "</font>");
        });
    }
    
    QString analysisType = analysisTypeCombo->currentText();
    QString stepFile = stepFileEdit->text();
    double clminVal = clminEdit->value();
    double clmaxVal = clmaxEdit->value();
    QString outputDir = outputDirEdit->text();
    
    // Проверка параметров сетки
    if (clminVal <= 0 || clmaxVal <= 0) {
        QMessageBox::warning(this, "Ошибка", "Параметры сетки должны быть положительными числами!");
        runBtn->setEnabled(true);
        return;
    }
    
    if (clminVal > clmaxVal) {
        QMessageBox::warning(this, "Ошибка", 
            QString("clmin (%1) должен быть меньше или равен clmax (%2)!\nАвтоматически исправляю значения...")
            .arg(clminVal).arg(clmaxVal));
        double temp = clminVal;
        clminVal = qMin(clminVal, clmaxVal);
        clmaxVal = qMax(temp, clmaxVal);
        clminEdit->setValue(clminVal);
        clmaxEdit->setValue(clmaxVal);
        outputText->append(QString("Исправлены параметры сетки: clmin=%.6f, clmax=%.6f\n").arg(clminVal).arg(clmaxVal));
    }
    
    QString clmin = QString::number(clminVal, 'g', 6);
    QString clmax = QString::number(clmaxVal, 'g', 6);
    
    // Подготовка запуска скрипта
    QString meshScript = projectRoot + "/HyperMesh/generate_mesh.sh";
    QString bashPath = "/bin/bash";
    
    if (!QFileInfo::exists(meshScript)) {
        outputText->append(QString("Ошибка: скрипт не найден: %1\n").arg(meshScript));
        QMessageBox::critical(this, "Ошибка", QString("Скрипт не найден: %1").arg(meshScript));
        runBtn->setEnabled(true);
        return;
    }
    
    if (analysisType.contains("Модальный")) {
        int numModes = numModesEdit->value();
        double rho = rhoEdit->value();
        double h = hEdit->value();
        
        outputText->append("Тип: Модальный анализ\n");
        outputText->append(QString("STEP файл: %1\n").arg(stepFile));
        outputText->append(QString("Параметры сетки: clmin=%1, clmax=%2\n").arg(clmin, clmax));
        outputText->append(QString("Моды: %1, Плотность: %2, Толщина: %3\n").arg(numModes).arg(rho).arg(h));
        
        outputText->append(QString("Запуск: %1 %2 %3 %4 %5\n").arg(bashPath, meshScript, stepFile, clmin, clmax));
        process->setWorkingDirectory(projectRoot + "/HyperMesh");
        process->start(bashPath, QStringList() << meshScript << stepFile << clmin << clmax);
        
    } else {
        outputText->append("Тип: Статический FEM анализ\n");
        outputText->append(QString("STEP файл: %1\n").arg(stepFile));
        outputText->append(QString("Параметры сетки: clmin=%1, clmax=%2\n").arg(clmin, clmax));
        
        outputText->append(QString("Запуск: %1 %2 %3 %4 %5\n").arg(bashPath, meshScript, stepFile, clmin, clmax));
        process->setWorkingDirectory(projectRoot + "/HyperMesh");
        process->start(bashPath, QStringList() << meshScript << stepFile << clmin << clmax);
    }
}

void MainWindow::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    runBtn->setEnabled(true);
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        outputText->append("\n=== Расчет завершен успешно ===\n");
        
        QString meshScript = projectRoot + "/HyperMesh/generate_mesh.sh";
        if (process->program().contains("bash") && process->arguments().contains(meshScript)) {
            QString analysisType = analysisTypeCombo->currentText();
            QString meshFile = projectRoot + "/build/node.txt";
            
            if (analysisType.contains("Модальный")) {
                int numModes = numModesEdit->value();
                double rho = rhoEdit->value();
                double h = hEdit->value();
                
                outputText->append("Запуск модального анализа...\n");
                
                QProcess *modalProcess = new QProcess(this);
                connect(modalProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        [this, modalProcess](int code, QProcess::ExitStatus status) {
                            if (status == QProcess::NormalExit && code == 0) {
                                saveResultsToDesktop();
                            }
                            modalProcess->deleteLater();
                        });
                connect(modalProcess, &QProcess::readyReadStandardOutput, [this, modalProcess]() {
                    outputText->append(modalProcess->readAllStandardOutput());
                });
                connect(modalProcess, &QProcess::readyReadStandardError, [this, modalProcess]() {
                    outputText->append("<font color='red'>" + modalProcess->readAllStandardError() + "</font>");
                });
                
                QString modalExecutable = projectRoot + "/fem-module/build/modal";
                if (!QFileInfo::exists(modalExecutable)) {
                    outputText->append(QString("Ошибка: Модальный модуль не найден: %1\n").arg(modalExecutable));
                    QMessageBox::warning(this, "Предупреждение", "Модальный модуль не найден. Пожалуйста, соберите его:\ncd fem-module/modal && make");
                    runBtn->setEnabled(true);
                    return;
                }
                
                // Устанавливаем рабочую директорию для модального анализа
                // Результаты будут сохранены в fem-module/build/result.txt
                modalProcess->setWorkingDirectory(projectRoot + "/fem-module/build");
                
                // Преобразуем путь к файлу сетки в абсолютный путь
                QFileInfo meshFileInfo(meshFile);
                QString absoluteMeshFile = meshFileInfo.absoluteFilePath();
                
                outputText->append(QString("Запуск модального модуля: %1 %2 %3 %4 %5\n")
                    .arg(modalExecutable, absoluteMeshFile, QString::number(numModes), QString::number(rho), QString::number(h)));
                modalProcess->start(modalExecutable, 
                    QStringList() << absoluteMeshFile << QString::number(numModes) << QString::number(rho) << QString::number(h));
            } else {
                outputText->append("Запуск FEM анализа...\n");
                
                QProcess *femProcess = new QProcess(this);
                connect(femProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        [this, femProcess](int code, QProcess::ExitStatus status) {
                            if (status == QProcess::NormalExit && code == 0) {
                                saveResultsToDesktop();
                            }
                            femProcess->deleteLater();
                        });
                connect(femProcess, &QProcess::readyReadStandardOutput, [this, femProcess]() {
                    outputText->append(femProcess->readAllStandardOutput());
                });
                connect(femProcess, &QProcess::readyReadStandardError, [this, femProcess]() {
                    outputText->append("<font color='red'>" + femProcess->readAllStandardError() + "</font>");
                });
                
                QString femExecutable = projectRoot + "/fem-module/build/fem";
                if (!QFileInfo::exists(femExecutable)) {
                    outputText->append(QString("Ошибка: FEM модуль не найден: %1\n").arg(femExecutable));
                    QMessageBox::warning(this, "Предупреждение", "FEM модуль не найден. Пожалуйста, соберите его:\ncd fem-module/2D && make");
                    runBtn->setEnabled(true);
                    return;
                }
                
                outputText->append(QString("Запуск FEM модуля: %1 %2\n").arg(femExecutable, meshFile));
                femProcess->start(femExecutable, QStringList() << meshFile);
            }
        } else {
            saveResultsToDesktop();
        }
    } else {
        outputText->append(QString("\n=== Ошибка: код выхода %1 ===\n").arg(exitCode));
        QMessageBox::critical(this, "Ошибка", QString("Расчет завершился с ошибкой. Код: %1").arg(exitCode));
    }
}

void MainWindow::saveResultsToDesktop() {
    QString outputDir = outputDirEdit->text();
    QString analysisType = analysisTypeCombo->currentText();
    QString resultFileName;
    
    // Определяем имя файла в зависимости от типа анализа
    if (analysisType.contains("Модальный")) {
        resultFileName = resultFileEdit->text().isEmpty() ? "Modal_result.txt" : resultFileEdit->text();
    } else {
        resultFileName = resultFileEdit->text().isEmpty() ? "result.txt" : resultFileEdit->text();
    }
    
    QString resultsFile = outputDir + "/" + resultFileName;
    
    // Определяем, откуда читать результаты в зависимости от типа анализа
    QString femResultFile;
    
    if (analysisType.contains("Модальный")) {
        // Для модального анализа читаем из fem-module/build/Modal_result.txt
        femResultFile = projectRoot + "/fem-module/build/Modal_result.txt";
    } else {
        // Для статического анализа читаем из fem-module/2D/build/result.txt
        femResultFile = projectRoot + "/fem-module/2D/build/result.txt";
    }
    
    QString resultContent;
    
    if (QFileInfo::exists(femResultFile)) {
        QFile femFile(femResultFile);
        if (femFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&femFile);
            resultContent = in.readAll();
            femFile.close();
            outputText->append(QString("✓ Файл результатов найден: %1\n").arg(femResultFile));
        } else {
            outputText->append(QString("✗ Не удалось открыть файл результатов: %1\n").arg(femResultFile));
        }
    } else {
        outputText->append(QString("✗ Файл результатов не найден: %1\n").arg(femResultFile));
        // Проверяем альтернативные пути
        QString altPath1 = projectRoot + "/fem-module/build/Modal_result.txt";
        QString altPath2 = projectRoot + "/build/Modal_result.txt";
        if (QFileInfo::exists(altPath1)) {
            outputText->append(QString("Найден альтернативный файл: %1\n").arg(altPath1));
            femResultFile = altPath1;
            QFile femFile(femResultFile);
            if (femFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&femFile);
                resultContent = in.readAll();
                femFile.close();
            }
        } else if (QFileInfo::exists(altPath2)) {
            outputText->append(QString("Найден альтернативный файл: %1\n").arg(altPath2));
            femResultFile = altPath2;
            QFile femFile(femResultFile);
            if (femFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&femFile);
                resultContent = in.readAll();
                femFile.close();
            }
        }
    }
    
    // Если результатов нет, сообщаем об ошибке
    if (resultContent.isEmpty()) {
        outputText->append("\n=== Ошибка: Результаты расчета не найдены ===\n");
        outputText->append(QString("Проверьте, что модальный анализ завершился успешно.\n"));
        outputText->append(QString("Ожидаемый файл: %1\n").arg(femResultFile));
        QMessageBox::warning(this, "Ошибка", 
            QString("Файл результатов не найден:\n%1\n\nПроверьте, что модальный анализ завершился успешно.").arg(femResultFile));
        return;
    }
    
    // Также сохраняем в Desktop
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString desktopResultsFile = desktopPath + "/" + resultFileName;
    
    // Сохраняем только результаты расчета (без параметров и лога)
    QFile file(resultsFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        // Сохраняем только содержимое result.txt (результаты расчета)
        out << resultContent;
        file.close();
        
        outputText->append(QString("\n=== Результаты сохранены: %1 ===\n").arg(resultsFile));
        
        // Копируем также на Desktop
        QFile::copy(resultsFile, desktopResultsFile);
        
        openGmsh();
        
        QMessageBox::information(this, "Успешно", QString("Результаты сохранены в:\n%1\n\nТакже скопировано на Desktop:\n%2\n\nGmsh открыт для визуализации.").arg(resultsFile).arg(desktopResultsFile));
    } else {
        outputText->append(QString("\n=== Ошибка сохранения: %1 ===\n").arg(file.errorString()));
    }
}

void MainWindow::openGmsh() {
    QString mshFile = projectRoot + "/build/HyperMesh.msh";
    QString resultFile = projectRoot + "/fem-module/2D/build/result.txt";
    QString nodeFile = projectRoot + "/build/node.txt";
    QString posFile = projectRoot + "/build/results.pos";
    
    if (!QFileInfo::exists(mshFile)) {
        outputText->append(QString("Предупреждение: Файл сетки не найден: %1\n").arg(mshFile));
        return;
    }
    
    if (QFileInfo::exists(resultFile) && QFileInfo::exists(nodeFile)) {
        QString scriptPath = projectRoot + "/HyperMesh/create_gmsh_results.py";
        if (QFileInfo::exists(scriptPath)) {
            QProcess *createPosProcess = new QProcess(this);
            createPosProcess->setWorkingDirectory(projectRoot);
            createPosProcess->start("python3", QStringList() 
                << scriptPath << resultFile << nodeFile << posFile);
            createPosProcess->waitForFinished(5000);
            if (createPosProcess->exitCode() == 0) {
                outputText->append("Создан файл результатов для визуализации\n");
            }
            createPosProcess->deleteLater();
        }
    }
    
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
    
    if (!found) {
        outputText->append("Предупреждение: Gmsh не найден. Установите Gmsh:\n  macOS: brew install gmsh\n");
        return;
    }
    
    QStringList filesToLoad;
    filesToLoad << mshFile;
    if (QFileInfo::exists(posFile)) {
        filesToLoad << posFile;
    }
    
    outputText->append(QString("Открытие Gmsh для визуализации: %1\n").arg(mshFile));
    if (QFileInfo::exists(posFile)) {
        outputText->append(QString("Загрузка результатов: %1\n").arg(posFile));
    }
    
    QProcess *gmshProcess = new QProcess(this);
    gmshProcess->startDetached(gmshPath, filesToLoad);
    
    if (!gmshProcess->waitForStarted(2000)) {
        outputText->append("Ошибка: Не удалось запустить Gmsh\n");
        gmshProcess->deleteLater();
    } else {
        outputText->append("Gmsh запущен для визуализации сетки и результатов\n");
    }
}

void MainWindow::processError(QProcess::ProcessError error) {
    runBtn->setEnabled(true);
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Не удалось запустить процесс";
            break;
        case QProcess::Crashed:
            errorMsg = "Процесс аварийно завершился";
            break;
        default:
            errorMsg = "Ошибка выполнения процесса";
    }
    outputText->append(QString("\n=== Ошибка: %1 ===\n").arg(errorMsg));
    QMessageBox::critical(this, "Ошибка", errorMsg);
}

