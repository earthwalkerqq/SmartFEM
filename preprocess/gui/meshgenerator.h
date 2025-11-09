#ifndef MESHGENERATOR_H
#define MESHGENERATOR_H

#include <QString>
#include <QStringList>

class MeshGenerator {
public:
    MeshGenerator();
    ~MeshGenerator();
    
    // Генерация сетки из STEP файла
    // Возвращает true при успехе, false при ошибке
    // errorMsg содержит описание ошибки при неудаче
    bool generateMesh(const QString &stepFile, 
                      const QString &outputMeshFile,
                      const QString &outputNodeFile,
                      const QString &outputAdjFile,
                      double clmin, 
                      double clmax,
                      QString &errorMsg);
    
    // Получить количество сгенерированных узлов и элементов
    int getNodeCount() const { return nodeCount; }
    int getElementCount() const { return elementCount; }
    
    // Получить последнее сообщение об ошибке
    QString getLastError() const { return lastError; }

private:
    int nodeCount;
    int elementCount;
    QString lastError;
    
    // Конвертация .msh в node.txt формат
    bool convertMshToNodeTxt(const QString &mshFile, const QString &nodeFile, QString &errorMsg);
    
    // Генерация adjacency.txt файла
    bool generateAdjacencyFile(const QString &mshFile, const QString &adjFile, QString &errorMsg);
};

#endif // MESHGENERATOR_H

