#include "mainwindow.h"
#include "meshviewer.h"
#include "meshgenerator.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QFile>
#include <QTabWidget>
#include <QSplitter>
#include <QScrollArea>
#include <QListWidget>
#include <QPair>
#include <QVector3D>
#include <QTimer>
#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), process(nullptr), meshViewer(nullptr), boundaryConditionsModifiedByUser(false) {
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
    
    // ВАЖНО: Всегда устанавливаем значения по умолчанию для сил в 0
    // Это гарантирует, что поля будут пустыми при запуске приложения
    if (loadFxEdit) {
        loadFxEdit->blockSignals(true);  // Блокируем сигналы, чтобы избежать лишних обновлений
        loadFxEdit->setValue(0.0);
        loadFxEdit->blockSignals(false);
    }
    if (loadFyEdit) {
        loadFyEdit->blockSignals(true);
        loadFyEdit->setValue(0.0);
        loadFyEdit->blockSignals(false);
    }
    
    // Удаляем старый файл loads.txt, если он существует, чтобы избежать загрузки старых значений
    QString loadsFile = projectRoot + "/build/loads.txt";
    if (QFileInfo::exists(loadsFile)) {
        QFile::remove(loadsFile);
        outputText->append("Удален старый файл loads.txt\n");
    }
    
    outputText->append(QString("Project root: %1\n").arg(projectRoot));
}

MainWindow::~MainWindow() {
    if (process) {
        process->kill();
        process->deleteLater();
    }
}

void MainWindow::setupUI() {
    setWindowTitle("SmartFEM - 3D Моделирование и Расчет");
    setMinimumSize(1400, 700);  // Компактный минимальный размер окна
    resize(1600, 900);  // Компактный начальный размер окна
    
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);  // Увеличенные отступы для лучшего вида
    mainLayout->setSpacing(10);  // Увеличенный spacing между элементами
    
    // Создаем главный разделитель: слева - 3D визуализация, справа - параметры
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Левая часть: 3D визуализация
    QWidget *viewerWidget = new QWidget(this);
    viewerWidget->setStyleSheet("QWidget { background-color: #2d2d2d; border: 2px solid #404040; border-radius: 8px; }");
    QVBoxLayout *viewerLayout = new QVBoxLayout(viewerWidget);
    viewerLayout->setContentsMargins(0, 0, 0, 0);  // Убираем отступы, чтобы meshViewer совпадал с viewerWidget
    viewerLayout->setSpacing(0);
    
    // Создаем 3D визуализатор
    meshViewer = new MeshViewer(this);
    // Устанавливаем фиксированный размер для совпадения с размером viewerWidget
    // width() и height() в meshviewer.cpp должны возвращать 825x734 (как viewerWidget)
    meshViewer->setFixedSize(825, 734);  // Фиксированный размер, совпадающий с viewerWidget
    
    // Подключаем сигналы от визуализатора
    connect(meshViewer, &MeshViewer::nodeClicked, this, &MainWindow::onNodeClicked);
    connect(meshViewer, &MeshViewer::nodeDoubleClicked, this, &MainWindow::onNodeDoubleClicked);
    connect(meshViewer, &MeshViewer::nodesSelected, this, &MainWindow::onNodesSelected);
    
    viewerLayout->addWidget(meshViewer);  // Добавляем без растягивания (фиксированный размер)
    mainSplitter->addWidget(viewerWidget);
    
    // Устанавливаем фиксированные размеры ПОСЛЕ добавления в splitter
    // Border в CSS (2px solid) НЕ влияет на width()/height() в Qt, поэтому устанавливаем одинаковые размеры
    viewerWidget->setFixedSize(825, 734);
    meshViewer->setFixedSize(825, 734);  // Устанавливаем еще раз после добавления в layout
    
    // Устанавливаем размеры еще раз после полной инициализации виджета
    // Это гарантирует, что размеры будут правильными даже если Qt изменит их
    QTimer::singleShot(0, this, [this, viewerWidget]() {
        viewerWidget->setFixedSize(825, 734);
        meshViewer->setFixedSize(825, 734);
        qDebug() << "=== MainWindow::setupUI() (after show) ===";
        qDebug() << "  viewerWidget size:" << viewerWidget->width() << "x" << viewerWidget->height();
        qDebug() << "  meshViewer size:" << meshViewer->width() << "x" << meshViewer->height();
        qDebug() << "  Expected: 825 x 734";
    });
    
    // Не используем setStretchFactor, так как размер фиксированный
    
    // Правая часть: Панель параметров (скроллируемая, но растягивается на всю высоту)
    paramsScrollArea = new QScrollArea(this);
    paramsScrollArea->setWidgetResizable(true);
    paramsScrollArea->setMinimumWidth(450);  // Компактная минимальная ширина правой части
    paramsScrollArea->setMaximumWidth(550);  // Компактная максимальная ширина
    paramsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);  // Скроллбар только при необходимости
    paramsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // Отключаем горизонтальный скроллбар
    // Устанавливаем отступы для QScrollArea, чтобы контент не обрезался
    paramsScrollArea->setFrameShape(QFrame::NoFrame);  // Убираем рамку для большего пространства
    // Правая панель фиксированной ширины
    paramsScrollArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    
    QWidget *paramsWidget = new QWidget(this);
    paramsWidget->setMinimumHeight(1);  // Минимальная высота для растягивания
    // Устанавливаем компактную ширину для paramsWidget
    paramsWidget->setMinimumWidth(430);  // Компактная минимальная ширина для размещения текста
    paramsWidget->setMaximumWidth(530);  // Компактная максимальная ширина виджета
    paramsWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    QVBoxLayout *paramsLayout = new QVBoxLayout(paramsWidget);
    paramsLayout->setSpacing(12);  // Компактный spacing между группами
    // Компактные отступы
    paramsLayout->setContentsMargins(12, 12, 12, 12);  // Компактные отступы: left, top, right, bottom
    
    // Добавляем группы параметров
    paramsLayout->addWidget(createFileGroup());
    paramsLayout->addWidget(createMaterialGroup());
    paramsLayout->addWidget(createMeshGroup());
    paramsLayout->addWidget(createBoundaryGroup());
    paramsLayout->addWidget(createAnalysisGroup());
    
    // Добавляем stretch в конец для растягивания панели на всю высоту
    // Это гарантирует, что панель займет всю доступную высоту splitter'а
    paramsLayout->addStretch(1);
    
    paramsScrollArea->setWidget(paramsWidget);
    paramsScrollArea->setWidgetResizable(true);
    mainSplitter->addWidget(paramsScrollArea);
    mainSplitter->setStretchFactor(0, 1);  // Левая часть (3D визуализация) - растягивается
    mainSplitter->setStretchFactor(1, 0);  // Правая часть (параметры) - фиксированная ширина
    // Устанавливаем начальные размеры: правая часть имеет компактную ширину
    mainSplitter->setSizes(QList<int>() << 1100 << 480);  // Левая: 1100px, Правая: 480px (компактная ширина)
    
    // Создаем контейнер для splitter и нижней панели
    QWidget *splitterContainer = new QWidget(this);
    QVBoxLayout *splitterLayout = new QVBoxLayout(splitterContainer);
    splitterLayout->setContentsMargins(0, 0, 0, 0);
    splitterLayout->setSpacing(0);
    splitterLayout->addWidget(mainSplitter, 1);  // Splitter растягивается
    
    mainLayout->addWidget(splitterContainer, 1);  // Контейнер растягивается
    
    // Нижняя панель: Вывод и кнопка запуска
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    
    // Вывод
    QGroupBox *outputGroup = new QGroupBox("Вывод", this);
    QVBoxLayout *outputLayout = new QVBoxLayout(outputGroup);
    outputText = new QTextEdit(this);
    outputText->setReadOnly(true);
    outputText->setFont(QFont("Courier", 9));
    outputText->setMaximumHeight(150);
    outputLayout->addWidget(outputText);
    bottomLayout->addWidget(outputGroup, 3);
    
    // Кнопка запуска
    runBtn = new QPushButton("Запустить расчет", this);
    runBtn->setObjectName("runBtn");
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::runAnalysis);
    bottomLayout->addWidget(runBtn, 0);
    
    mainLayout->addLayout(bottomLayout, 0);
    
    // Применяем красивые стили к интерфейсу
    applyModernStyles();
}

void MainWindow::applyModernStyles() {
    // Темная тема для всего приложения
    QString styleSheet = R"(
        /* Основной фон окна - темный */
        QMainWindow {
            background-color: #1e1e1e;
        }
        
        /* Центральный виджет - темный */
        QWidget {
            background-color: #1e1e1e;
            color: #e0e0e0;
        }
        
        /* Панель параметров - темный фон */
        QScrollArea {
            background-color: #2d2d2d;
            border: none;
        }
        
        QScrollArea > QWidget > QWidget {
            background-color: #2d2d2d;
        }
        
        /* Группы параметров - темный стиль */
        QGroupBox {
            font-weight: bold;
            font-size: 10pt;
            color: #e0e0e0;
            border: 2px solid #505050;
            border-radius: 8px;
            margin-top: 8px;
            padding-top: 12px;
            padding-left: 8px;
            padding-right: 8px;
            padding-bottom: 10px;
            background-color: #2d2d2d;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            left: 10px;
            background-color: #2d2d2d;
            color: #5dade2;
            font-weight: bold;
            font-size: 10pt;
        }
        
        /* Поля ввода - темный стиль */
        QLineEdit, QSpinBox, QDoubleSpinBox {
            border: 2px solid #404040;
            border-radius: 4px;
            padding: 6px;
            background-color: #353535;
            color: #e0e0e0;
            selection-background-color: #5dade2;
            selection-color: #000000;
            font-size: 10pt;
        }
        
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 2px solid #5dade2;
            background-color: #404040;
        }
        
        /* Кнопки - темный стиль */
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 10pt;
            font-weight: bold;
            min-height: 30px;
        }
        
        QPushButton:hover {
            background-color: #5dade2;
        }
        
        QPushButton:pressed {
            background-color: #2980b9;
        }
        
        /* Кнопка генерации сетки - оранжевая */
        QPushButton[objectName="generateMeshBtn"] {
            background-color: #e67e22;
            font-size: 11pt;
            padding: 10px 20px;
        }
        
        QPushButton[objectName="generateMeshBtn"]:hover {
            background-color: #f39c12;
        }
        
        /* Кнопка запуска расчета - зеленая */
        QPushButton[objectName="runBtn"] {
            background-color: #27ae60;
            font-size: 12pt;
            padding: 12px 24px;
        }
        
        QPushButton[objectName="runBtn"]:hover {
            background-color: #2ecc71;
        }
        
        /* Комбобоксы - темный стиль */
        QComboBox {
            border: 2px solid #404040;
            border-radius: 4px;
            padding: 6px;
            background-color: #353535;
            color: #e0e0e0;
            font-size: 10pt;
        }
        
        QComboBox:focus {
            border: 2px solid #5dade2;
        }
        
        QComboBox::drop-down {
            border: none;
            width: 20px;
            background-color: #404040;
        }
        
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #e0e0e0;
            width: 0;
            height: 0;
        }
        
        QComboBox QAbstractItemView {
            background-color: #353535;
            border: 2px solid #404040;
            color: #e0e0e0;
            selection-background-color: #5dade2;
            selection-color: #000000;
        }
        
        /* Списки - темный стиль */
        QListWidget {
            border: 2px solid #404040;
            border-radius: 4px;
            background-color: #353535;
            color: #e0e0e0;
            font-size: 9pt;
        }
        
        QListWidget::item {
            padding: 4px;
            border-bottom: 1px solid #404040;
        }
        
        QListWidget::item:selected {
            background-color: #5dade2;
            color: #000000;
        }
        
        QListWidget::item:hover {
            background-color: #404040;
        }
        
        /* Текстовое поле вывода - темное */
        QTextEdit {
            border: 2px solid #404040;
            border-radius: 4px;
            background-color: #1a1a1a;
            color: #00ff00;
            font-family: 'Courier New', monospace;
            font-size: 9pt;
            padding: 5px;
        }
        
        /* Метки - светлый текст */
        QLabel {
            color: #e0e0e0;
            font-size: 10pt;
        }
        
        /* Инструкция - выделенный блок темный */
        QLabel[objectName="infoLabel"] {
            background-color: #2d3e50;
            border: 1px solid #5dade2;
            border-radius: 6px;
            padding: 10px;
            color: #e0e0e0;
        }
        
        /* Splitter - скрыт */
        QSplitter::handle {
            background-color: transparent;
            width: 0px;
        }
        
        QSplitter::handle:hover {
            background-color: transparent;
        }
        
        /* Скроллбары - темные */
        QScrollBar:vertical {
            border: none;
            background-color: #2d2d2d;
            width: 12px;
            margin: 0;
        }
        
        QScrollBar::handle:vertical {
            background-color: #555555;
            border-radius: 6px;
            min-height: 20px;
        }
        
        QScrollBar::handle:vertical:hover {
            background-color: #666666;
        }
        
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )";
    
    setStyleSheet(styleSheet);
}

QGroupBox* MainWindow::createFileGroup() {
    QGroupBox *group = new QGroupBox("Выбор файла и места сохранения", this);
    group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    QFormLayout *layout = new QFormLayout(group);
    // Устанавливаем компактные отступы для layout
    layout->setContentsMargins(10, 12, 10, 12);  // Компактные отступы: left, top, right, bottom
    layout->setSpacing(8);  // Компактное расстояние между строками
    // Настраиваем QFormLayout для правильного отображения меток
    layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);  // Выравнивание меток по правому краю
    layout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);  // Поля не растягиваются
    layout->setRowWrapPolicy(QFormLayout::DontWrapRows);  // Не переносим строки
    layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);  // Выравнивание формы
    layout->setHorizontalSpacing(8);  // Компактное расстояние между меткой и полем
    
    stepFileEdit = new QLineEdit(this);
    stepFileEdit->setMinimumWidth(250);  // Компактная минимальная ширина поля ввода
    stepFileEdit->setMaximumWidth(300);  // Компактная максимальная ширина
    browseStepBtn = new QPushButton("Обзор...", this);
    QHBoxLayout *stepLayout = new QHBoxLayout();
    stepLayout->setContentsMargins(0, 0, 0, 0);
    stepLayout->setSpacing(8);
    stepLayout->addWidget(stepFileEdit, 1);  // Поле ввода растягивается
    stepLayout->addWidget(browseStepBtn, 0);  // Кнопка фиксированного размера
    // Создаем метки с компактной шириной для правильного выравнивания
    QLabel *stepLabel = new QLabel("STEP файл:", this);
    stepLabel->setMinimumWidth(140);
    stepLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    stepLabel->setWordWrap(false);
    layout->addRow(stepLabel, stepLayout);
    connect(browseStepBtn, &QPushButton::clicked, this, &MainWindow::browseStepFile);
    
    outputDirEdit = new QLineEdit(this);
    outputDirEdit->setMinimumWidth(250);  // Компактная минимальная ширина поля ввода
    outputDirEdit->setMaximumWidth(300);  // Компактная максимальная ширина
    browseOutputBtn = new QPushButton("Обзор...", this);
    QHBoxLayout *outputDirLayout = new QHBoxLayout();
    outputDirLayout->setContentsMargins(0, 0, 0, 0);
    outputDirLayout->setSpacing(8);
    outputDirLayout->addWidget(outputDirEdit, 1);  // Поле ввода растягивается
    outputDirLayout->addWidget(browseOutputBtn, 0);
    QLabel *outputDirLabel = new QLabel("Выходная директория:", this);
    outputDirLabel->setMinimumWidth(140);
    outputDirLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    outputDirLabel->setWordWrap(false);
    layout->addRow(outputDirLabel, outputDirLayout);
    connect(browseOutputBtn, &QPushButton::clicked, this, &MainWindow::browseOutputDir);
    
    resultFileEdit = new QLineEdit(this);
    resultFileEdit->setPlaceholderText("result.txt");
    resultFileEdit->setMinimumWidth(250);  // Компактная минимальная ширина поля ввода
    resultFileEdit->setMaximumWidth(300);  // Компактная максимальная ширина
    QLabel *resultFileLabel = new QLabel("Имя файла результатов:", this);
    resultFileLabel->setMinimumWidth(140);
    resultFileLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    resultFileLabel->setWordWrap(false);
    layout->addRow(resultFileLabel, resultFileEdit);
    
    return group;
}

QGroupBox* MainWindow::createMaterialGroup() {
    QGroupBox *group = new QGroupBox("Параметры материала", this);
    group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    QFormLayout *layout = new QFormLayout(group);
    // Устанавливаем отступы для layout: уменьшаем левый, увеличиваем правый
    layout->setContentsMargins(10, 12, 10, 12);  // Компактные отступы
    layout->setSpacing(8);
    // Настраиваем QFormLayout для правильного отображения меток
    layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);  // Поля не растягиваются
    layout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->setHorizontalSpacing(8);
    
    eEdit = new QDoubleSpinBox(this);
    eEdit->setRange(1.0, 1.0e12);
    eEdit->setValue(2.1e5);
    eEdit->setSuffix(" МПа");
    eEdit->setDecimals(0);
    eEdit->setMinimumWidth(120);  // Компактная минимальная ширина поля ввода
    eEdit->setMaximumWidth(180);  // Компактная максимальная ширина
    QLabel *eLabel = new QLabel("Модуль упругости E:", this);
    eLabel->setMinimumWidth(140);
    eLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    eLabel->setWordWrap(false);
    layout->addRow(eLabel, eEdit);
    
    nuEdit = new QDoubleSpinBox(this);
    nuEdit->setRange(0.0, 0.5);
    nuEdit->setValue(0.3);
    nuEdit->setDecimals(3);
    nuEdit->setSingleStep(0.01);
    nuEdit->setMinimumWidth(120);  // Компактная минимальная ширина поля ввода
    nuEdit->setMaximumWidth(180);  // Компактная максимальная ширина
    QLabel *nuLabel = new QLabel("Коэффициент Пуассона ν:", this);
    nuLabel->setMinimumWidth(140);
    nuLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    nuLabel->setWordWrap(false);
    layout->addRow(nuLabel, nuEdit);
    
    rhoEdit = new QDoubleSpinBox(this);
    rhoEdit->setRange(1.0, 50000.0);
    rhoEdit->setValue(7850.0);
    rhoEdit->setSuffix(" кг/м³");
    rhoEdit->setDecimals(1);
    rhoEdit->setMinimumWidth(120);  // Компактная минимальная ширина поля ввода
    rhoEdit->setMaximumWidth(180);  // Компактная максимальная ширина
    QLabel *rhoLabel = new QLabel("Плотность ρ:", this);
    rhoLabel->setMinimumWidth(140);
    rhoLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    rhoLabel->setWordWrap(false);
    layout->addRow(rhoLabel, rhoEdit);
    
    hEdit = new QDoubleSpinBox(this);
    hEdit->setRange(0.001, 100.0);
    hEdit->setValue(1.0);
    hEdit->setSuffix(" м");
    hEdit->setDecimals(3);
    hEdit->setMinimumWidth(120);  // Компактная минимальная ширина поля ввода
    hEdit->setMaximumWidth(180);  // Компактная максимальная ширина
    QLabel *hLabel = new QLabel("Толщина h:", this);
    hLabel->setMinimumWidth(140);
    hLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    hLabel->setWordWrap(false);
    layout->addRow(hLabel, hEdit);
    
    return group;
}

QGroupBox* MainWindow::createMeshGroup() {
    QGroupBox *group = new QGroupBox("Параметры сетки", this);
    group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    QFormLayout *layout = new QFormLayout(group);
    // Устанавливаем отступы для layout: уменьшаем левый, увеличиваем правый
    layout->setContentsMargins(10, 12, 10, 12);  // Компактные отступы
    layout->setSpacing(8);
    // Настраиваем QFormLayout для правильного отображения меток
    layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);  // Поля не растягиваются
    layout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->setHorizontalSpacing(8);
    
    numElementsEdit = new QSpinBox(this);
    numElementsEdit->setRange(10, 100000);
    numElementsEdit->setValue(500);
    numElementsEdit->setMinimumWidth(80);  // Компактная минимальная ширина поля ввода
    numElementsEdit->setMaximumWidth(120);  // Компактная максимальная ширина
    calcMeshParamsBtn = new QPushButton("Рассчитать параметры", this);
    calcMeshParamsBtn->setMinimumWidth(140);  // Компактная минимальная ширина кнопки
    calcMeshParamsBtn->setMaximumWidth(200);  // Компактная максимальная ширина кнопки
    QHBoxLayout *numElementsLayout = new QHBoxLayout();
    numElementsLayout->setContentsMargins(0, 0, 0, 0);
    numElementsLayout->setSpacing(8);
    numElementsLayout->addWidget(numElementsEdit, 0);  // Поле ввода фиксированного размера
    numElementsLayout->addWidget(calcMeshParamsBtn, 0);  // Кнопка фиксированного размера, не растягивается
    QLabel *numElementsLabel = new QLabel("Желаемое количество элементов:", this);
    numElementsLabel->setMinimumWidth(160);
    numElementsLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    numElementsLabel->setWordWrap(false);
    layout->addRow(numElementsLabel, numElementsLayout);
    connect(calcMeshParamsBtn, &QPushButton::clicked, this, &MainWindow::calculateMeshParams);
    
    clminEdit = new QDoubleSpinBox(this);
    clminEdit->setRange(0.001, 100.0);
    clminEdit->setValue(0.8);
    clminEdit->setDecimals(3);
    clminEdit->setMinimumWidth(120);  // Компактная минимальная ширина поля ввода
    clminEdit->setMaximumWidth(180);  // Компактная максимальная ширина
    QLabel *clminLabel = new QLabel("Минимальный размер элемента (clmin):", this);
    clminLabel->setMinimumWidth(160);
    clminLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    clminLabel->setWordWrap(false);
    layout->addRow(clminLabel, clminEdit);
    
    clmaxEdit = new QDoubleSpinBox(this);
    clmaxEdit->setRange(0.01, 100.0);
    clmaxEdit->setValue(6.0);
    clmaxEdit->setDecimals(3);
    clmaxEdit->setMinimumWidth(120);  // Компактная минимальная ширина поля ввода
    clmaxEdit->setMaximumWidth(180);  // Компактная максимальная ширина
    QLabel *clmaxLabel = new QLabel("Максимальный размер элемента (clmax):", this);
    clmaxLabel->setMinimumWidth(160);
    clmaxLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    clmaxLabel->setWordWrap(false);
    layout->addRow(clmaxLabel, clmaxEdit);
    
    // Кнопка генерации сетки
    QPushButton *generateMeshBtn = new QPushButton("Сгенерировать сетку", this);
    generateMeshBtn->setObjectName("generateMeshBtn");
    connect(generateMeshBtn, &QPushButton::clicked, this, &MainWindow::generateMesh);
    layout->addRow(generateMeshBtn);
    
    return group;
}

QGroupBox* MainWindow::createBoundaryGroup() {
    QGroupBox *group = new QGroupBox("Граничные условия и нагрузки", this);
    group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    QVBoxLayout *layout = new QVBoxLayout(group);
    // Устанавливаем компактные отступы для layout
    layout->setContentsMargins(10, 12, 10, 12);  // Компактные отступы
    layout->setSpacing(8);
    
    QLabel *infoLabel = new QLabel(
        "Инструкция:\n"
        "• Двойной клик по узлу в 3D модели - выделение/снятие выделения узла\n"
        "• Выберите тип условия и значения, затем нажмите 'Добавить закрепление' или 'Добавить нагрузку'", this);
    infoLabel->setObjectName("infoLabel");
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);
    
    // Тип условия
    constraintTypeCombo = new QComboBox(this);
    constraintTypeCombo->addItem("Закрепление по U (x)");
    constraintTypeCombo->addItem("Закрепление по V (y)");
    constraintTypeCombo->addItem("Сосредоточенная сила");
    constraintTypeCombo->setMinimumWidth(200);  // Компактная минимальная ширина
    constraintTypeCombo->setMaximumWidth(280);  // Компактная максимальная ширина
    layout->addWidget(new QLabel("Тип условия:", this));
    layout->addWidget(constraintTypeCombo);
    
    // Значения сил
    loadFxEdit = new QDoubleSpinBox(this);
    loadFxEdit->setRange(-1e10, 1e10);
    loadFxEdit->setValue(0.0);
    loadFxEdit->setSuffix(" Н");
    loadFxEdit->setDecimals(2);
    loadFxEdit->setMinimumWidth(120);  // Компактная минимальная ширина
    loadFxEdit->setMaximumWidth(180);  // Компактная максимальная ширина
    layout->addWidget(new QLabel("Сила Fx:", this));
    layout->addWidget(loadFxEdit);
    
    loadFyEdit = new QDoubleSpinBox(this);
    loadFyEdit->setRange(-1e10, 1e10);
    loadFyEdit->setValue(0.0);
    loadFyEdit->setSuffix(" Н");
    loadFyEdit->setDecimals(2);
    loadFyEdit->setMinimumWidth(120);  // Компактная минимальная ширина
    loadFyEdit->setMaximumWidth(180);  // Компактная максимальная ширина
    layout->addWidget(new QLabel("Сила Fy:", this));
    layout->addWidget(loadFyEdit);
    
    // Кнопки добавления
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);  // Отступ между кнопками
    buttonLayout->setContentsMargins(0, 0, 0, 0);  // Без отступов в layout
    addFixedBtn = new QPushButton("Добавить закрепление", this);
    addLoadBtn = new QPushButton("Добавить нагрузку", this);
    buttonLayout->addWidget(addFixedBtn);
    buttonLayout->addWidget(addLoadBtn);
    buttonLayout->addStretch(1);  // Добавляем stretch справа, чтобы кнопки не обрезались
    layout->addLayout(buttonLayout);
    
    connect(addFixedBtn, &QPushButton::clicked, this, &MainWindow::addFixedNode);
    connect(addLoadBtn, &QPushButton::clicked, this, &MainWindow::addLoadNode);
    connect(constraintTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                bool isLoad = (index == 2);
                loadFxEdit->setEnabled(isLoad);
                loadFyEdit->setEnabled(isLoad);
                addFixedBtn->setEnabled(!isLoad);
                addLoadBtn->setEnabled(isLoad);
            });
    
    // Списки выбранных узлов в вертикальном layout для лучшего отображения
    QVBoxLayout *listsLayout = new QVBoxLayout();
    
    // Закрепления U
    QLabel *fixedULabel = new QLabel("Закрепления по U (x):", this);
    fixedULabel->setStyleSheet("QLabel { font-weight: bold; color: #e0e0e0; }");
    listsLayout->addWidget(fixedULabel);
    fixedUListWidget = new QListWidget(this);
    fixedUListWidget->setMaximumHeight(80);
    fixedUListWidget->setAlternatingRowColors(true);
    listsLayout->addWidget(fixedUListWidget);
    
    // Закрепления V
    QLabel *fixedVLabel = new QLabel("Закрепления по V (y):", this);
    fixedVLabel->setStyleSheet("QLabel { font-weight: bold; color: #e0e0e0; }");
    listsLayout->addWidget(fixedVLabel);
    fixedVListWidget = new QListWidget(this);
    fixedVListWidget->setMaximumHeight(80);
    fixedVListWidget->setAlternatingRowColors(true);
    listsLayout->addWidget(fixedVListWidget);
    
    // Нагрузки
    QLabel *loadsHeaderLabel = new QLabel("Нагрузки:", this);
    loadsHeaderLabel->setStyleSheet("QLabel { font-weight: bold; color: #e0e0e0; }");
    listsLayout->addWidget(loadsHeaderLabel);
    loadedListWidget = new QListWidget(this);
    loadedListWidget->setMaximumHeight(80);
    loadedListWidget->setAlternatingRowColors(true);
    listsLayout->addWidget(loadedListWidget);
    
    layout->addLayout(listsLayout);
    
    // Кнопки удаления
    QHBoxLayout *removeLayout = new QHBoxLayout();
    removeLayout->setSpacing(8);  // Отступ между кнопками
    removeLayout->setContentsMargins(0, 0, 0, 0);  // Без отступов в layout
    removeFixedBtn = new QPushButton("Удалить закрепление", this);
    removeLoadBtn = new QPushButton("Удалить нагрузку", this);
    QPushButton *clearAllLoadsBtn = new QPushButton("Очистить все нагрузки", this);
    clearAllLoadsBtn->setStyleSheet("QPushButton { background-color: #e74c3c; } QPushButton:hover { background-color: #c0392b; }");
    removeLayout->addWidget(removeFixedBtn);
    removeLayout->addWidget(removeLoadBtn);
    removeLayout->addWidget(clearAllLoadsBtn);
    removeLayout->addStretch(1);  // Добавляем stretch справа, чтобы кнопки не обрезались
    layout->addLayout(removeLayout);
    
    connect(removeFixedBtn, &QPushButton::clicked, this, &MainWindow::removeFixedNode);
    connect(removeLoadBtn, &QPushButton::clicked, this, &MainWindow::removeLoadNode);
    connect(clearAllLoadsBtn, &QPushButton::clicked, this, &MainWindow::clearAllLoads);
    
    boundaryConditionsLabel = new QLabel("Закрепления: не заданы", this);
    loadsLabel = new QLabel("Нагрузки: не заданы", this);
    layout->addWidget(boundaryConditionsLabel);
    layout->addWidget(loadsLabel);
    
    return group;
}

QGroupBox* MainWindow::createAnalysisGroup() {
    QGroupBox *group = new QGroupBox("Выбор расчета", this);
    group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    QFormLayout *layout = new QFormLayout(group);
    // Устанавливаем отступы для layout: уменьшаем левый, увеличиваем правый
    layout->setContentsMargins(10, 12, 10, 12);  // Компактные отступы
    layout->setSpacing(8);
    // Настраиваем QFormLayout для правильного отображения меток
    layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);  // Поля не растягиваются
    layout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->setHorizontalSpacing(8);
    
    analysisTypeCombo = new QComboBox(this);
    analysisTypeCombo->addItem("Статический FEM анализ");
    analysisTypeCombo->addItem("Модальный анализ (колебания)");
    analysisTypeCombo->setMinimumWidth(200);  // Компактная минимальная ширина комбобокса
    analysisTypeCombo->setMaximumWidth(280);  // Компактная максимальная ширина
    QLabel *analysisTypeLabel = new QLabel("Тип анализа:", this);
    analysisTypeLabel->setMinimumWidth(140);
    analysisTypeLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    analysisTypeLabel->setWordWrap(false);
    layout->addRow(analysisTypeLabel, analysisTypeCombo);
    
    numModesEdit = new QSpinBox(this);
    numModesEdit->setRange(1, 100);
    numModesEdit->setValue(5);
    numModesEdit->setEnabled(false);
    numModesEdit->setMinimumWidth(120);  // Компактная минимальная ширина поля ввода
    numModesEdit->setMaximumWidth(180);  // Компактная максимальная ширина
    QLabel *numModesLabel = new QLabel("Количество мод:", this);
    numModesLabel->setMinimumWidth(140);
    numModesLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    numModesLabel->setWordWrap(false);
    layout->addRow(numModesLabel, numModesEdit);
    
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
    
    if (desiredElements < 10) {
        QMessageBox::warning(this, "Предупреждение", "Количество элементов должно быть не менее 10!");
        numElementsEdit->setValue(10);
        desiredElements = 10;
    }
    
    // Пытаемся получить реальную площадь модели из существующей сетки
    double estimatedArea = 520.0;  // Значение по умолчанию
    double modelWidth = 52.0;
    double modelHeight = 10.0;
    bool hasExistingMesh = false;
    
    // Если есть существующая сетка, используем ее для определения размеров модели
    QString nodeFile = projectRoot + "/build/node.txt";
    if (QFileInfo::exists(nodeFile)) {
        QFile file(nodeFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString firstLine = in.readLine();
            bool ok;
            int numNodes = firstLine.toInt(&ok);
            
            if (ok && numNodes > 0) {
                double minX = 1e9, maxX = -1e9;
                double minY = 1e9, maxY = -1e9;
                int validNodes = 0;
                
                for (int i = 0; i < numNodes && !in.atEnd(); i++) {
                    QString line = in.readLine();
                    QStringList parts = line.split(" ", Qt::SkipEmptyParts);
                    if (parts.size() >= 2) {
                        double x = parts[0].toDouble(&ok);
                        if (ok) {
                            double y = parts[1].toDouble(&ok);
                            if (ok) {
                                if (x < minX) minX = x;
                                if (x > maxX) maxX = x;
                                if (y < minY) minY = y;
                                if (y > maxY) maxY = y;
                                validNodes++;
                            }
                        }
                    }
                }
                
                if (validNodes > 0 && maxX > minX && maxY > minY) {
                    modelWidth = maxX - minX;
                    modelHeight = maxY - minY;
                    estimatedArea = modelWidth * modelHeight;
                    hasExistingMesh = true;
                }
            }
            file.close();
        }
    }
    
    // Улучшенная формула расчета на основе реального опыта работы с Gmsh
    // Для треугольной сетки: количество элементов ≈ площадь / (0.433 * clmax^2)
    // Коэффициент 0.433 получен из формулы площади равностороннего треугольника: (sqrt(3)/4) * h^2
    // где h - характерная длина элемента (clmax)
    const double elementAreaFactor = 0.433;  // Более точный коэффициент для треугольных элементов
    
    // Улучшенный расчет clmax с учетом нелинейности Gmsh
    // Gmsh генерирует больше элементов, чем предсказывает простая формула,
    // особенно для сложных геометрий и малого количества элементов
    double baseClmax = sqrt(estimatedArea / (elementAreaFactor * desiredElements));
    
    // Улучшенная корректировка на основе обратной связи от реальных результатов
    // Используем более агрессивную корректировку для малого количества элементов
    // Gmsh генерирует значительно больше элементов из-за адаптации к геометрии
    double correctionFactor;
    if (desiredElements < 20) {
        // Для очень малого количества элементов (10-19) используем очень агрессивную корректировку
        // Опыт показывает, что для 10 элементов нужно увеличить clmax в 3.0-5.0 раза
        correctionFactor = 2.5 + 2.5 * (20.0 - desiredElements) / 20.0;
        correctionFactor = qBound(3.0, correctionFactor, 5.5);  // Ограничиваем от 3.0 до 5.5
    } else if (desiredElements < 50) {
        // Для малого количества элементов (20-49) используем агрессивную корректировку
        correctionFactor = 1.5 + 1.0 * log(50.0 / (desiredElements + 1.0));
        correctionFactor = qBound(1.8, correctionFactor, 3.0);  // Ограничиваем от 1.8 до 3.0
    } else if (desiredElements < 200) {
        // Среднее количество элементов
        correctionFactor = 1.0 + 0.4 * log(200.0 / (desiredElements + 1.0));
        correctionFactor = qBound(1.2, correctionFactor, 1.8);  // Ограничиваем от 1.2 до 1.8
    } else if (desiredElements < 1000) {
        // Большое количество элементов
        correctionFactor = 1.0 + 0.2 * log(1000.0 / (desiredElements + 1.0));
        correctionFactor = qBound(1.1, correctionFactor, 1.4);  // Ограничиваем от 1.1 до 1.4
    } else {
        // Очень большое количество элементов - минимальная корректировка
        correctionFactor = 1.1;
    }
    
    double clmax = baseClmax * correctionFactor;
    
    // Улучшенный расчет clmin на основе желаемого количества элементов
    // Для грубых сеток (мало элементов) clmin должен быть близок к clmax
    // Для тонких сеток (много элементов) clmin может быть намного меньше clmax
    double clminRatio;
    if (desiredElements < 20) {
        // Очень грубая сетка - clmin очень близок к clmax для равномерности
        clminRatio = 0.85;  // clmin = 85% от clmax
    } else if (desiredElements < 50) {
        // Очень грубая сетка - clmin близок к clmax для равномерности
        clminRatio = 0.80;  // clmin = 80% от clmax
    } else if (desiredElements < 200) {
        // Грубая сетка
        clminRatio = 0.6;   // clmin = 60% от clmax
    } else if (desiredElements < 1000) {
        // Средняя сетка
        clminRatio = 0.4;   // clmin = 40% от clmax
    } else {
        // Тонкая сетка - clmin может быть значительно меньше clmax
        clminRatio = 0.25;  // clmin = 25% от clmax
    }
    
    double clmin = clmax * clminRatio;
    
    // Ограничения значений для стабильности работы Gmsh
    if (clmin < 0.01) clmin = 0.01;  // Минимальный размер элемента
    if (clmax < clmin * 1.05) clmax = clmin * 1.05;  // clmax должен быть хотя бы на 5% больше clmin
    if (clmax > 1000.0) clmax = 1000.0;  // Абсолютный максимум для предотвращения ошибок
    
    clminEdit->setValue(clmin);
    clmaxEdit->setValue(clmax);
    
    // Расчет ожидаемого количества элементов с учетом реальных параметров
    double expectedElements = estimatedArea / (elementAreaFactor * clmax * clmax);
    
    // Формируем информативное сообщение
    QString infoMsg = QString("Рассчитаны параметры сетки:\n")
                      .append(QString("  Желаемое количество элементов: %1\n").arg(desiredElements))
                      .append(QString("  Ожидаемое количество элементов: ~%1\n").arg((int)expectedElements))
                      .append(QString("  clmin=%1, clmax=%2\n").arg(clmin, 0, 'f', 4).arg(clmax, 0, 'f', 4))
                      .append(QString("  Корректирующий коэффициент: %1\n").arg(correctionFactor, 0, 'f', 2));
    
    if (hasExistingMesh) {
        infoMsg.append(QString("  Площадь модели: %1 (ширина: %2, высота: %3) [из существующей сетки]\n")
                       .arg(estimatedArea, 0, 'f', 2).arg(modelWidth, 0, 'f', 2).arg(modelHeight, 0, 'f', 2));
    } else {
        infoMsg.append(QString("  Площадь модели: %1 (ширина: %2, высота: %3) [значение по умолчанию]\n")
                       .arg(estimatedArea, 0, 'f', 2).arg(modelWidth, 0, 'f', 2).arg(modelHeight, 0, 'f', 2))
                       .append(QString("  Примечание: После первой генерации сетки размеры будут определяться автоматически.\n"));
    }
    
    infoMsg.append(QString("  Формула: элементы ≈ площадь / (0.433 * clmax²)\n"))
            .append(QString("  Если элементов получилось слишком много/мало, можно вручную скорректировать clmax.\n"));
    
    outputText->append(infoMsg);
}

void MainWindow::loadMesh() {
    QString nodeFile = projectRoot + "/build/node.txt";
    QString mshFile = projectRoot + "/build/HyperMesh.msh";
    
    if (QFileInfo::exists(nodeFile) && meshViewer) {
        meshViewer->loadMesh(nodeFile, mshFile);
        outputText->append("Сетка загружена в визуализатор\n");
        
        // Загружаем граничные условия из файла, если они есть
        loadBoundaryConditionsFromFile();
        
        // Убеждаемся, что поля Fx и Fy установлены в 0 после загрузки
        loadFxEdit->setValue(0.0);
        loadFyEdit->setValue(0.0);
        
        // Восстанавливаем граничные условия в визуализаторе
        updateMeshViewerBoundaryConditions();
    }
}

// Вспомогательная функция для подсчета элементов в .msh файле
int MainWindow::countElementsInMeshFile(const QString &mshFile) {
    QFile file(mshFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1;
    }
    
    QTextStream in(&file);
    bool inElementsSection = false;
    int elementCount = 0;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        if (line == "$Elements" || line == "$ELM") {
            inElementsSection = true;
            continue;
        }
        
        if (inElementsSection) {
            if (line == "$EndElements" || line == "$ENDELM") {
                break;
            }
            
            // Строка с элементом: element-number element-type number-of-tags tag1 tag2 ... node1 node2 ...
            QStringList parts = line.split(" ", Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                bool ok;
                int elementType = parts[1].toInt(&ok);
                // Типы элементов: 1=линия, 2=треугольник, 3=квад, 4=тетра, 5=гекса, 6=призма, 7=пирамида
                // Нас интересуют только 2D элементы (2, 3)
                if (ok && (elementType == 2 || elementType == 3)) {
                    elementCount++;
                }
            }
        }
    }
    
    return elementCount;
}

void MainWindow::generateMesh() {
    QString stepFile = stepFileEdit->text();
    if (stepFile.isEmpty() || !QFileInfo::exists(stepFile)) {
        QMessageBox::warning(this, "Ошибка", "Сначала выберите STEP файл!");
        return;
    }
    
    // Получаем желаемое количество элементов от пользователя
    int desiredElements = numElementsEdit->value();
    
    if (desiredElements < 10) {
        QMessageBox::warning(this, "Ошибка", "Количество элементов должно быть не менее 10!");
        numElementsEdit->setValue(10);
        desiredElements = 10;
    }
    
    outputText->append("=== Генерация сетки с заданным количеством элементов ===\n");
    outputText->append(QString("Входной файл: %1\n").arg(stepFile));
    outputText->append(QString("Желаемое количество элементов: %1\n").arg(desiredElements));
    outputText->repaint();
    QApplication::processEvents();
    
    // Определяем пути к выходным файлам
    QString mshFile = projectRoot + "/build/HyperMesh.msh";
    QString nodeFile = projectRoot + "/build/node.txt";
    QString adjFile = projectRoot + "/build/adjacency.txt";
    QString meshScript = projectRoot + "/HyperMesh/generate_mesh.sh";
    
    // Создаем директорию build если её нет
    QDir buildDir(projectRoot + "/build");
    if (!buildDir.exists()) {
        buildDir.mkpath(".");
    }
    
    // Проверяем наличие скрипта генерации
    if (!QFileInfo::exists(meshScript)) {
        QMessageBox::critical(this, "Ошибка", 
            QString("Скрипт генерации сетки не найден: %1").arg(meshScript));
        outputText->append(QString("❌ Ошибка: скрипт не найден: %1\n").arg(meshScript));
        return;
    }
    
    // Вычисляем начальные параметры сетки
    calculateMeshParams();
    double clmin = clminEdit->value();
    double clmax = clmaxEdit->value();
    
    // Итеративный подбор параметров для достижения желаемого количества элементов
    // Увеличиваем количество итераций для лучшей сходимости
    int maxIterations = (desiredElements < 30) ? 25 : (desiredElements < 50) ? 20 : 15;
    // Для очень грубых сеток увеличиваем допустимое отклонение, т.к. Gmsh может иметь ограничения
    double tolerance = (desiredElements < 20) ? 0.30 : 0.20;  // 30% для < 20 элементов, 20% для остальных
    int actualElements = 0;
    bool success = false;
    QString errorMsg;
    
    // История итераций для интерполяции
    struct IterationData {
        double clmax;
        int elements;
        double clmin;
    };
    QVector<IterationData> history;
    
    outputText->append(QString("Начальные параметры: clmin=%1, clmax=%2\n").arg(clmin, 0, 'f', 4).arg(clmax, 0, 'f', 4));
    outputText->append(QString("Максимум итераций: %1 (tolerance: %2%)\n").arg(maxIterations).arg(tolerance * 100.0, 0, 'f', 0));
    
    for (int iteration = 0; iteration < maxIterations; iteration++) {
        outputText->append(QString("\n--- Итерация %1/%2 ---\n").arg(iteration + 1).arg(maxIterations));
        outputText->append(QString("Параметры: clmin=%1, clmax=%2\n").arg(clmin, 0, 'f', 4).arg(clmax, 0, 'f', 4));
        outputText->repaint();
        QApplication::processEvents();
        
        QProcess meshProcess;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        meshProcess.setProcessEnvironment(env);
        meshProcess.setWorkingDirectory(projectRoot);
        
        // Запускаем скрипт генерации сетки
        QStringList args;
        args << meshScript
             << stepFile 
             << QString::number(clmin, 'f', 6)
             << QString::number(clmax, 'f', 6)
             << "0";  // Без GUI
        
        outputText->append(QString("Запуск: bash %1 %2 %3 %4\n")
                          .arg(meshScript).arg(stepFile).arg(QString::number(clmin, 'f', 6)).arg(QString::number(clmax, 'f', 6)));
        outputText->repaint();
        QApplication::processEvents();
        
        meshProcess.start("bash", args);
        
        if (!meshProcess.waitForStarted(5000)) {
            errorMsg = "Не удалось запустить процесс генерации сетки";
            outputText->append(QString("❌ Ошибка: %1\n").arg(errorMsg));
            break;
        }
        
        // Ждем завершения с периодическим обновлением UI
        while (!meshProcess.waitForFinished(1000)) {
            QApplication::processEvents();
            
            QString output = meshProcess.readAllStandardOutput();
            QString errors = meshProcess.readAllStandardError();
            
            if (!output.isEmpty()) {
                outputText->append(output);
            }
            if (!errors.isEmpty()) {
                outputText->append("STDERR: " + errors);
            }
            outputText->repaint();
        }
        
        QString stdOut = meshProcess.readAllStandardOutput();
        QString stdErr = meshProcess.readAllStandardError();
        int exitCode = meshProcess.exitCode();
        
        if (exitCode != 0) {
            errorMsg = QString("Ошибка генерации сетки (код выхода: %1)\n\nSTDERR:\n%2\n\nSTDOUT:\n%3")
                      .arg(exitCode).arg(stdErr).arg(stdOut);
            outputText->append(QString("❌ Ошибка: код выхода %1\n").arg(exitCode));
            if (!stdErr.isEmpty()) {
                outputText->append("Детали ошибки:\n" + stdErr + "\n");
            }
            break;
        }
        
        // Подсчитываем количество элементов в сгенерированной сетке
        if (!QFileInfo::exists(mshFile)) {
            errorMsg = "Файл сетки не был создан";
            outputText->append(QString("❌ Ошибка: %1\n").arg(errorMsg));
            break;
        }
        
        actualElements = countElementsInMeshFile(mshFile);
        
        if (actualElements <= 0) {
            errorMsg = "Не удалось определить количество элементов в сетке";
            outputText->append(QString("❌ Ошибка: %1\n").arg(errorMsg));
            break;
        }
        
        outputText->append(QString("Фактическое количество элементов: %1\n").arg(actualElements));
        outputText->repaint();
        QApplication::processEvents();
        
        // Сохраняем историю
        IterationData data;
        data.clmax = clmax;
        data.clmin = clmin;
        data.elements = actualElements;
        history.append(data);
        
        // Проверяем, соответствует ли количество элементов желаемому
        double ratio = (double)actualElements / desiredElements;
        double deviationPercent = (ratio - 1.0) * 100.0;
        
        outputText->append(QString("Отношение фактического к желаемому: %1 (отклонение: %2%)\n")
                          .arg(ratio, 0, 'f', 2).arg(deviationPercent, 0, 'f', 1));
        
        // Для очень малого количества элементов используем более гибкие критерии успеха
        // Gmsh может иметь ограничения геометрии, которые не позволяют создать очень грубую сетку
        bool isAcceptable = false;
        double effectiveTolerance = tolerance;
        
        if (desiredElements < 20) {
            // Для очень грубых сеток (< 20 элементов) допускаем большее отклонение
            // Если получили разумное количество элементов (не слишком много), считаем успехом
            if (ratio >= 0.5 && ratio <= 3.0) {
                // От 50% до 300% от желаемого - приемлемо для очень грубых сеток
                effectiveTolerance = 2.0;  // 200% отклонение допустимо
                isAcceptable = true;
                outputText->append(QString("  (Для очень грубой сетки допустимо отклонение до 200%%)\n"));
            }
        } else if (desiredElements < 50) {
            // Для грубых сеток (20-49 элементов) допускаем большее отклонение
            if (ratio >= 0.7 && ratio <= 2.5) {
                effectiveTolerance = 1.5;  // 150% отклонение допустимо
                isAcceptable = true;
                outputText->append(QString("  (Для грубой сетки допустимо отклонение до 150%%)\n"));
            }
        }
        
        // Стандартная проверка на точное соответствие
        if (ratio >= (1.0 - tolerance) && ratio <= (1.0 + tolerance)) {
            // Количество элементов в строгих допустимых пределах
            outputText->append(QString("✓ Количество элементов в допустимых пределах (отклонение: %1%)\n")
                              .arg(deviationPercent, 0, 'f', 1));
            success = true;
            isAcceptable = true;
            
            // Обновляем параметры в UI
            clminEdit->setValue(clmin);
            clmaxEdit->setValue(clmax);
            break;
        } else if (isAcceptable) {
            // Количество элементов в гибких допустимых пределах (для грубых сеток)
            outputText->append(QString("✓ Количество элементов приемлемо для грубой сетки (отклонение: %1%)\n")
                              .arg(deviationPercent, 0, 'f', 1));
            outputText->append(QString("  Примечание: Для очень грубых сеток Gmsh может иметь ограничения геометрии.\n"));
            success = true;
            
            // Обновляем параметры в UI
            clminEdit->setValue(clmin);
            clmaxEdit->setValue(clmax);
            break;
        }
        
        // Корректируем параметры для следующей итерации
        if (iteration < maxIterations - 1) {
            // Сохраняем текущее отношение clmin/clmax для сохранения пропорции
            double clminRatio = clmin / clmax;
            
            // Пытаемся использовать интерполяцию между известными точками
            // Приоритет интерполяции: если у нас есть точки с элементами больше и меньше желаемого, используем интерполяцию
            bool useInterpolation = false;
            double interpolatedClmax = clmax;
            
            // Всегда пытаемся использовать интерполяцию, если есть достаточно истории
            if (history.size() >= 2) {
                // Собираем все точки из истории, которые могут быть полезны для интерполяции
                IterationData bestAbove, bestBelow;
                bool foundAbove = false, foundBelow = false;
                double minDiffAbove = 1e9;
                double minDiffBelow = 1e9;
                
                // Ищем точку с элементами больше желаемого (ближайшую сверху)
                for (int i = history.size() - 1; i >= 0; i--) {
                    if (history[i].elements > desiredElements) {
                        double diff = history[i].elements - desiredElements;
                        if (!foundAbove || diff < minDiffAbove) {
                            bestAbove = history[i];
                            foundAbove = true;
                            minDiffAbove = diff;
                        }
                    }
                }
                
                // Ищем точку с элементами меньше желаемого (ближайшую снизу)
                for (int i = history.size() - 1; i >= 0; i--) {
                    if (history[i].elements < desiredElements) {
                        double diff = desiredElements - history[i].elements;
                        if (!foundBelow || diff < minDiffBelow) {
                            bestBelow = history[i];
                            foundBelow = true;
                            minDiffBelow = diff;
                        }
                    }
                }
                
                // Отладочная информация
                if (iteration >= 2) {  // Начинаем с 3-й итерации показывать отладку
                    outputText->append(QString("  [Отладка интерполяции] История: %1 точек, foundAbove=%2, foundBelow=%3\n")
                                      .arg(history.size()).arg(foundAbove ? "да" : "нет").arg(foundBelow ? "да" : "нет"));
                    if (foundAbove) {
                        outputText->append(QString("    Точка сверху: clmax=%1, элементы=%2 (diff=%3)\n")
                                          .arg(bestAbove.clmax, 0, 'f', 4).arg(bestAbove.elements).arg(minDiffAbove, 0, 'f', 1));
                    }
                    if (foundBelow) {
                        outputText->append(QString("    Точка снизу: clmax=%1, элементы=%2 (diff=%3)\n")
                                          .arg(bestBelow.clmax, 0, 'f', 4).arg(bestBelow.elements).arg(minDiffBelow, 0, 'f', 1));
                    }
                }
                
                // Если нашли обе точки и они охватывают желаемое значение, используем интерполяцию
                // Приоритет интерполяции: всегда использовать, если есть обе точки
                if (foundAbove && foundBelow && 
                    bestBelow.elements < desiredElements && desiredElements < bestAbove.elements &&
                    bestAbove.elements != bestBelow.elements) {
                    
                    // Используем интерполяцию на основе обратной зависимости: elements ≈ k / clmax^2
                    // Поэтому clmax ≈ sqrt(k / elements), и мы интерполируем в логарифмическом масштабе
                    // Или используем более простую формулу: clmax_new = clmax_old * sqrt(elements_old / elements_new)
                    
                    // Вариант 1: Логарифмическая интерполяция (более точная для нелинейной зависимости)
                    double logElementsBelow = log((double)bestBelow.elements + 1.0);  // +1 для избежания log(0)
                    double logElementsAbove = log((double)bestAbove.elements + 1.0);
                    double logElementsDesired = log((double)desiredElements + 1.0);
                    
                    if (logElementsAbove > logElementsBelow) {
                        double weight = (logElementsDesired - logElementsBelow) / (logElementsAbove - logElementsBelow);
                        weight = qMax(0.0, qMin(1.0, weight));  // Ограничиваем вес от 0 до 1
                        
                        // Интерполируем clmax в логарифмическом масштабе
                        double logClmaxBelow = log(bestBelow.clmax);
                        double logClmaxAbove = log(bestAbove.clmax);
                        double logClmaxInterpolated = logClmaxBelow + (logClmaxAbove - logClmaxBelow) * weight;
                        interpolatedClmax = exp(logClmaxInterpolated);
                        
                        // Проверяем, что интерполированное значение разумное
                        // Делаем проверку более мягкой, чтобы интерполяция срабатывала чаще
                        double minClmax = qMin(bestBelow.clmax, bestAbove.clmax) * 0.2;
                        double maxClmax = qMax(bestBelow.clmax, bestAbove.clmax) * 5.0;
                        
                        // Дополнительная проверка: интерполированное значение должно быть между известными точками
                        double actualMinClmax = qMin(bestBelow.clmax, bestAbove.clmax);
                        double actualMaxClmax = qMax(bestBelow.clmax, bestAbove.clmax);
                        
                        if (interpolatedClmax >= minClmax && interpolatedClmax <= maxClmax) {
                            // Проверяем, что значение значительно отличается от текущего (хотя бы на 5%)
                            double diffFromCurrent = qAbs(interpolatedClmax - clmax) / qMax(interpolatedClmax, clmax);
                            if (diffFromCurrent > 0.05 || iteration >= 3) {  // После 3-й итерации используем интерполяцию всегда
                                useInterpolation = true;
                                outputText->append(QString("  ✓ Интерполяция [итерация %1]: clmax_below=%2 (эл.=%3), clmax_above=%4 (эл.=%5) -> clmax=%6 (цель: %7 эл., weight=%.3f)\n")
                                                  .arg(iteration + 1).arg(bestBelow.clmax, 0, 'f', 4).arg(bestBelow.elements)
                                                  .arg(bestAbove.clmax, 0, 'f', 4).arg(bestAbove.elements)
                                                  .arg(interpolatedClmax, 0, 'f', 4).arg(desiredElements).arg(weight, 0, 'f', 3));
                            } else if (iteration >= 2) {
                                outputText->append(QString("  [Отладка] Интерполяция не использована: diffFromCurrent=%.3f (нужно >0.05)\n")
                                                  .arg(diffFromCurrent));
                            }
                        } else if (iteration >= 2) {
                            outputText->append(QString("  [Отладка] Интерполяция не использована: interpolatedClmax=%.4f вне диапазона [%.4f, %.4f]\n")
                                              .arg(interpolatedClmax, 0, 'f', 4)
                                              .arg(minClmax, 0, 'f', 4).arg(maxClmax, 0, 'f', 4));
                        }
                    } else if (iteration >= 2) {
                        outputText->append(QString("  [Отладка] Интерполяция не использована: logElementsAbove <= logElementsBelow\n"));
                    }
                } else if (iteration >= 2) {
                    outputText->append(QString("  [Отладка] Интерполяция не использована: не найдены обе точки или они не охватывают желаемое значение\n"));
                }
            }
            
            // Улучшенный алгоритм коррекции с учетом желаемого количества элементов
            // Приоритет: интерполяция > обычная коррекция
            
            if (useInterpolation) {
                // Используем интерполированное значение (приоритет)
                clmax = interpolatedClmax;
                clmin = clmax * clminRatio;
                outputText->append(QString("  Используется интерполированное значение clmax=%1\n").arg(clmax, 0, 'f', 4));
            } else if (ratio > 1.0 + tolerance) {
                // Слишком много элементов - увеличиваем clmax
                // Используем более прямую зависимость для более быстрой сходимости
                // Теоретически: элементы ≈ площадь / (0.433 * clmax^2)
                // Значит: clmax_new = clmax_old * sqrt(actual / desired)
                // Но из-за нелинейности Gmsh нужна дополнительная коррекция
                
                double correctionFactor;
                
                if (desiredElements < 30) {
                    // Для очень малого количества элементов используем очень агрессивную коррекцию
                    // и более прямую зависимость
                    if (ratio > 5.0) {
                        // Очень большое отклонение - используем почти прямую зависимость
                        correctionFactor = pow(ratio, 0.9);
                    } else if (ratio > 2.5) {
                        // Большое отклонение (например, 24 вместо 10 = 2.4)
                        // Используем более прямую зависимость с дополнительным запасом
                        correctionFactor = pow(ratio, 0.75) * 1.1;
                    } else if (ratio > 2.0) {
                        // Умеренно-большое отклонение
                        correctionFactor = pow(ratio, 0.7) * 1.2;
                    } else {
                        // Умеренное отклонение
                        correctionFactor = sqrt(ratio) * 1.25;
                    }
                } else if (desiredElements < 100) {
                    // Для малого количества элементов
                    if (ratio > 10.0) {
                        correctionFactor = pow(ratio, 0.8);
                    } else if (ratio > 3.0) {
                        correctionFactor = pow(ratio, 0.7) * 1.15;
                    } else {
                        correctionFactor = sqrt(ratio) * 1.2;
                    }
                } else {
                    // Для среднего и большого количества элементов
                    if (ratio > 10.0) {
                        correctionFactor = pow(ratio, 0.7);
                    } else if (ratio > 3.0) {
                        correctionFactor = sqrt(ratio) * 1.2;
                    } else {
                        correctionFactor = sqrt(ratio);
                    }
                }
                
                // Дополнительная коррекция на основе истории итераций
                // Если мы на поздних итерациях и все еще далеки от цели, будем еще агрессивнее
                if (iteration > 5 && ratio > 1.5) {
                    correctionFactor *= 1.15;  // Дополнительные 15% для поздних итераций
                    outputText->append(QString("  (Поздняя итерация: дополнительная коррекция +15%)\n"));
                }
                
                clmax *= correctionFactor;
                
                outputText->append(QString("  Коэффициент коррекции: %1 (ratio=%2)\n")
                                  .arg(correctionFactor, 0, 'f', 3).arg(ratio, 0, 'f', 2));
                
                // Для очень грубых сеток (мало элементов) делаем clmin ближе к clmax
                if (desiredElements < 30) {
                    // Увеличиваем clminRatio для более равномерной сетки
                    clminRatio = qMax(clminRatio, 0.8);
                }
                
                // Восстанавливаем пропорцию
                clmin = clmax * clminRatio;
            } else if (ratio < 1.0 - tolerance) {
                // Слишком мало элементов - уменьшаем clmax
                // Используем улучшенную коррекцию для случая, когда элементов слишком мало
                double correctionFactor;
                
                if (ratio < 0.3) {
                    // Очень мало элементов - агрессивное уменьшение clmax
                    correctionFactor = pow(1.0 / ratio, 0.6);
                } else if (ratio < 0.5) {
                    // Мало элементов
                    correctionFactor = pow(1.0 / ratio, 0.65);
                } else {
                    // Умеренно мало элементов
                    correctionFactor = sqrt(1.0 / ratio) * 0.9;  // Немного более консервативно
                }
                
                clmax /= correctionFactor;
                outputText->append(QString("  Коэффициент коррекции (мало элементов): %1 (ratio=%2)\n")
                                  .arg(correctionFactor, 0, 'f', 3).arg(ratio, 0, 'f', 2));
                
                // Восстанавливаем пропорцию
                clmin = clmax * clminRatio;
            }
            
            // Защита от зацикливания: если мы уже были в этом диапазоне clmax с похожим количеством элементов
            // Используем интерполяцию или более агрессивную коррекцию
            if (history.size() >= 3 && !useInterpolation) {
                // Проверяем, не попадаем ли мы в уже проверенный диапазон
                bool foundCycle = false;
                for (int i = history.size() - 2; i >= qMax(0, history.size() - 6); i--) {
                    double prevClmax = history[i].clmax;
                    double prevElements = (double)history[i].elements;
                    double clmaxDiff = qAbs(clmax - prevClmax) / qMax(clmax, prevClmax);
                    double elementsDiff = qAbs((double)actualElements - prevElements) / qMax((double)actualElements, prevElements);
                    
                    // Если и clmax, и количество элементов очень похожи, значит мы зациклились
                    if (clmaxDiff < 0.10 && elementsDiff < 0.15) {
                        foundCycle = true;
                        
                        // Если мы зациклились, пытаемся использовать среднее значение между известными точками
                        // которые дают разное количество элементов
                        if (history.size() >= 4) {
                            // Ищем две точки с разным количеством элементов
                            IterationData point1, point2;
                            bool foundPair = false;
                            
                            for (int j = history.size() - 1; j >= qMax(0, history.size() - 8); j--) {
                                for (int k = j - 1; k >= qMax(0, history.size() - 8); k--) {
                                    if (history[j].elements != history[k].elements &&
                                        (history[j].elements > desiredElements) != (history[k].elements > desiredElements)) {
                                        // Нашли пару точек по разные стороны от желаемого значения
                                        if (history[j].elements > desiredElements && history[k].elements < desiredElements) {
                                            point1 = history[j];
                                            point2 = history[k];
                                        } else if (history[j].elements < desiredElements && history[k].elements > desiredElements) {
                                            point1 = history[k];
                                            point2 = history[j];
                                        } else {
                                            continue;
                                        }
                                        
                                        // Используем бинарный поиск: берем среднее между двумя точками
                                        double midClmax = (point1.clmax + point2.clmax) / 2.0;
                                        
                                        // Но лучше использовать взвешенное среднее на основе желаемого количества элементов
                                        double weight1 = qAbs((double)point1.elements - desiredElements);
                                        double weight2 = qAbs((double)point2.elements - desiredElements);
                                        double totalWeight = weight1 + weight2;
                                        if (totalWeight > 0) {
                                            midClmax = (point2.clmax * weight1 + point1.clmax * weight2) / totalWeight;
                                        }
                                        
                                        clmax = midClmax;
                                        clmin = clmax * clminRatio;
                                        foundPair = true;
                                        outputText->append(QString("  🔄 Защита от зацикливания: используем среднее между clmax=%1 (эл.=%2) и clmax=%3 (эл.=%4) -> clmax=%5\n")
                                                          .arg(point2.clmax, 0, 'f', 4).arg(point2.elements)
                                                          .arg(point1.clmax, 0, 'f', 4).arg(point1.elements)
                                                          .arg(clmax, 0, 'f', 4));
                                        break;
                                    }
                                }
                                if (foundPair) break;
                            }
                            
                            if (!foundPair) {
                                // Если не нашли подходящую пару, используем более консервативную коррекцию
                                if (ratio > 1.0) {
                                    clmax = prevClmax * 1.15;  // Увеличиваем на 15%
                                } else {
                                    clmax = prevClmax * 0.85;  // Уменьшаем на 15%
                                }
                                clmin = clmax * clminRatio;
                                outputText->append(QString("  🔄 Защита от зацикливания: скорректирован clmax до %1\n")
                                                  .arg(clmax, 0, 'f', 4));
                            }
                        } else {
                            // Если истории недостаточно, используем простую коррекцию
                            if (ratio > 1.0) {
                                clmax = prevClmax * 1.15;
                            } else {
                                clmax = prevClmax * 0.85;
                            }
                            clmin = clmax * clminRatio;
                            outputText->append(QString("  🔄 Защита от зацикливания: скорректирован clmax до %1\n")
                                              .arg(clmax, 0, 'f', 4));
                        }
                        break;
                    }
                }
            }
            
            // Ограничения
            if (clmin < 0.01) clmin = 0.01;
            if (clmax < clmin * 1.05) clmax = clmin * 1.05;
            if (clmax > 1000.0) clmax = 1000.0;
            
            // Для очень грубых сеток убеждаемся, что clmin не слишком мал
            if (desiredElements < 30 && clmin / clmax < 0.7) {
                clmin = clmax * 0.7;
            }
            
            outputText->append(QString("Корректировка параметров: clmin=%1, clmax=%2 (clmin/clmax=%3)\n")
                              .arg(clmin, 0, 'f', 4).arg(clmax, 0, 'f', 4).arg(clmin / clmax, 0, 'f', 2));
        } else {
            // Последняя итерация - выводим информацию о неудаче и предлагаем лучшее найденное решение
            outputText->append(QString("⚠ Достигнуто максимальное количество итераций (%1)\n").arg(maxIterations));
            outputText->append(QString("   Текущее отклонение: %1% (желаемое: ±%2%)\n")
                              .arg(deviationPercent, 0, 'f', 1).arg(tolerance * 100.0, 0, 'f', 0));
            
            // Находим лучшее решение из истории (ближайшее к желаемому)
            if (history.size() > 0) {
                int bestElements = actualElements;
                double bestClmax = clmax;
                double bestClmin = clmin;
                double minDeviation = qAbs((double)actualElements - desiredElements);
                
                for (int i = 0; i < history.size(); i++) {
                    double deviation = qAbs((double)history[i].elements - desiredElements);
                    if (deviation < minDeviation) {
                        minDeviation = deviation;
                        bestElements = history[i].elements;
                        bestClmax = history[i].clmax;
                        bestClmin = history[i].clmin;
                    }
                }
                
                if (bestElements != actualElements) {
                    outputText->append(QString("\n📊 Лучшее найденное решение из истории:\n"));
                    outputText->append(QString("   Элементы: %1 (отклонение: %2%)\n")
                                      .arg(bestElements)
                                      .arg(((double)bestElements - desiredElements) / desiredElements * 100.0, 0, 'f', 1));
                    outputText->append(QString("   clmin=%1, clmax=%2\n").arg(bestClmin, 0, 'f', 4).arg(bestClmax, 0, 'f', 4));
                    
                    // Обновляем параметры на лучшее найденное значение
                    clmin = bestClmin;
                    clmax = bestClmax;
                    clminEdit->setValue(clmin);
                    clmaxEdit->setValue(clmax);
                }
            }
        }
    }
    
    if (!success) {
        // Проверяем, является ли результат приемлемым, даже если не достигли точной цели
        if (actualElements > 0 && errorMsg.isEmpty()) {
            double finalRatio = (double)actualElements / desiredElements;
            
            // Для очень малого количества элементов принимаем результат, если он разумен
            if (desiredElements < 20 && finalRatio >= 0.5 && finalRatio <= 3.0) {
                // Результат приемлем для очень грубой сетки
                outputText->append(QString("\n⚠ ВНИМАНИЕ: Не удалось достичь точного количества элементов\n"));
                outputText->append(QString("  Желаемое: %1 элементов\n").arg(desiredElements));
                outputText->append(QString("  Получено: %1 элементов (отклонение: %2%)\n")
                                  .arg(actualElements).arg((finalRatio - 1.0) * 100.0, 0, 'f', 1));
                outputText->append(QString("  Для очень грубых сеток это приемлемый результат.\n"));
                outputText->append(QString("  Gmsh может иметь ограничения геометрии, которые не позволяют создать более грубую сетку.\n\n"));
                
                // Принимаем результат как успех
                success = true;
                
                // Обновляем параметры в UI
                clminEdit->setValue(clmin);
                clmaxEdit->setValue(clmax);
            } else if (desiredElements < 50 && finalRatio >= 0.7 && finalRatio <= 2.5) {
                // Результат приемлем для грубой сетки
                outputText->append(QString("\n⚠ ВНИМАНИЕ: Не удалось достичь точного количества элементов\n"));
                outputText->append(QString("  Желаемое: %1 элементов\n").arg(desiredElements));
                outputText->append(QString("  Получено: %1 элементов (отклонение: %2%)\n")
                                  .arg(actualElements).arg((finalRatio - 1.0) * 100.0, 0, 'f', 1));
                outputText->append(QString("  Для грубой сетки это приемлемый результат.\n\n"));
                
                // Принимаем результат как успех
                success = true;
                
                // Обновляем параметры в UI
                clminEdit->setValue(clmin);
                clmaxEdit->setValue(clmax);
            } else {
                // Результат не приемлем - показываем ошибку
                QString fullError = QString("Не удалось сгенерировать сетку с желаемым количеством элементов:\n\n")
                                  .append(QString("Желаемое: %1\n").arg(desiredElements))
                                  .append(QString("Получено: %1 (отклонение: %2%)\n")
                                          .arg(actualElements).arg((finalRatio - 1.0) * 100.0, 0, 'f', 1))
                                  .append(QString("\nДостигнуто максимальное количество итераций (%1)\n\n").arg(maxIterations))
                                  .append("Попробуйте:\n")
                                  .append("- Увеличить желаемое количество элементов\n")
                                  .append("- Вручную скорректировать clmax и clmin\n")
                                  .append("- Проверить геометрию модели\n");
                QMessageBox::warning(this, "Предупреждение о генерации сетки", fullError);
                outputText->append(QString("\n⚠ ПРЕДУПРЕЖДЕНИЕ: Не удалось достичь желаемого количества элементов\n"));
                outputText->append(QString("  Желаемое: %1, Получено: %2 (отклонение: %3%)\n")
                                  .arg(desiredElements).arg(actualElements).arg((finalRatio - 1.0) * 100.0, 0, 'f', 1));
                
                // Все равно загружаем сетку, но с предупреждением
                success = true;
                clminEdit->setValue(clmin);
                clmaxEdit->setValue(clmax);
            }
        } else {
            // Реальная ошибка (не удалось сгенерировать сетку)
            QString fullError = QString("Не удалось сгенерировать сетку:\n\n")
                              .append(QString("%1\n\n").arg(errorMsg.isEmpty() ? "Достигнуто максимальное количество итераций" : errorMsg))
                              .append("Проверьте:\n")
                              .append("- Правильность пути к STEP файлу\n")
                              .append("- Что файл является валидным STEP файлом\n")
                              .append("- Права доступа к директории build\n")
                              .append("- Что Gmsh установлен: brew install gmsh\n");
            QMessageBox::critical(this, "Ошибка генерации сетки", fullError);
            outputText->append(QString("\n❌ ОШИБКА: %1\n").arg(errorMsg.isEmpty() ? "Превышено максимальное количество итераций" : errorMsg));
            outputText->repaint();
            return;
        }
    }
    
    // Генерируем файл смежности (если нужно)
    if (QFileInfo::exists(mshFile) && QFileInfo::exists(projectRoot + "/HyperMesh/generate_adjacency.py")) {
        QProcess adjProcess;
        adjProcess.setWorkingDirectory(projectRoot + "/HyperMesh");
        adjProcess.start("python3", QStringList() 
                        << (projectRoot + "/HyperMesh/generate_adjacency.py")
                        << mshFile << adjFile);
        adjProcess.waitForFinished(5000);
    }
    
    // Загружаем сетку в визуализатор
    if (QFileInfo::exists(nodeFile) && meshViewer) {
        outputText->append("\nЗагрузка сетки в визуализатор...\n");
        outputText->repaint();
        QApplication::processEvents();
        
        meshViewer->loadMesh(nodeFile, mshFile);
        int generatedNodes = meshViewer->getNodeCount();
        int generatedElementsFromViewer = meshViewer->getElementCount();
        
        outputText->append(QString("✓ Сетка загружена: узлов=%1, элементов=%2\n")
                          .arg(generatedNodes).arg(generatedElementsFromViewer));
        
        // Проверяем соответствие количества элементов
        if (generatedElementsFromViewer != actualElements) {
            outputText->append(QString("⚠ Предупреждение: количество элементов в визуализаторе (%1) не совпадает с подсчитанным (%2)\n")
                              .arg(generatedElementsFromViewer).arg(actualElements));
        }
    } else {
        outputText->append("⚠ Предупреждение: не удалось загрузить сетку в визуализатор\n");
    }
    
    outputText->repaint();
    
    if (!QFileInfo::exists(nodeFile)) {
        QMessageBox::critical(this, "Ошибка", 
                             QString("Файл сетки не найден: %1\n\nПроверьте вывод скрипта выше для деталей.").arg(nodeFile));
        return;
    }
    
    // Выводим финальную информацию о сгенерированной сетке
    outputText->append(QString("\n=== Результаты генерации ===\n")
                      .append(QString("  Желаемое количество элементов: %1\n").arg(desiredElements))
                      .append(QString("  Фактическое количество элементов: %1\n").arg(actualElements))
                      .append(QString("  Отклонение: %1%\n").arg(((double)actualElements / desiredElements - 1.0) * 100.0, 0, 'f', 1))
                      .append(QString("  Финальные параметры: clmin=%1, clmax=%2\n").arg(clmin, 0, 'f', 4).arg(clmax, 0, 'f', 4)));
    
    // Очищаем граничные условия при генерации новой сетки
    // Пользователь может задать новые условия после генерации
    fixedNodesU.clear();
    fixedNodesV.clear();
    loadedNodes.clear();
    nodeLoads.clear();
    fixedUListWidget->clear();
    fixedVListWidget->clear();
    loadedListWidget->clear();
    boundaryConditionsModifiedByUser = false;  // Сбрасываем флаг, так как сетка новая
    
    // Сбрасываем значения полей сил на 0
    loadFxEdit->setValue(0.0);
    loadFyEdit->setValue(0.0);
    
    onBoundaryConditionsChanged();
    
    outputText->append("✓ Генерация сетки завершена успешно\n");
}

void MainWindow::selectNodeById() {
    // Метод больше не используется, так как убрали поле ввода nodeIdEdit
    // Оставлен для совместимости
}

void MainWindow::onNodesSelected(const QSet<int> &nodeIds) {
    // Получаем общее количество выбранных узлов из визуализатора
    if (meshViewer) {
        QSet<int> allSelectedNodes = meshViewer->getSelectedNodes();
        int totalSelected = allSelectedNodes.size();
        
        // Показываем общее количество выбранных узлов
        QString message = QString("Выбрано узлов: %1").arg(totalSelected);
        
        // Если в последней операции добавили узлы, показываем информацию об этом
        if (!nodeIds.isEmpty() && nodeIds.size() <= 10) {
            QStringList nodeList;
            for (int nodeId : nodeIds) {
                nodeList.append(QString::number(nodeId));
            }
            message += QString(" (добавлено в этом выборе: %1)").arg(nodeList.join(", "));
        } else if (!nodeIds.isEmpty()) {
            message += QString(" (добавлено в этом выборе: %1)").arg(nodeIds.size());
        }
        
        outputText->append(message + "\n");
    } else if (!nodeIds.isEmpty()) {
        // Если визуализатор недоступен, показываем только последний выбор
        QString nodesStr = QString::number(nodeIds.size());
        if (nodeIds.size() <= 5) {
            QStringList nodeList;
            for (int nodeId : nodeIds) {
                nodeList.append(QString::number(nodeId));
            }
            nodesStr = nodeList.join(", ");
        }
        outputText->append(QString("Выбрано узлов: %1\n").arg(nodesStr));
    }
}

void MainWindow::updateSelectedNodesLabel() {
    // Метод больше не используется, так как убрали метку selectedNodesLabel
    // Оставлен для совместимости
}

void MainWindow::onBoundaryConditionsChanged() {
    QString bcText = QString("Закрепления: U_fixed=%1, V_fixed=%2")
                     .arg(fixedNodesU.size()).arg(fixedNodesV.size());
    boundaryConditionsLabel->setText(bcText);
    
    QString loadsText = QString("Нагрузки: %1 узлов")
                       .arg(loadedNodes.size());
    loadsLabel->setText(loadsText);
}

void MainWindow::onNodeClicked(int nodeId, const QPointF &coords) {
    // При одиночном клике просто выводим информацию о узле (не выделяем)
    outputText->append(QString("Узел %1: (%2, %3)\n").arg(nodeId).arg(coords.x(), 0, 'f', 2).arg(coords.y(), 0, 'f', 2));
}

void MainWindow::onNodeDoubleClicked(int nodeId, const QPointF &coords) {
    // Двойной клик - переключаем выделение узла (выделяем/снимаем выделение)
    if (!meshViewer) return;
    
    QSet<int> selected = meshViewer->getSelectedNodes();
    if (selected.contains(nodeId)) {
        // Узел уже выделен - убираем выделение
        selected.remove(nodeId);
        meshViewer->setSelectedNodes(selected);
        outputText->append(QString("Выделение с узла %1 снято\n").arg(nodeId));
    } else {
        // Узел не выделен - выделяем его
        selected.insert(nodeId);
        meshViewer->setSelectedNodes(selected);
        outputText->append(QString("Узел %1 выделен\n").arg(nodeId));
    }
    meshViewer->update();  // Обновляем визуализацию
}

void MainWindow::addFixedNode() {
    // Добавляем закрепление для выбранных узлов в визуализаторе
    if (!meshViewer) {
        QMessageBox::warning(this, "Ошибка", "Сначала загрузите сетку!");
        return;
    }
    
    QSet<int> selected = meshViewer->getSelectedNodes();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите узлы в 3D модели!");
        return;
    }
    
    int addedCount = 0;
    for (int nodeId : selected) {
        QString nodeStr = QString::number(nodeId);
        int type = constraintTypeCombo->currentIndex();
        
        // Проверяем, не является ли этот узел узлом с нагрузкой
        if (loadedNodes.contains(nodeStr)) {
            outputText->append(QString("⚠ Узел %1 имеет нагрузку, пропускаем закрепление\n").arg(nodeId));
            continue;
        }
        
        if (type == 0 && !fixedNodesU.contains(nodeStr)) {  // Закрепление по U
            fixedNodesU.append(nodeStr);
            fixedUListWidget->addItem(QString("Узел %1").arg(nodeId));
            addedCount++;
        } else if (type == 1 && !fixedNodesV.contains(nodeStr)) {  // Закрепление по V
            fixedNodesV.append(nodeStr);
            fixedVListWidget->addItem(QString("Узел %1").arg(nodeId));
            addedCount++;
        }
    }
    
    if (addedCount > 0) {
        boundaryConditionsModifiedByUser = true;  // Пользователь изменил граничные условия
        updateMeshViewerBoundaryConditions();
        onBoundaryConditionsChanged();
        outputText->append(QString("✓ Добавлено закреплений: %1\n").arg(addedCount));
    }
}

void MainWindow::addLoadNode() {
    // Добавляем нагрузку для выбранных узлов в визуализаторе
    if (!meshViewer) {
        QMessageBox::warning(this, "Ошибка", "Сначала загрузите сетку!");
        return;
    }
    
    QSet<int> selected = meshViewer->getSelectedNodes();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите узлы в 3D модели!");
        return;
    }
    
    double fx = loadFxEdit->value();
    double fy = loadFyEdit->value();
    
    if (fx == 0.0 && fy == 0.0) {
        QMessageBox::warning(this, "Ошибка", "Задайте ненулевые значения сил!");
        return;
    }
    
    int addedCount = 0;
    int skippedCount = 0;
    for (int nodeId : selected) {
        QString nodeStr = QString::number(nodeId);
        
        // Проверяем, не закреплен ли этот узел
        // Если узел закреплен, пропускаем его с предупреждением (не удаляем закрепление автоматически)
        if (fixedNodesU.contains(nodeStr)) {
            outputText->append(QString("⚠ Узел %1 закреплен по U, пропускаем добавление нагрузки\n").arg(nodeId));
            skippedCount++;
            continue;
        }
        if (fixedNodesV.contains(nodeStr)) {
            outputText->append(QString("⚠ Узел %1 закреплен по V, пропускаем добавление нагрузки\n").arg(nodeId));
            skippedCount++;
            continue;
        }
        
        if (!loadedNodes.contains(nodeStr)) {
            loadedNodes.append(nodeStr);
            nodeLoads[nodeStr] = QPair<double, double>(fx, fy);
            loadedListWidget->addItem(QString("Узел %1: Fx=%2, Fy=%3").arg(nodeId).arg(fx, 0, 'f', 2).arg(fy, 0, 'f', 2));
            addedCount++;
            boundaryConditionsModifiedByUser = true;  // Пользователь изменил граничные условия
        } else {
            // Обновляем нагрузку
            nodeLoads[nodeStr] = QPair<double, double>(fx, fy);
            boundaryConditionsModifiedByUser = true;  // Пользователь изменил граничные условия
            for (int i = 0; i < loadedListWidget->count(); i++) {
                if (loadedListWidget->item(i)->text().startsWith(QString("Узел %1:").arg(nodeId))) {
                    loadedListWidget->item(i)->setText(QString("Узел %1: Fx=%2, Fy=%3").arg(nodeId).arg(fx, 0, 'f', 2).arg(fy, 0, 'f', 2));
                    break;
                }
            }
        }
    }
    
    if (addedCount > 0) {
        boundaryConditionsModifiedByUser = true;  // Пользователь изменил граничные условия
        updateMeshViewerBoundaryConditions();
        onBoundaryConditionsChanged();
        outputText->append(QString("✓ Добавлено нагрузок: %1\n").arg(addedCount));
    }
    if (skippedCount > 0) {
        outputText->append(QString("⚠ Пропущено узлов (закреплены): %1\n").arg(skippedCount));
    }
}

void MainWindow::removeFixedNode() {
    bool removed = false;
    QListWidgetItem *item = fixedUListWidget->currentItem();
    if (item) {
        QString text = item->text();
        int nodeId = text.split(" ")[1].toInt();
        fixedNodesU.removeAll(QString::number(nodeId));
        delete item;
        removed = true;
    }
    
    item = fixedVListWidget->currentItem();
    if (item) {
        QString text = item->text();
        int nodeId = text.split(" ")[1].toInt();
        fixedNodesV.removeAll(QString::number(nodeId));
        delete item;
        removed = true;
    }
    
    if (removed) {
        boundaryConditionsModifiedByUser = true;  // Пользователь изменил граничные условия
        updateMeshViewerBoundaryConditions();
        onBoundaryConditionsChanged();
    }
}

void MainWindow::removeLoadNode() {
    QListWidgetItem *item = loadedListWidget->currentItem();
    if (item) {
        QString text = item->text();
        int nodeId = text.split(" ")[1].split(":")[0].toInt();
        QString nodeStr = QString::number(nodeId);
        loadedNodes.removeAll(nodeStr);
        nodeLoads.remove(nodeStr);
        delete item;
        boundaryConditionsModifiedByUser = true;  // Пользователь изменил граничные условия
        updateMeshViewerBoundaryConditions();
        onBoundaryConditionsChanged();
    }
}

void MainWindow::clearAllLoads() {
    // Очищаем все нагрузки
    if (!loadedNodes.isEmpty()) {
        boundaryConditionsModifiedByUser = true;  // Пользователь изменил граничные условия
    }
    loadedNodes.clear();
    nodeLoads.clear();
    loadedListWidget->clear();
    
    // Сбрасываем значения полей на 0
    loadFxEdit->setValue(0.0);
    loadFyEdit->setValue(0.0);
    
    updateMeshViewerBoundaryConditions();
    onBoundaryConditionsChanged();
    outputText->append("Все нагрузки очищены\n");
}

void MainWindow::updateMeshViewerBoundaryConditions() {
    if (!meshViewer) return;
    
    QSet<int> fixedU, fixedV;
    for (const QString &n : fixedNodesU) {
        fixedU.insert(n.toInt());
    }
    for (const QString &n : fixedNodesV) {
        fixedV.insert(n.toInt());
    }
    
    meshViewer->setFixedNodesU(fixedU);
    meshViewer->setFixedNodesV(fixedV);
    
    QMap<int, QPair<double, double>> loads;
    for (auto it = nodeLoads.begin(); it != nodeLoads.end(); ++it) {
        loads.insert(it.key().toInt(), it.value());
    }
    meshViewer->setLoadNodes(loads);
}

bool MainWindow::saveBoundaryConditionsToFile() {
    QString nodeFile = projectRoot + "/build/node.txt";
    
    // Отладочный вывод
    outputText->append(QString("Отладка: Сохранение граничных условий в файл: %1\n").arg(nodeFile));
    outputText->append(QString("Отладка: Граничные условия в GUI: U_fixed=%1, V_fixed=%2, Loaded=%3\n")
                       .arg(fixedNodesU.size()).arg(fixedNodesV.size()).arg(loadedNodes.size()));
    
    QFile file(nodeFile);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qDebug() << "Ошибка: не удалось открыть файл" << nodeFile;
        outputText->append(QString("✗ Ошибка: не удалось открыть файл %1\n").arg(nodeFile));
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
        qDebug() << "Ошибка: файл пуст";
        return false;
    }
    
    file.resize(0);  // Очищаем файл
    file.seek(0);
    QTextStream out(&file);
    
    int numNodes = lines[0].toInt();
    out << numNodes << "\n";
    
    // Создаем множества для быстрого поиска по ID узлов из GUI
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
    
    // Создаем маппинг: ID узла из GUI -> порядковый номер в файле
    // Используем координаты узлов для точного сопоставления
    QMap<int, int> nodeIdToFileIndex;  // GUI nodeId -> file index (1-based)
    
    // Создаем обратный маппинг: координаты (x, y) -> порядковый номер в файле
    QMap<QString, int> coordsToFileIndex;  // "x,y" -> file index (1-based)
    const double coordTolerance = 1e-9;
    
    // Читаем координаты узлов из файла и создаем маппинг
    for (int i = 1; i <= numNodes && i < lines.size(); i++) {
        QString line = lines[i];
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        
        if (parts.size() >= 2) {
            double x = parts[0].toDouble();
            double y = parts[1].toDouble();
            // Используем точный формат для ключа координат
            QString coordKey = QString("%1,%2").arg(x, 0, 'g', 15).arg(y, 0, 'g', 15);
            coordsToFileIndex[coordKey] = i;
        }
    }
    
    // Создаем маппинг GUI nodeId -> file index, используя координаты из meshViewer
    if (meshViewer) {
        // Получаем координаты узлов из meshViewer и сопоставляем с координатами в файле
        QSet<int> allGuiNodeIds;
        for (int nodeId : fixedUSet) allGuiNodeIds.insert(nodeId);
        for (int nodeId : fixedVSet) allGuiNodeIds.insert(nodeId);
        for (int nodeId : loadedSet) allGuiNodeIds.insert(nodeId);
        
        for (int guiNodeId : allGuiNodeIds) {
            // Получаем координаты узла из meshViewer
            QPair<double, double> coords2D = meshViewer->getNodeCoords2D(guiNodeId);
            QVector3D coords3D = meshViewer->getNodeCoords3D(guiNodeId);
            
            double x = 0.0, y = 0.0;
            if (coords2D.first != 0.0 || coords2D.second != 0.0) {
                // 2D модель
                x = coords2D.first;
                y = coords2D.second;
            } else if (coords3D.x() != 0.0 || coords3D.y() != 0.0) {
                // 3D модель (используем только x, y)
                x = coords3D.x();
                y = coords3D.y();
            }
            
            if (x != 0.0 || y != 0.0) {
                // Ищем узел в файле с такими же координатами
                QString coordKey = QString("%1,%2").arg(x, 0, 'g', 15).arg(y, 0, 'g', 15);
                if (coordsToFileIndex.contains(coordKey)) {
                    nodeIdToFileIndex[guiNodeId] = coordsToFileIndex[coordKey];
                } else {
                    // Если точного совпадения нет, ищем ближайший узел
                    int closestFileIndex = -1;
                    double minDist = 1e10;
                    for (auto it = coordsToFileIndex.begin(); it != coordsToFileIndex.end(); ++it) {
                        QStringList coords = it.key().split(",");
                        if (coords.size() == 2) {
                            double fx = coords[0].toDouble();
                            double fy = coords[1].toDouble();
                            double dist = sqrt((x - fx) * (x - fx) + (y - fy) * (y - fy));
                            if (dist < minDist && dist < coordTolerance * 1000) {  // Увеличиваем допуск
                                minDist = dist;
                                closestFileIndex = it.value();
                            }
                        }
                    }
                    if (closestFileIndex > 0) {
                        nodeIdToFileIndex[guiNodeId] = closestFileIndex;
                        qDebug() << "Найден ближайший узел для GUI nodeId" << guiNodeId 
                                 << "-> file index" << closestFileIndex << "dist=" << minDist;
                    }
                }
            }
        }
    }
    
    // Если маппинг не создан, используем предположение, что ID узлов соответствуют порядковым номерам
    // (это работает, если Gmsh генерирует узлы с последовательными ID, начиная с 1)
    if (nodeIdToFileIndex.isEmpty()) {
        qDebug() << "Маппинг не создан, используем предположение: ID узла = порядковый номер";
        for (int guiNodeId : fixedUSet) nodeIdToFileIndex[guiNodeId] = guiNodeId;
        for (int guiNodeId : fixedVSet) nodeIdToFileIndex[guiNodeId] = guiNodeId;
        for (int guiNodeId : loadedSet) nodeIdToFileIndex[guiNodeId] = guiNodeId;
    }
    
    // Создаем множества порядковых номеров узлов в файле
    QSet<int> fixedUFileIndices, fixedVFileIndices, loadedFileIndices;
    for (int guiNodeId : fixedUSet) {
        if (nodeIdToFileIndex.contains(guiNodeId)) {
            fixedUFileIndices.insert(nodeIdToFileIndex[guiNodeId]);
        }
    }
    for (int guiNodeId : fixedVSet) {
        if (nodeIdToFileIndex.contains(guiNodeId)) {
            fixedVFileIndices.insert(nodeIdToFileIndex[guiNodeId]);
        }
    }
    for (int guiNodeId : loadedSet) {
        if (nodeIdToFileIndex.contains(guiNodeId)) {
            loadedFileIndices.insert(nodeIdToFileIndex[guiNodeId]);
        }
    }
    
    // Записываем узлы с граничными условиями
    for (int i = 1; i <= numNodes && i < lines.size(); i++) {
        QString line = lines[i];
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        
        if (parts.size() >= 3) {
            double x = parts[0].toDouble();
            double y = parts[1].toDouble();
            double z = parts[2].toDouble();
            
            // Определяем флаги граничных условий по порядковому номеру в файле
            int fileIndex = i;  // Порядковый номер узла в файле
            
            // Если граничные условия заданы в GUI, используем их
            // Иначе используем существующие значения из файла (если они есть)
            int u_flag, v_flag, load_flag;
            
            if (parts.size() >= 6) {
                // В файле уже есть граничные условия, используем их как базовые
                u_flag = parts[3].toInt();
                v_flag = parts[4].toInt();
                load_flag = parts[5].toInt();
            } else {
                // В файле нет граничных условий, используем значения по умолчанию
                u_flag = 1;
                v_flag = 1;
                load_flag = 0;
            }
            
            // Если узел задан в GUI как закрепленный, перезаписываем флаги
            if (fixedUFileIndices.contains(fileIndex)) {
                u_flag = 0;
            }
            if (fixedVFileIndices.contains(fileIndex)) {
                v_flag = 0;
            }
            if (loadedFileIndices.contains(fileIndex)) {
                load_flag = 100;
            }
            
            // Записываем: x y z u_flag v_flag load_flag
            out << QString::number(x, 'g', 15) << " "
                << QString::number(y, 'g', 15) << " "
                << QString::number(z, 'g', 15) << " "
                << u_flag << " "
                << v_flag << " "
                << load_flag << "\n";
        } else {
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
    // Используем маппинг GUI nodeId -> file index для правильного сохранения
    QString loadsFile = QFileInfo(nodeFile).absolutePath() + "/loads.txt";
    QFile loadsFileHandle(loadsFile);
    if (loadsFileHandle.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream loadsOut(&loadsFileHandle);
        int loadsSaved = 0;
        for (auto it = nodeLoads.begin(); it != nodeLoads.end(); ++it) {
            int guiNodeId = it.key().toInt();
            double fx = it.value().first;
            double fy = it.value().second;
            
            // Используем маппинг для преобразования GUI nodeId в file index
            if (nodeIdToFileIndex.contains(guiNodeId)) {
                int fileIndex = nodeIdToFileIndex[guiNodeId];
                loadsOut << fileIndex << " " << QString::number(fx, 'g', 15) << " " << QString::number(fy, 'g', 15) << "\n";
                loadsSaved++;
            } else {
                // Если маппинг не найден, используем ID напрямую (предполагаем, что ID = file index)
                loadsOut << guiNodeId << " " << QString::number(fx, 'g', 15) << " " << QString::number(fy, 'g', 15) << "\n";
                loadsSaved++;
                qDebug() << "Предупреждение: маппинг не найден для GUI nodeId" << guiNodeId << ", используем ID напрямую";
            }
        }
        loadsFileHandle.close();
        qDebug() << "Сохранено" << loadsSaved << "нагрузок в файл" << loadsFile;
    } else {
        qDebug() << "Ошибка: не удалось открыть файл нагрузок" << loadsFile;
    }
    
    qDebug() << "Граничные условия сохранены: U_fixed=" << fixedUFileIndices.size() 
             << "узлов (из" << fixedNodesU.size() << "в GUI), V_fixed=" << fixedVFileIndices.size()
             << "узлов (из" << fixedNodesV.size() << "в GUI), Loaded=" << loadedFileIndices.size()
             << "узлов (из" << loadedNodes.size() << "в GUI)";
    
    // Выводим информацию в outputText для отладки
    outputText->append(QString("Отладка: Граничные условия сохранены: U_fixed=%1 узлов (из %2 в GUI), V_fixed=%3 узлов (из %4 в GUI), Loaded=%5 узлов (из %6 в GUI)\n")
                       .arg(fixedUFileIndices.size()).arg(fixedNodesU.size())
                       .arg(fixedVFileIndices.size()).arg(fixedNodesV.size())
                       .arg(loadedFileIndices.size()).arg(loadedNodes.size()));
    
    if (nodeIdToFileIndex.size() < fixedUSet.size() + fixedVSet.size() + loadedSet.size()) {
        qDebug() << "Предупреждение: не все узлы из GUI были сопоставлены с узлами в файле!";
        qDebug() << "Сопоставлено" << nodeIdToFileIndex.size() << "узлов из" 
                 << (fixedUSet.size() + fixedVSet.size() + loadedSet.size()) << "общих";
        outputText->append(QString("⚠ Предупреждение: не все узлы из GUI были сопоставлены с узлами в файле! Сопоставлено %1 узлов из %2 общих\n")
                           .arg(nodeIdToFileIndex.size())
                           .arg(fixedUSet.size() + fixedVSet.size() + loadedSet.size()));
    }
    
    // Проверяем, что хотя бы некоторые граничные условия были сохранены
    if (fixedUFileIndices.isEmpty() && fixedVFileIndices.isEmpty() && loadedFileIndices.isEmpty()) {
        if (!fixedNodesU.isEmpty() || !fixedNodesV.isEmpty() || !loadedNodes.isEmpty()) {
            outputText->append(QString("⚠ Ошибка: Граничные условия заданы в GUI, но не были сохранены в файл! Возможно, проблема с маппингом узлов.\n"));
            qDebug() << "Ошибка: Граничные условия заданы в GUI, но не были сохранены в файл!";
        }
    }
    
    return true;
}

bool MainWindow::loadBoundaryConditionsFromFile() {
    QString nodeFile = projectRoot + "/build/node.txt";
    QString loadsFile = projectRoot + "/build/loads.txt";
    
    if (!QFileInfo::exists(nodeFile)) {
        return false;
    }
    
    // Очищаем текущие граничные условия
    fixedNodesU.clear();
    fixedNodesV.clear();
    loadedNodes.clear();
    nodeLoads.clear();
    fixedUListWidget->clear();
    fixedVListWidget->clear();
    loadedListWidget->clear();
    
    // Устанавливаем флаг, что граничные условия загружены из файла (не изменены пользователем)
    boundaryConditionsModifiedByUser = false;
    
    // Читаем граничные условия из node.txt
    QFile file(nodeFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream in(&file);
    QString firstLine = in.readLine();
    int numNodes = firstLine.toInt();
    
    QSet<int> loadedNodesSet;
    
    for (int i = 1; i <= numNodes && !in.atEnd(); i++) {
        QString line = in.readLine();
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        
        if (parts.size() >= 6) {
            // Формат: x y z u_flag v_flag load_flag
            int u_flag = parts[3].toInt();
            int v_flag = parts[4].toInt();
            int load_flag = parts[5].toInt();
            
            if (u_flag == 0) {
                fixedNodesU.append(QString::number(i));
                fixedUListWidget->addItem(QString("Узел %1").arg(i));
            }
            if (v_flag == 0) {
                fixedNodesV.append(QString::number(i));
                fixedVListWidget->addItem(QString("Узел %1").arg(i));
            }
            if (load_flag == 100) {
                loadedNodesSet.insert(i);
            }
        }
    }
    
    file.close();
    
    // Загружаем значения нагрузок из loads.txt
    // НЕ загружаем автоматически - пользователь должен явно добавить нагрузки через интерфейс
    // Это предотвращает загрузку старых/некорректных значений
    // Если нужно загрузить из файла, это можно сделать через отдельную кнопку
    
    // Всегда сбрасываем поля Fx и Fy на 0 после загрузки граничных условий
    // Это гарантирует, что поля будут пустыми, даже если в файле были старые значения
    if (loadFxEdit) {
        loadFxEdit->setValue(0.0);
    }
    if (loadFyEdit) {
        loadFyEdit->setValue(0.0);
    }
    
    onBoundaryConditionsChanged();
    outputText->append(QString("Загружены граничные условия: U_fixed=%1, V_fixed=%2, нагрузок=%3\n")
                      .arg(fixedNodesU.size()).arg(fixedNodesV.size()).arg(loadedNodes.size()));
    
    return true;
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
    
    // Проверка наличия закреплений
    if (fixedNodesU.isEmpty() && fixedNodesV.isEmpty()) {
        int ret = QMessageBox::warning(this, "Предупреждение", 
            "Не задано ни одного закрепления! Это может привести к ошибке расчета.\nПродолжить?",
            QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::No) {
            return;
        }
    }
    
    // НЕ сохраняем граничные условия здесь, так как сетка еще не сгенерирована
    // Граничные условия будут сохранены ПОСЛЕ генерации сетки в processFinished()
    
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
        outputText->append("\n=== Генерация сетки завершена успешно ===\n");
        
        QString meshScript = projectRoot + "/HyperMesh/generate_mesh.sh";
        if (process->program().contains("bash") && process->arguments().contains(meshScript)) {
            // Сетка сгенерирована, загружаем её в визуализатор
            QString nodeFile = projectRoot + "/build/node.txt";
            QString mshFile = projectRoot + "/build/HyperMesh.msh";
            
            if (QFileInfo::exists(nodeFile) && meshViewer) {
                outputText->append("Загрузка сетки в визуализатор...\n");
                meshViewer->loadMesh(nodeFile, mshFile);
                outputText->append(QString("✓ Сетка загружена: узлов=%1, элементов=%2\n")
                                  .arg(meshViewer->getNodeCount()).arg(meshViewer->getElementCount()));
                
                // Загружаем граничные условия из файла (но НЕ устанавливаем флаг modifiedByUser)
                // Это позволяет использовать автоматически определенные граничные условия
                loadBoundaryConditionsFromFile();
            }
            
            // Сохраняем граничные условия ПЕРЕД запуском расчета
            // Но только если пользователь ИЗМЕНИЛ граничные условия в GUI после загрузки из файла
            // Иначе используем граничные условия из файла node.txt (которые были автоматически определены)
            if (QFileInfo::exists(nodeFile)) {
                // Проверяем, изменил ли пользователь граничные условия в GUI после загрузки из файла
                outputText->append(QString("Отладка: boundaryConditionsModifiedByUser=%1\n")
                                  .arg(boundaryConditionsModifiedByUser ? "true" : "false"));
                outputText->append(QString("Отладка: Граничные условия в GUI: U_fixed=%1, V_fixed=%2, Loaded=%3\n")
                                  .arg(fixedNodesU.size()).arg(fixedNodesV.size()).arg(loadedNodes.size()));
                
                if (boundaryConditionsModifiedByUser) {
                    outputText->append("Сохранение граничных условий в файл...\n");
                    if (!saveBoundaryConditionsToFile()) {
                        QMessageBox::warning(this, "Предупреждение", 
                            "Не удалось сохранить граничные условия в файл!\nРасчет может быть некорректным.");
                        outputText->append("⚠ Ошибка: не удалось сохранить граничные условия\n");
                    } else {
                        outputText->append("✓ Граничные условия сохранены\n");
                    }
                } else {
                    outputText->append("Используются граничные условия из файла (автоматически определены)\n");
                }
            } else {
                QMessageBox::warning(this, "Ошибка", 
                    "Файл сетки не найден после генерации: " + nodeFile);
                return;
            }
            
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
                // Функция для преобразования ANSI цветовых кодов в HTML
                auto ansiToHtml = [](const QString& text) -> QString {
                    QString result = text;
                    // Заменяем ANSI коды на HTML теги
                    result.replace("\033[32m", "<font color='green'>");  // зеленый
                    result.replace("\033[31m", "<font color='red'>");    // красный
                    result.replace("\033[33m", "<font color='orange'>"); // желтый/оранжевый
                    result.replace("\033[0m", "</font>");                // сброс
                    // Заменяем переносы строк на <br> для HTML
                    result.replace("\n", "<br>");
                    return result;
                };
                
                connect(modalProcess, &QProcess::readyReadStandardOutput, [this, modalProcess, ansiToHtml]() {
                    QString output = modalProcess->readAllStandardOutput();
                    outputText->append(ansiToHtml(output));
                    // Прокручиваем вниз
                    QTextCursor cursor = outputText->textCursor();
                    cursor.movePosition(QTextCursor::End);
                    outputText->setTextCursor(cursor);
                });
                connect(modalProcess, &QProcess::readyReadStandardError, [this, modalProcess, ansiToHtml]() {
                    QString error = modalProcess->readAllStandardError();
                    outputText->append(ansiToHtml(error));
                    // Прокручиваем вниз
                    QTextCursor cursor = outputText->textCursor();
                    cursor.movePosition(QTextCursor::End);
                    outputText->setTextCursor(cursor);
                });
                
                QString modalExecutable = projectRoot + "/fem-module/build/3d";
                if (!QFileInfo::exists(modalExecutable)) {
                    outputText->append(QString("<font color='red'>Ошибка: Модальный модуль не найден: %1</font><br>").arg(modalExecutable));
                    QMessageBox::warning(this, "Предупреждение", "Модальный модуль не найден. Пожалуйста, соберите его:\ncd fem-module/3D && make");
                    runBtn->setEnabled(true);
                    return;
                }
                
                // Устанавливаем рабочую директорию для модального анализа
                // Результаты будут сохранены в fem-module/build/result.txt
                modalProcess->setWorkingDirectory(projectRoot + "/fem-module/build");
                
                // Преобразуем путь к файлу сетки в абсолютный путь
                QFileInfo meshFileInfo(meshFile);
                QString absoluteMeshFile = meshFileInfo.absoluteFilePath();
                
                outputText->append(QString("Запуск модального модуля: %1 %2 %3 %4 %5<br>")
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
        // Для модального анализа читаем с рабочего стола
        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        femResultFile = desktopPath + "/Modal_result.txt";
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


