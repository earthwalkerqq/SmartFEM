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
                
                outputText->append(QString("Запуск модального модуля: %1 %2 %3 %4 %5\n")
                    .arg(modalExecutable, meshFile, QString::number(numModes), QString::number(rho), QString::number(h)));
                modalProcess->start(modalExecutable, 
                    QStringList() << meshFile << QString::number(numModes) << QString::number(rho) << QString::number(h));
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
    QString resultFileName = resultFileEdit->text().isEmpty() ? "result.txt" : resultFileEdit->text();
    QString resultsFile = outputDir + "/" + resultFileName;
    
    // Также сохраняем в Desktop
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString desktopResultsFile = desktopPath + "/Results.txt";
    
    QFile file(resultsFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "=== SmartFEM Results ===\n\n";
        out << "Date: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
        out << "Parameters:\n";
        out << "  STEP file: " << stepFileEdit->text() << "\n";
        out << "  Mesh: clmin=" << clminEdit->value() << ", clmax=" << clmaxEdit->value() << "\n";
        out << "  Material: E=" << eEdit->value() << ", ν=" << nuEdit->value() 
            << ", ρ=" << rhoEdit->value() << ", h=" << hEdit->value() << "\n";
        out << "  Analysis type: " << analysisTypeCombo->currentText() << "\n";
        if (analysisTypeCombo->currentText().contains("Модальный")) {
            out << "  Number of modes: " << numModesEdit->value() << "\n";
        }
        out << "\n";
        out << "Output:\n";
        out << outputText->toPlainText();
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

