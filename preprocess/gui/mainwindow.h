#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QFileDialog>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QMessageBox>
#include <QTabWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QMap>
#include <QSplitter>
#include <QScrollArea>
#include <QListWidget>

// Предварительное объявление
class MeshViewer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Раздел 1: Выбор файла и места сохранения
    void browseStepFile();
    void browseOutputDir();
    void loadMesh();
    
    // Раздел 2: Материал (уже есть в UI)
    
    // Раздел 3: Параметры сетки
    void calculateMeshParams();
    void generateMesh();
    
    // Раздел 4: Выбор сил и граничных условий
    void onBoundaryConditionsChanged();
    void onNodesSelected(const QSet<int> &nodeIds);
    void onNodeClicked(int nodeId, const QPointF &coords);
    void onNodeDoubleClicked(int nodeId, const QPointF &coords);
    void addFixedNode();
    void addLoadNode();
    void removeFixedNode();
    void removeLoadNode();
    void clearAllLoads();
    
    // Раздел 5: Выбор расчета
    void runAnalysis();
    
    // Вспомогательные
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError error);
    void saveResultsToDesktop();
    void openGmsh();

private:
    void setupUI();
    void setupFileGroup();      // Раздел 1
    void setupMaterialGroup();  // Раздел 2
    void setupMeshGroup();      // Раздел 3
    void setupBoundaryGroup();  // Раздел 4
    void setupAnalysisGroup();  // Раздел 5
    void applyModernStyles();   // Применение современных стилей
    
    // Создание групп UI
    QGroupBox* createFileGroup();
    QGroupBox* createMaterialGroup();
    QGroupBox* createMeshGroup();
    QGroupBox* createBoundaryGroup();
    QGroupBox* createAnalysisGroup();
    
    // Вспомогательные методы для работы с граничными условиями
    void updateMeshViewerBoundaryConditions();
    bool saveBoundaryConditionsToFile();
    bool loadBoundaryConditionsFromFile();
    void selectNodeById();  // Оставлен для совместимости (не используется)
    void updateSelectedNodesLabel();  // Оставлен для совместимости (не используется)
    
    // Вспомогательные методы для генерации сетки
    int countElementsInMeshFile(const QString &mshFile);
    
    // Раздел 1: Файлы
    QLineEdit *stepFileEdit;
    QPushButton *browseStepBtn;
    QLineEdit *outputDirEdit;
    QPushButton *browseOutputBtn;
    QLineEdit *resultFileEdit;  // Имя файла результатов
    
    // Раздел 2: Материал
    QDoubleSpinBox *eEdit;      // Модуль Юнга (E)
    QDoubleSpinBox *nuEdit;     // Коэффициент Пуассона (ν)
    QDoubleSpinBox *rhoEdit;    // Плотность (ρ)
    QDoubleSpinBox *hEdit;      // Толщина (h)
    
    // Раздел 3: Параметры сетки
    QSpinBox *numElementsEdit;
    QPushButton *calcMeshParamsBtn;
    QDoubleSpinBox *clminEdit;
    QDoubleSpinBox *clmaxEdit;
    
    // Раздел 4: Граничные условия и нагрузки
    QLabel *boundaryConditionsLabel;
    QLabel *loadsLabel;
    QComboBox *constraintTypeCombo;
    QDoubleSpinBox *loadFxEdit;
    QDoubleSpinBox *loadFyEdit;
    QPushButton *addFixedBtn;
    QPushButton *addLoadBtn;
    QListWidget *fixedUListWidget;
    QListWidget *fixedVListWidget;
    QListWidget *loadedListWidget;
    QPushButton *removeFixedBtn;
    QPushButton *removeLoadBtn;
    
    // Раздел 5: Выбор расчета
    QComboBox *analysisTypeCombo;
    QSpinBox *numModesEdit;
    QPushButton *runBtn;
    
    // Вывод
    QTextEdit *outputText;
    
    // Процесс
    QProcess *process;
    
    // Данные граничных условий (будут заполняться через окно выбора узлов)
    QStringList fixedNodesU;    // Узлы с закреплением по U (x)
    QStringList fixedNodesV;    // Узлы с закреплением по V (y)
    QStringList loadedNodes;    // Узлы с нагрузками
    QMap<QString, QPair<double, double>> nodeLoads;  // Нагрузки: node_id -> (Fx, Fy)
    bool boundaryConditionsModifiedByUser;  // Флаг: были ли граничные условия изменены пользователем после загрузки из файла
    
    // Корень проекта
    QString projectRoot;
    
    // 3D визуализация
    MeshViewer *meshViewer;
    QSplitter *mainSplitter;
    QScrollArea *paramsScrollArea;
};

#endif // MAINWINDOW_H
