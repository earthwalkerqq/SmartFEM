#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), process(nullptr) {
    // Определяем корень проекта
    // Если запускаем из build/SmartFEM_GUI.app, то путь к проекту - на 2 уровня выше
    // Если запускаем из preprocess/gui, то тоже на 2 уровня выше
    QString appPath = QApplication::applicationFilePath();
    QFileInfo appInfo(appPath);
    QDir appDir = appInfo.absoluteDir();
    
    // Для .app bundle: appDir = SmartFEM_GUI.app/Contents/MacOS
    // Для обычного executable: appDir = build или preprocess/gui
    if (appPath.contains(".app")) {
        // Это .app bundle, идем на 3 уровня вверх
        appDir.cdUp(); // Contents
        appDir.cdUp(); // SmartFEM_GUI.app
        appDir.cdUp(); // build
    }
    
    // Ищем корень проекта (где есть папка HyperMesh)
    // Важно: не выходим за пределы Desktop, чтобы не создать build папку не там
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    while (!appDir.isRoot() && !QDir(appDir.absolutePath() + "/HyperMesh").exists()) {
        // Останавливаемся, если дошли до Desktop или выше
        if (appDir.absolutePath() == desktopPath || appDir.absolutePath().count("/") <= 2) {
            break;
        }
        appDir.cdUp();
    }
    
    projectRoot = appDir.absolutePath();
    
    // Если не нашли, используем абсолютный путь по умолчанию
    if (projectRoot.isEmpty() || !QDir(projectRoot + "/HyperMesh").exists()) {
        projectRoot = "/Users/matvej/Desktop/SmartFEM";
    }
    
    // Финальная проверка: убеждаемся, что projectRoot не на Desktop (кроме самого SmartFEM)
    if (projectRoot == desktopPath || projectRoot.contains(desktopPath + "/build")) {
        projectRoot = "/Users/matvej/Desktop/SmartFEM";
    }
    
    setupUI();
    
    // Установить значения по умолчанию
    stepFileEdit->setText(projectRoot + "/materials/data-sample/52.stp");
    outputDirEdit->setText(projectRoot + "/build");
    
    // Добавить информацию о project root в вывод (после создания outputText)
    outputText->append(QString("Project root: %1\n").arg(projectRoot));
    
    // Установить значения по умолчанию
    numElementsEdit->setText("500");
    clminEdit->setText("0.8");
    clmaxEdit->setText("6.0");
    eEdit->setText("2.1e5");
    nuEdit->setText("0.3");
    rhoEdit->setText("7850");
    hEdit->setText("1.0");
    numModesEdit->setText("5");
}

MainWindow::~MainWindow() {
    if (process) {
        process->kill();
        process->deleteLater();
    }
}

void MainWindow::setupUI() {
    setWindowTitle("SmartFEM - Параметры расчета");
    setMinimumSize(800, 600);
    
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Input file group
    QGroupBox *inputGroup = new QGroupBox("Входной файл", this);
    QFormLayout *inputLayout = new QFormLayout(inputGroup);
    
    stepFileEdit = new QLineEdit(this);
    browseStepBtn = new QPushButton("Обзор...", this);
    QHBoxLayout *stepLayout = new QHBoxLayout();
    stepLayout->addWidget(stepFileEdit);
    stepLayout->addWidget(browseStepBtn);
    inputLayout->addRow("STEP файл:", stepLayout);
    connect(browseStepBtn, &QPushButton::clicked, this, &MainWindow::browseStepFile);
    
    outputDirEdit = new QLineEdit(this);
    browseOutputBtn = new QPushButton("Обзор...", this);
    QHBoxLayout *outputDirLayout = new QHBoxLayout();
    outputDirLayout->addWidget(outputDirEdit);
    outputDirLayout->addWidget(browseOutputBtn);
    inputLayout->addRow("Выходная директория:", outputDirLayout);
    connect(browseOutputBtn, &QPushButton::clicked, this, &MainWindow::browseOutputDir);
    
    mainLayout->addWidget(inputGroup);
    
    // Mesh parameters
    QGroupBox *meshGroup = new QGroupBox("Параметры сетки", this);
    QFormLayout *meshLayout = new QFormLayout(meshGroup);
    
    // Количество элементов
    numElementsEdit = new QLineEdit(this);
    numElementsEdit->setPlaceholderText("500");
    numElementsEdit->setToolTip("Желаемое количество элементов в сетке");
    calcMeshParamsBtn = new QPushButton("Рассчитать параметры", this);
    QHBoxLayout *numElementsLayout = new QHBoxLayout();
    numElementsLayout->addWidget(numElementsEdit);
    numElementsLayout->addWidget(calcMeshParamsBtn);
    meshLayout->addRow("Желаемое количество элементов:", numElementsLayout);
    connect(calcMeshParamsBtn, &QPushButton::clicked, this, &MainWindow::calculateMeshParams);
    
    clminEdit = new QLineEdit(this);
    clminEdit->setPlaceholderText("0.1");
    clminEdit->setToolTip("Минимальный размер элемента (автоматически рассчитывается)");
    meshLayout->addRow("Минимальный размер элемента (clmin):", clminEdit);
    clmaxEdit = new QLineEdit(this);
    clmaxEdit->setPlaceholderText("1.0");
    clmaxEdit->setToolTip("Максимальный размер элемента (автоматически рассчитывается)");
    meshLayout->addRow("Максимальный размер элемента (clmax):", clmaxEdit);
    showMeshGuiBtn = new QPushButton("Показать сетку в Gmsh", this);
    meshLayout->addRow(showMeshGuiBtn);
    mainLayout->addWidget(meshGroup);
    
    // Material parameters
    QGroupBox *materialGroup = new QGroupBox("Параметры материала", this);
    QFormLayout *materialLayout = new QFormLayout(materialGroup);
    eEdit = new QLineEdit(this);
    eEdit->setPlaceholderText("2.1e5 (МПа)");
    materialLayout->addRow("Модуль упругости E (МПа):", eEdit);
    nuEdit = new QLineEdit(this);
    nuEdit->setPlaceholderText("0.3");
    materialLayout->addRow("Коэффициент Пуассона ν:", nuEdit);
    rhoEdit = new QLineEdit(this);
    rhoEdit->setPlaceholderText("7850 (кг/м³)");
    materialLayout->addRow("Плотность ρ (кг/м³):", rhoEdit);
    hEdit = new QLineEdit(this);
    hEdit->setPlaceholderText("1.0 (м)");
    materialLayout->addRow("Толщина h (м):", hEdit);
    mainLayout->addWidget(materialGroup);
    
    // Analysis parameters
    QGroupBox *analysisGroup = new QGroupBox("Тип анализа", this);
    QFormLayout *analysisLayout = new QFormLayout(analysisGroup);
    analysisTypeCombo = new QComboBox(this);
    analysisTypeCombo->addItem("Статический FEM анализ");
    analysisTypeCombo->addItem("Модальный анализ (колебания)");
    analysisLayout->addRow("Тип анализа:", analysisTypeCombo);
    numModesEdit = new QLineEdit(this);
    numModesEdit->setPlaceholderText("5");
    numModesEdit->setEnabled(false);
    analysisLayout->addRow("Количество мод:", numModesEdit);
    connect(analysisTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                numModesEdit->setEnabled(index == 1);
            });
    mainLayout->addWidget(analysisGroup);
    
    // Output
    QGroupBox *outputGroup = new QGroupBox("Вывод", this);
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);
    outputText = new QTextEdit(this);
    outputText->setReadOnly(true);
    outputText->setFont(QFont("Courier", 10));
    outputLayout->addWidget(outputText);
    mainLayout->addWidget(outputGroup);
    
    // Run button
    runBtn = new QPushButton("Запустить расчет", this);
    runBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-size: 14px; padding: 10px; }");
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::runAnalysis);
    mainLayout->addWidget(runBtn);
}

void MainWindow::setupMeshGroup() {
    // Пустая функция - все создается в setupUI()
}

void MainWindow::setupMaterialGroup() {
    // Пустая функция - все создается в setupUI()
}

void MainWindow::setupAnalysisGroup() {
    // Пустая функция - все создается в setupUI()
}

void MainWindow::setupOutputGroup() {
    // Пустая функция - все создается в setupUI()
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
    bool ok;
    int desiredElements = numElementsEdit->text().toInt(&ok);
    
    if (!ok || desiredElements <= 0) {
        QMessageBox::warning(this, "Ошибка", "Введите корректное количество элементов (положительное число)");
        return;
    }
    
    // Улучшенная эмпирическая формула на основе реальных данных
    // Для данной геометрии (примерно 23.5 x 20.0 = 470 единиц площади):
    // clmax=1.0 -> ~29622 элементов
    // clmax=0.97 -> ~32408 элементов
    // 
    // Эмпирическая зависимость: elements ≈ area / (clmax^2) * factor
    // где factor ≈ 63 (получено из реальных данных)
    
    double estimatedArea = 470.0; // Примерная площадь модели
    double factor = 82.8; // Эмпирический коэффициент (скорректирован на основе реальных данных)
    
    // Расчет clmax на основе желаемого количества элементов
    // elements = area / (clmax^2) * factor
    // clmax^2 = area * factor / elements
    double clmax = sqrt(estimatedArea * factor / desiredElements);
    
    // clmin обычно составляет 0.1-0.2 от clmax для хорошего качества сетки
    // Но для мелких сеток используем меньший коэффициент
    double clminRatio = (desiredElements < 1000) ? 0.2 : 0.15;
    double clmin = clmax * clminRatio;
    
    // Ограничения для разумных значений
    if (clmin < 0.05) clmin = 0.05;  // Минимальный размер элемента
    if (clmax < 0.1) clmax = 0.1;    // Минимальный максимальный размер
    if (clmax > 50.0) clmax = 50.0;  // Максимальный размер
    
    // Округляем до 3 знаков после запятой для точности
    clminEdit->setText(QString::number(clmin, 'f', 3));
    clmaxEdit->setText(QString::number(clmax, 'f', 3));
    
    // Оценка фактического количества элементов (для информации)
    double estimatedElements = estimatedArea * factor / (clmax * clmax);
    
    outputText->append(QString("Рассчитаны параметры сетки:\n")
                      .append(QString("  Желаемое количество элементов: %1\n").arg(desiredElements))
                      .append(QString("  Ожидаемое количество элементов: ~%1\n").arg((int)estimatedElements))
                      .append(QString("  clmin=%.3f, clmax=%.3f\n").arg(clmin).arg(clmax)));
}

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
    QString clminStr = clminEdit->text();
    QString clmaxStr = clmaxEdit->text();
    QString outputDir = outputDirEdit->text();
    
    // Проверка и валидация параметров сетки
    bool ok1, ok2;
    double clminVal = clminStr.toDouble(&ok1);
    double clmaxVal = clmaxStr.toDouble(&ok2);
    
    if (!ok1 || !ok2 || clminVal <= 0 || clmaxVal <= 0) {
        QMessageBox::warning(this, "Ошибка", "Параметры сетки должны быть положительными числами!");
        runBtn->setEnabled(true);
        return;
    }
    
    if (clminVal > clmaxVal) {
        QMessageBox::warning(this, "Ошибка", 
            QString("clmin (%1) должен быть меньше или равен clmax (%2)!\nАвтоматически исправляю значения...")
            .arg(clminVal).arg(clmaxVal));
        // Автоматически исправляем: меняем местами
        double temp = clminVal;
        clminVal = qMin(clminVal, clmaxVal);
        clmaxVal = qMax(temp, clmaxVal);
        clminEdit->setText(QString::number(clminVal, 'g', 6));
        clmaxEdit->setText(QString::number(clmaxVal, 'g', 6));
        outputText->append(QString("Исправлены параметры сетки: clmin=%.6f, clmax=%.6f\n").arg(clminVal).arg(clmaxVal));
    }
    
    QString clmin = QString::number(clminVal, 'g', 6);
    QString clmax = QString::number(clmaxVal, 'g', 6);
    
    // Подготовка запуска скрипта
    QString meshScript = projectRoot + "/HyperMesh/generate_mesh.sh";
    QString bashPath = "/bin/bash";  // Абсолютный путь к bash
    
    // Проверяем существование файлов
    if (!QFileInfo::exists(meshScript)) {
        outputText->append(QString("Ошибка: скрипт не найден: %1\n").arg(meshScript));
        QMessageBox::critical(this, "Ошибка", QString("Скрипт не найден: %1").arg(meshScript));
        runBtn->setEnabled(true);
        return;
    }
    
    if (!QFileInfo::exists(bashPath)) {
        bashPath = "/usr/bin/bash";  // Альтернативный путь
        if (!QFileInfo::exists(bashPath)) {
            outputText->append("Ошибка: bash не найден\n");
            QMessageBox::critical(this, "Ошибка", "bash не найден");
            runBtn->setEnabled(true);
            return;
        }
    }
    
    if (analysisType.contains("Модальный")) {
        // Модальный анализ
        QString numModes = numModesEdit->text();
        QString rho = rhoEdit->text();
        QString h = hEdit->text();
        
        outputText->append("Тип: Модальный анализ\n");
        outputText->append(QString("STEP файл: %1\n").arg(stepFile));
        outputText->append(QString("Параметры сетки: clmin=%1, clmax=%2\n").arg(clmin, clmax));
        outputText->append(QString("Моды: %1, Плотность: %2, Толщина: %3\n").arg(numModes, rho, h));
        
        // Сначала генерируем сетку
        outputText->append(QString("Запуск: %1 %2 %3 %4 %5\n").arg(bashPath, meshScript, stepFile, clmin, clmax));
        process->setWorkingDirectory(projectRoot + "/HyperMesh");
        process->start(bashPath, QStringList() << meshScript << stepFile << clmin << clmax);
        
    } else {
        // Статический FEM анализ
        outputText->append("Тип: Статический FEM анализ\n");
        outputText->append(QString("STEP файл: %1\n").arg(stepFile));
        outputText->append(QString("Параметры сетки: clmin=%1, clmax=%2\n").arg(clmin, clmax));
        
        // Сначала генерируем сетку
        outputText->append(QString("Запуск: %1 %2 %3 %4 %5\n").arg(bashPath, meshScript, stepFile, clmin, clmax));
        process->setWorkingDirectory(projectRoot + "/HyperMesh");
        process->start(bashPath, QStringList() << meshScript << stepFile << clmin << clmax);
    }
}

void MainWindow::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    runBtn->setEnabled(true);
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        outputText->append("\n=== Расчет завершен успешно ===\n");
        
        // Проверяем, что это был скрипт генерации сетки, и запускаем FEM
        QString meshScript = projectRoot + "/HyperMesh/generate_mesh.sh";
        if (process->program().contains("bash") && process->arguments().contains(meshScript)) {
            QString analysisType = analysisTypeCombo->currentText();
            QString meshFile = projectRoot + "/build/node.txt";
            
            if (analysisType.contains("Модальный")) {
                // Запускаем модальный анализ
                QString numModes = numModesEdit->text();
                QString rho = rhoEdit->text();
                QString h = hEdit->text();
                
                outputText->append("Запуск модального анализа...\n");
                process->setWorkingDirectory(projectRoot + "/fem-module/modal");
                
                // Запускаем процесс для модального анализа
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
                
                // Проверяем существование модального модуля
                QString modalExecutable = projectRoot + "/fem-module/build/modal";
                if (!QFileInfo::exists(modalExecutable)) {
                    outputText->append(QString("Ошибка: Модальный модуль не найден: %1\n").arg(modalExecutable));
                    outputText->append("Попытка сборки модального модуля...\n");
                    QMessageBox::warning(this, "Предупреждение", "Модальный модуль не найден. Пожалуйста, соберите его:\ncd fem-module/modal && make");
                    runBtn->setEnabled(true);
                    return;
                }
                
                outputText->append(QString("Запуск модального модуля: %1 %2 %3 %4 %5\n").arg(modalExecutable, meshFile, numModes, rho, h));
                modalProcess->start(modalExecutable, 
                    QStringList() << meshFile << numModes << rho << h);
            } else {
                // Запускаем FEM анализ
                outputText->append("Запуск FEM анализа...\n");
                process->setWorkingDirectory(projectRoot + "/fem-module/2D");
                
                // Запускаем процесс для FEM анализа
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
                
                // Проверяем существование FEM модуля
                QString femExecutable = projectRoot + "/fem-module/build/fem";
                if (!QFileInfo::exists(femExecutable)) {
                    outputText->append(QString("Ошибка: FEM модуль не найден: %1\n").arg(femExecutable));
                    outputText->append("Попытка сборки FEM модуля...\n");
                    QMessageBox::warning(this, "Предупреждение", "FEM модуль не найден. Пожалуйста, соберите его:\ncd fem-module/2D && make");
                    runBtn->setEnabled(true);
                    return;
                }
                
                outputText->append(QString("Запуск FEM модуля: %1 %2\n").arg(femExecutable, meshFile));
                femProcess->start(femExecutable, QStringList() << meshFile);
            }
        } else {
            // Это был FEM или модальный анализ, сохраняем результаты
            saveResultsToDesktop();
        }
    } else {
        outputText->append(QString("\n=== Ошибка: код выхода %1 ===\n").arg(exitCode));
        QMessageBox::critical(this, "Ошибка", QString("Расчет завершился с ошибкой. Код: %1").arg(exitCode));
    }
}

void MainWindow::saveResultsToDesktop() {
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString resultsFile = desktopPath + "/Results.txt";
    
    QFile file(resultsFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "=== SmartFEM Results ===\n\n";
        out << "Date: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
        out << "Parameters:\n";
        out << "  STEP file: " << stepFileEdit->text() << "\n";
        out << "  Mesh: clmin=" << clminEdit->text() << ", clmax=" << clmaxEdit->text() << "\n";
        out << "  Material: E=" << eEdit->text() << ", ν=" << nuEdit->text() 
            << ", ρ=" << rhoEdit->text() << ", h=" << hEdit->text() << "\n";
        out << "  Analysis type: " << analysisTypeCombo->currentText() << "\n";
        if (analysisTypeCombo->currentText().contains("Модальный")) {
            out << "  Number of modes: " << numModesEdit->text() << "\n";
        }
        out << "\n";
        out << "Output:\n";
        out << outputText->toPlainText();
        file.close();
        
        outputText->append(QString("\n=== Результаты сохранены: %1 ===\n").arg(resultsFile));
        
        // Открываем Gmsh для визуализации сетки
        openGmsh();
        
        QMessageBox::information(this, "Успешно", QString("Результаты сохранены в:\n%1\n\nGmsh открыт для визуализации.").arg(resultsFile));
    } else {
        outputText->append(QString("\n=== Ошибка сохранения: %1 ===\n").arg(file.errorString()));
    }
}

void MainWindow::openGmsh() {
    QString mshFile = projectRoot + "/build/HyperMesh.msh";
    QString resultFile = projectRoot + "/fem-module/2D/build/result.txt";
    QString nodeFile = projectRoot + "/build/node.txt";
    QString posFile = projectRoot + "/build/results.pos";
    
    // Проверяем существование файла сетки
    if (!QFileInfo::exists(mshFile)) {
        outputText->append(QString("Предупреждение: Файл сетки не найден: %1\n").arg(mshFile));
        return;
    }
    
    // Создаем файл результатов для Gmsh, если есть результаты расчета
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
    
    // Ищем gmsh в PATH
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
    
    // Проверяем через which
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
    
    // Формируем список файлов для загрузки в Gmsh
    QStringList filesToLoad;
    filesToLoad << mshFile;
    if (QFileInfo::exists(posFile)) {
        filesToLoad << posFile;
    }
    
    outputText->append(QString("Открытие Gmsh для визуализации: %1\n").arg(mshFile));
    if (QFileInfo::exists(posFile)) {
        outputText->append(QString("Загрузка результатов: %1\n").arg(posFile));
    }
    
    // Запускаем Gmsh в фоновом режиме
    QProcess *gmshProcess = new QProcess(this);
    gmshProcess->startDetached(gmshPath, filesToLoad);
    
    if (!gmshProcess->waitForStarted(2000)) {
        outputText->append("Ошибка: Не удалось запустить Gmsh\n");
        gmshProcess->deleteLater();
    } else {
        outputText->append("Gmsh запущен для визуализации сетки и результатов\n");
        // Не удаляем процесс, так как он запущен detached
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

