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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void browseStepFile();
    void browseOutputDir();
    void calculateMeshParams();
    void runAnalysis();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError error);
    void saveResultsToDesktop();
    void openGmsh();

private:
    void setupUI();
    void setupMeshGroup();
    void setupMaterialGroup();
    void setupAnalysisGroup();
    void setupOutputGroup();
    
    // UI Elements
    QLineEdit *stepFileEdit;
    QPushButton *browseStepBtn;
    QLineEdit *outputDirEdit;
    QPushButton *browseOutputBtn;
    
    // Mesh parameters
    QLineEdit *numElementsEdit;
    QPushButton *calcMeshParamsBtn;
    QLineEdit *clminEdit;
    QLineEdit *clmaxEdit;
    QPushButton *showMeshGuiBtn;
    
    // Material parameters
    QLineEdit *eEdit;
    QLineEdit *nuEdit;
    QLineEdit *rhoEdit;
    QLineEdit *hEdit;
    
    // Analysis parameters
    QComboBox *analysisTypeCombo;
    QLineEdit *numModesEdit;
    QPushButton *runBtn;
    
    // Output
    QTextEdit *outputText;
    
    // Process
    QProcess *process;
    
    QString projectRoot;
};

#endif // MAINWINDOW_H

