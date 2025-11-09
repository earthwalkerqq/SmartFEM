#include "meshgenerator.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#include <QMap>
#include <QSet>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <stdexcept>

// Gmsh C API должен быть включен с extern "C"
extern "C" {
#include <gmshc.h>
}

#define MESH_ORDER 1
#define DIMENSION 2

MeshGenerator::MeshGenerator() : nodeCount(0), elementCount(0) {
}

MeshGenerator::~MeshGenerator() {
    // Gmsh должен быть финализирован после каждого использования
}

bool MeshGenerator::generateMesh(const QString &stepFile, 
                                 const QString &outputMeshFile,
                                 const QString &outputNodeFile,
                                 const QString &outputAdjFile,
                                 double clmin, 
                                 double clmax,
                                 QString &errorMsg) {
    nodeCount = 0;
    elementCount = 0;
    lastError.clear();
    
    // Проверка существования входного файла
    if (!QFileInfo::exists(stepFile)) {
        errorMsg = QString("STEP файл не найден: %1").arg(stepFile);
        lastError = errorMsg;
        return false;
    }
    
    // Проверка параметров
    if (clmin <= 0 || clmax <= 0) {
        errorMsg = QString("Некорректные параметры сетки: clmin=%1, clmax=%2").arg(clmin).arg(clmax);
        lastError = errorMsg;
        return false;
    }
    
    if (clmin > clmax) {
        errorMsg = QString("clmin (%1) должен быть меньше или равен clmax (%2)").arg(clmin).arg(clmax);
        lastError = errorMsg;
        return false;
    }
    
    int error = 0;
    bool gmshInitialized = false;
    
    try {
        qDebug() << "Step 1: Initializing Gmsh...";
        // Шаг 1: Инициализация Gmsh
        gmshInitialize(1, NULL, 1, 0, &error);
        if (error) {
            errorMsg = QString("Ошибка инициализации Gmsh (код: %1). Проверьте, что библиотека Gmsh установлена правильно.").arg(error);
            lastError = errorMsg;
            qDebug() << "Gmsh initialization failed with error:" << error;
            return false;
        }
        gmshInitialized = true;
        qDebug() << "Gmsh initialized successfully";
        
        // Шаг 2: Загрузка STEP файла
        qDebug() << "Step 2: Loading STEP file:" << stepFile;
        QByteArray stepFileBytes = stepFile.toUtf8(); // Используем UTF-8 для совместимости
        const char* stepFilePath = stepFileBytes.constData();
        
        gmshMerge(stepFilePath, &error);
        if (error) {
            errorMsg = QString("Ошибка загрузки STEP файла %1 (код: %2). Убедитесь, что файл существует и является валидным STEP файлом.").arg(stepFile).arg(error);
            lastError = errorMsg;
            qDebug() << "Failed to merge STEP file:" << stepFile << "Error:" << error;
            gmshFinalize(&error);
            return false;
        }
        qDebug() << "STEP file loaded successfully";
        
        // Шаг 3: Синхронизация модели
        qDebug() << "Step 3: Synchronizing model...";
        gmshModelOccSynchronize(&error);
        if (error) {
            errorMsg = QString("Ошибка синхронизации модели (код: %1). Возможно, проблема с геометрией STEP файла.").arg(error);
            lastError = errorMsg;
            qDebug() << "Failed to synchronize model. Error:" << error;
            gmshFinalize(&error);
            return false;
        }
        qDebug() << "Model synchronized successfully";
        
        // Шаг 4: Установка параметров сетки
        qDebug() << "Step 4: Setting mesh parameters (clmin=" << clmin << ", clmax=" << clmax << ")";
        
        // Устанавливаем формат MSH 2.2 для совместимости с существующим кодом
        gmshOptionSetNumber("Mesh.MshFileVersion", 2.2, &error);
        if (error) {
            qDebug() << "Warning: Failed to set MSH file version to 2.2, using default";
            error = 0; // Не критично, продолжаем
        }
        
        gmshOptionSetNumber("Mesh.CharacteristicLengthMax", clmax, &error);
        if (error) {
            errorMsg = QString("Ошибка установки clmax (код: %1)").arg(error);
            lastError = errorMsg;
            qDebug() << "Failed to set clmax. Error:" << error;
            gmshFinalize(&error);
            return false;
        }
        
        gmshOptionSetNumber("Mesh.CharacteristicLengthMin", clmin, &error);
        if (error) {
            errorMsg = QString("Ошибка установки clmin (код: %1)").arg(error);
            lastError = errorMsg;
            qDebug() << "Failed to set clmin. Error:" << error;
            gmshFinalize(&error);
            return false;
        }
        qDebug() << "Mesh parameters set successfully";
        
        // Шаг 5: Установка порядка элементов
        qDebug() << "Step 5: Setting mesh order to" << MESH_ORDER;
        gmshModelMeshSetOrder(MESH_ORDER, &error);
        if (error) {
            errorMsg = QString("Ошибка установки порядка элементов (код: %1)").arg(error);
            lastError = errorMsg;
            qDebug() << "Failed to set mesh order. Error:" << error;
            gmshFinalize(&error);
            return false;
        }
        
        // Шаг 6: Генерация сетки
        qDebug() << "Step 6: Generating 2D mesh...";
        // Генерируем 2D сетку (треугольные элементы)
        gmshModelMeshGenerate(2, &error); // Используем 2 для 2D сетки
        if (error) {
            errorMsg = QString("Ошибка генерации сетки (код: %1). Возможно, модель не является 2D или произошла ошибка в Gmsh. Проверьте геометрию модели.").arg(error);
            lastError = errorMsg;
            qDebug() << "Mesh generation failed. Error:" << error;
            gmshFinalize(&error);
            return false;
        }
        qDebug() << "Mesh generated successfully";
        
        // Шаг 7: Сохранение .msh файла
        qDebug() << "Step 7: Writing MSH file to" << outputMeshFile;
        // Создаем директорию для выходного файла если её нет
        QFileInfo mshFileInfo(outputMeshFile);
        QDir mshDir = mshFileInfo.absoluteDir();
        if (!mshDir.exists()) {
            qDebug() << "Creating directory:" << mshDir.absolutePath();
            if (!mshDir.mkpath(".")) {
                errorMsg = QString("Не удалось создать директорию для .msh файла: %1").arg(mshDir.absolutePath());
                lastError = errorMsg;
                qDebug() << "Failed to create directory:" << mshDir.absolutePath();
                gmshFinalize(&error);
                return false;
            }
        }
        
        QByteArray mshFileBytes = outputMeshFile.toUtf8();
        const char* mshFilePath = mshFileBytes.constData();
        
        gmshWrite(mshFilePath, &error);
        if (error) {
            errorMsg = QString("Ошибка сохранения .msh файла (код: %1). Проверьте права доступа к директории.").arg(error);
            lastError = errorMsg;
            qDebug() << "Failed to write MSH file. Error:" << error;
            gmshFinalize(&error);
            return false;
        }
        qDebug() << "MSH file written successfully";
        
        // Финализация Gmsh перед конвертацией
        qDebug() << "Finalizing Gmsh...";
        gmshFinalize(&error);
        gmshInitialized = false;
        qDebug() << "Gmsh finalized";
        
        // Проверяем, что .msh файл был создан
        if (!QFileInfo::exists(outputMeshFile)) {
            errorMsg = QString("Файл .msh не был создан: %1. Проверьте права доступа.").arg(outputMeshFile);
            lastError = errorMsg;
            qDebug() << "MSH file was not created:" << outputMeshFile;
            return false;
        }
        
        qDebug() << "MSH file exists, size:" << QFileInfo(outputMeshFile).size() << "bytes";
        
        // Шаг 8: Конвертация в node.txt и adjacency.txt
        qDebug() << "Step 8: Converting MSH to node.txt...";
        if (!convertMshToNodeTxt(outputMeshFile, outputNodeFile, errorMsg)) {
            qDebug() << "Failed to convert MSH to node.txt:" << errorMsg;
            return false;
        }
        qDebug() << "Node.txt created successfully. Nodes:" << nodeCount << ", Elements:" << elementCount;
        
        qDebug() << "Step 9: Generating adjacency file...";
        if (!generateAdjacencyFile(outputMeshFile, outputAdjFile, errorMsg)) {
            // adjacency.txt не критичен, только предупреждаем
            qDebug() << "Warning: Failed to generate adjacency file:" << errorMsg;
        } else {
            qDebug() << "Adjacency file created successfully";
        }
        
        qDebug() << "Mesh generation completed successfully";
        return true;
        
    } catch (const std::exception& e) {
        errorMsg = QString("Исключение при генерации сетки: %1").arg(e.what());
        lastError = errorMsg;
        if (gmshInitialized) {
            int finalizeError = 0;
            gmshFinalize(&finalizeError);
        }
        return false;
    } catch (...) {
        errorMsg = "Неизвестное исключение при генерации сетки";
        lastError = errorMsg;
        if (gmshInitialized) {
            int finalizeError = 0;
            gmshFinalize(&finalizeError);
        }
        return false;
    }
}

bool MeshGenerator::convertMshToNodeTxt(const QString &mshFile, const QString &nodeFile, QString &errorMsg) {
    // Проверка существования .msh файла
    if (!QFileInfo::exists(mshFile)) {
        errorMsg = QString("Файл .msh не найден: %1").arg(mshFile);
        lastError = errorMsg;
        return false;
    }
    
    QFile file(mshFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QString("Не удалось открыть файл %1: %2").arg(mshFile).arg(file.errorString());
        lastError = errorMsg;
        return false;
    }
    
    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();
    
    // Поиск секции узлов (поддерживаем формат MSH 2.2 и 4.x)
    QMap<int, QVector3D> nodes; // node_id -> (x, y, z)
    int i = 0;
    bool foundNodesSection = false;
    
    while (i < lines.size()) {
        QString line = lines[i].trimmed();
        if (line == "$Nodes" || line.startsWith("$Nodes")) {
            foundNodesSection = true;
            i++;
            if (i < lines.size()) {
                QStringList headerParts = lines[i].split(" ", Qt::SkipEmptyParts);
                if (headerParts.size() >= 1) {
                    int numNodes = headerParts[0].toInt();
                    i++;
                    
                    // В формате MSH 4.x может быть дополнительная информация
                    // Проверяем, является ли следующая строка началом данных узлов
                    for (int j = 0; j < numNodes && i < lines.size(); j++) {
                        QStringList parts = lines[i].split(" ", Qt::SkipEmptyParts);
                        if (parts.size() >= 4) {
                            bool ok;
                            int nodeId = parts[0].toInt(&ok);
                            if (!ok) {
                                i++;
                                continue;
                            }
                            double x = parts[1].toDouble(&ok);
                            if (!ok) {
                                i++;
                                continue;
                            }
                            double y = parts[2].toDouble(&ok);
                            if (!ok) {
                                i++;
                                continue;
                            }
                            double z = parts[3].toDouble(&ok);
                            if (!ok) {
                                i++;
                                continue;
                            }
                            nodes[nodeId] = QVector3D(x, y, z);
                        }
                        i++;
                    }
                }
            }
            break;
        }
        i++;
    }
    
    if (!foundNodesSection || nodes.isEmpty()) {
        errorMsg = QString("Не найдена секция узлов в файле %1 или она пуста").arg(mshFile);
        lastError = errorMsg;
        return false;
    }
    
    nodeCount = nodes.size();
    
    // Поиск секции элементов (треугольники)
    QList<QList<int>> elements;
    i = 0;
    bool foundElementsSection = false;
    
    while (i < lines.size()) {
        QString line = lines[i].trimmed();
        if (line.contains("$Elements")) {
            foundElementsSection = true;
            i++;
            if (i < lines.size()) {
                QStringList headerParts = lines[i].split(" ", Qt::SkipEmptyParts);
                if (headerParts.size() >= 1) {
                    int numElems = headerParts[0].toInt();
                    i++;
                    
                    for (int j = 0; j < numElems && i < lines.size(); j++) {
                        QStringList parts = lines[i].split(" ", Qt::SkipEmptyParts);
                        if (parts.size() >= 4) {
                            bool ok;
                            // Пропускаем elemId (не используется)
                            parts[0].toInt(&ok);
                            if (!ok) {
                                i++;
                                continue;
                            }
                            int elemType = parts[1].toInt(&ok);
                            if (!ok) {
                                i++;
                                continue;
                            }
                            
                            if (elemType == 2) { // Треугольник (MSH 2.2 и 4.x)
                                int numTags = parts[2].toInt(&ok);
                                if (!ok || numTags < 0) {
                                    i++;
                                    continue;
                                }
                                int nodeStartIdx = 3 + numTags;
                                if (parts.size() >= nodeStartIdx + 3) {
                                    QList<int> elemNodes;
                                    bool nodesOk = true;
                                    for (int k = 0; k < 3; k++) {
                                        int nodeId = parts[nodeStartIdx + k].toInt(&ok);
                                        if (!ok) {
                                            nodesOk = false;
                                            break;
                                        }
                                        elemNodes.append(nodeId);
                                    }
                                    if (nodesOk && elemNodes.size() == 3) {
                                        elements.append(elemNodes);
                                    }
                                }
                            }
                        }
                        i++;
                    }
                }
            }
            break;
        }
        i++;
    }
    
    if (!foundElementsSection) {
        qDebug() << "Warning: Elements section not found in MSH file, but continuing...";
    }
    
    elementCount = elements.size();
    
    // Создание маппинга: gmsh_node_id -> sequential_index (1-based)
    QList<int> sortedNodeIds = nodes.keys();
    std::sort(sortedNodeIds.begin(), sortedNodeIds.end());
    QMap<int, int> nodeIdToIndex;
    for (int idx = 0; idx < sortedNodeIds.size(); idx++) {
        nodeIdToIndex[sortedNodeIds[idx]] = idx + 1;
    }
    
    // Создаем директорию для node.txt если её нет
    QFileInfo nodeFileInfo(nodeFile);
    QDir nodeDir = nodeFileInfo.absoluteDir();
    if (!nodeDir.exists()) {
        if (!nodeDir.mkpath(".")) {
            errorMsg = QString("Не удалось создать директорию для node.txt: %1").arg(nodeDir.absolutePath());
            lastError = errorMsg;
            return false;
        }
    }
    
    // Запись в node.txt
    QFile outFile(nodeFile);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QString("Не удалось создать файл %1: %2").arg(nodeFile).arg(outFile.errorString());
        lastError = errorMsg;
        return false;
    }
    
    QTextStream out(&outFile);
    out << nodeCount << "\n";
    
    // Записываем узлы в порядке их ID
    for (int nodeId : sortedNodeIds) {
        QVector3D coords = nodes[nodeId];
        out << QString::number(coords.x(), 'f', 15) << " "
            << QString::number(coords.y(), 'f', 15) << " "
            << QString::number(coords.z(), 'f', 15) << "\n";
    }
    
    out << elementCount << "\n";
    
    // Записываем элементы с преобразованием ID узлов
    for (const QList<int> &elem : elements) {
        for (int k = 0; k < elem.size(); k++) {
            int gmshNodeId = elem[k];
            int sequentialIndex = nodeIdToIndex[gmshNodeId];
            out << sequentialIndex;
            if (k < elem.size() - 1) out << " ";
        }
        out << "\n";
    }
    
    outFile.close();
    
    return true;
}

bool MeshGenerator::generateAdjacencyFile(const QString &mshFile, const QString &adjFile, QString &errorMsg) {
    // Проверка существования .msh файла
    if (!QFileInfo::exists(mshFile)) {
        errorMsg = QString("Файл .msh не найден: %1").arg(mshFile);
        lastError = errorMsg;
        return false;
    }
    
    QFile file(mshFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QString("Не удалось открыть файл %1: %2").arg(mshFile).arg(file.errorString());
        lastError = errorMsg;
        return false;
    }
    
    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();
    
    // Поиск секции узлов
    QMap<int, QVector3D> nodes;
    int i = 0;
    while (i < lines.size()) {
        if (lines[i].trimmed() == "$Nodes") {
            i++;
            if (i < lines.size()) {
                int numNodes = lines[i].split(" ", Qt::SkipEmptyParts)[0].toInt();
                i++;
                for (int j = 0; j < numNodes && i < lines.size(); j++) {
                    QStringList parts = lines[i].split(" ", Qt::SkipEmptyParts);
                    if (parts.size() >= 4) {
                        int nodeId = parts[0].toInt();
                        double x = parts[1].toDouble();
                        double y = parts[2].toDouble();
                        double z = parts[3].toDouble();
                        nodes[nodeId] = QVector3D(x, y, z);
                    }
                    i++;
                }
            }
            break;
        }
        i++;
    }
    
    // Поиск секции элементов и создание adjacency списка
    QMap<int, QSet<int>> adjacency; // node_id -> set of adjacent node_ids
    i = 0;
    while (i < lines.size()) {
        if (lines[i].trimmed().contains("$Elements")) {
            i++;
            if (i < lines.size()) {
                int numElems = lines[i].split(" ", Qt::SkipEmptyParts)[0].toInt();
                i++;
                for (int j = 0; j < numElems && i < lines.size(); j++) {
                    QStringList parts = lines[i].split(" ", Qt::SkipEmptyParts);
                    if (parts.size() >= 4) {
                        int elemType = parts[1].toInt();
                        if (elemType == 2) { // Треугольник
                            int numTags = parts[2].toInt();
                            int nodeStartIdx = 3 + numTags;
                            if (parts.size() >= nodeStartIdx + 3) {
                                QList<int> elemNodes;
                                for (int k = 0; k < 3; k++) {
                                    elemNodes.append(parts[nodeStartIdx + k].toInt());
                                }
                                // Добавляем смежности для каждого узла
                                for (int k = 0; k < 3; k++) {
                                    int node1 = elemNodes[k];
                                    int node2 = elemNodes[(k + 1) % 3];
                                    int node3 = elemNodes[(k + 2) % 3];
                                    adjacency[node1].insert(node2);
                                    adjacency[node1].insert(node3);
                                }
                            }
                        }
                    }
                    i++;
                }
            }
            break;
        }
        i++;
    }
    
    // Создаем директорию для adjacency.txt если её нет
    QFileInfo adjFileInfo(adjFile);
    QDir adjDir = adjFileInfo.absoluteDir();
    if (!adjDir.exists()) {
        if (!adjDir.mkpath(".")) {
            errorMsg = QString("Не удалось создать директорию для adjacency.txt: %1").arg(adjDir.absolutePath());
            lastError = errorMsg;
            return false;
        }
    }
    
    // Запись в adjacency.txt
    QFile outFile(adjFile);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMsg = QString("Не удалось создать файл %1: %2").arg(adjFile).arg(outFile.errorString());
        lastError = errorMsg;
        return false;
    }
    
    QTextStream out(&outFile);
    QList<int> sortedNodeIds = nodes.keys();
    std::sort(sortedNodeIds.begin(), sortedNodeIds.end());
    
    for (int nodeId : sortedNodeIds) {
        QVector3D coords = nodes[nodeId];
        QSet<int> adjNodes = adjacency[nodeId];
        QList<int> adjList = adjNodes.values();
        std::sort(adjList.begin(), adjList.end());
        
        out << nodeId << " " 
            << QString::number(coords.x(), 'f', 15) << " "
            << QString::number(coords.y(), 'f', 15) << " "
            << QString::number(coords.z(), 'f', 15) << " "
            << adjList.size();
        
        for (int adjNodeId : adjList) {
            out << " " << adjNodeId;
        }
        out << "\n";
    }
    
    outFile.close();
    
    return true;
}

