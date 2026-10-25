#include "mainwindow.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QDateTime>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QComboBox>
#include <QScrollArea>
#include <QPainter>
#include <QPixmap>
#include <QThread>
#include <QDebug>

// Конструктор: открывает БД, создаёт менеджер портов и строит окна
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("Управление сборкой");
    resize(1000, 850);

    QString dbPath = QApplication::applicationDirPath() + "/assemblies.db";
    QString dbErr;
    if (!m_db.open(dbPath, dbErr)) QMessageBox::critical(this, "Ошибка БД", dbErr);

    m_links = new LinkManager(this);
    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    buildStartWindow();
    buildWorkWindow();

    m_stack->addWidget(startPage);
    m_stack->addWidget(workPage);
    m_stack->setCurrentWidget(startPage);

    connect(m_links, &LinkManager::logMessage, this, [this](const QString& msg) { logMessage(msg); });
    connect(m_links, &LinkManager::discoveryFinished, this, &MainWindow::onDiscoveryFinished);
    connect(m_links, &LinkManager::statusUpdated, this, &MainWindow::onMkStatusUpdated);
    connect(m_links, &LinkManager::axisUpdated, this, &MainWindow::onAxisUpdated);
}

// Деструктор: закрывает БД
MainWindow::~MainWindow() { m_db.close(); }

// Функция создания новой записи в БД
bool MainWindow::createNewSessionRecord()
{
    if (steps.isEmpty() || m_instructionNumber == 0) return false;

    QString errMsg;
    int nextSeq = m_db.getNextSequenceNumber(m_instructionNumber, errMsg);
    QString serialNumber = QString("%1-%2").arg(m_instructionNumber, 3, 10, QChar('0')).arg(nextSeq, 4, 10, QChar('0'));

    AssemblyRecord rec;
    rec.serialNumber = serialNumber; rec.operatorName = editOperator->text().trimmed();
    rec.instructionFile = fileName->text(); rec.instructionNumber = m_instructionNumber; rec.notes = "";
    rec.frameType = cbFrameType->currentText(); rec.motorSpecs = cbMotorSpecs->currentText();
    rec.escType = cbEscType->currentText(); rec.pdbType = cbPdbType->currentText();
    rec.fcType = cbFcType->currentText(); rec.rxType = cbRxType->currentText();
    rec.vtxType = cbVtxType->currentText(); rec.vtxPower = cbVtxPower->currentText();
    rec.cameraSpecs = cbCameraSpecs->currentText();

    m_currentRecordId = m_db.insertRecord(rec, errMsg);
    if (m_currentRecordId < 0) { QMessageBox::warning(this, "Ошибка БД", errMsg); return false; }

    editSerial->setText(serialNumber); lblSerial->setText("Серийный номер: " + serialNumber);
    return true;
}

// Функция обновления выпадающих списков комплектующих
void MainWindow::refreshComboBoxes()
{
    auto updateCombo = [](QComboBox* cb, const QStringList& items) {
        QString currentText = cb->currentText(); cb->clear(); cb->addItem(""); cb->addItems(items); cb->setCurrentText(currentText);
        };
    updateCombo(cbMotorSpecs, m_db.getUniqueValues("motor_specs")); updateCombo(cbEscType, m_db.getUniqueValues("esc_type"));
    updateCombo(cbPdbType, m_db.getUniqueValues("pdb_type")); updateCombo(cbFcType, m_db.getUniqueValues("fc_type"));
    updateCombo(cbRxType, m_db.getUniqueValues("rx_type")); updateCombo(cbVtxType, m_db.getUniqueValues("vtx_type"));
    updateCombo(cbVtxPower, m_db.getUniqueValues("vtx_power")); updateCombo(cbCameraSpecs, m_db.getUniqueValues("camera_specs"));
}

// Функция обновления доступности кнопок действий по текущему шагу
void MainWindow::updateStepUI()
{
    if (currentStep >= steps.size()) return;
    InstructionStep& step = steps[currentStep]; QString text = step.text.toLower();
    if (btnTighten) btnTighten->setEnabled(false);
    if (btnSticker) btnSticker->setEnabled(false);
    if (step.type == StepType::Text) {
        if (text.contains("затяжка") && btnTighten) btnTighten->setEnabled(true);
        else if ((text.contains("наклейк") || text.contains("маркировк")) && btnSticker) btnSticker->setEnabled(true);
    }
}

// Функция построения стартового окна
void MainWindow::buildStartWindow()
{
    startPage = new QWidget; auto* scrollArea = new QScrollArea; scrollArea->setWidgetResizable(true);
    auto* contentWidget = new QWidget; auto* main = new QVBoxLayout(contentWidget); main->setContentsMargins(20, 20, 20, 20);
    auto* title = new QLabel("Начало работы"); QFont f = title->font(); f.setPointSize(18); f.setBold(true); title->setFont(f); title->setAlignment(Qt::AlignCenter);

    editOperator = new QLineEdit; editOperator->setPlaceholderText("Иванов");
    editSerial = new QLineEdit; editSerial->setReadOnly(true); editSerial->setPlaceholderText("загрузите инструкцию"); editSerial->setStyleSheet("background-color: #F5F5F5; color: #333; font-weight: bold;");

    btnLoad = new QPushButton("Загрузить инструкцию…"); fileName = new QLabel("файл не выбран"); fileName->setStyleSheet("color: gray;");
    startStatus = new QLabel; startStatus->setStyleSheet("color: #c62828;");

    auto* form = new QFormLayout; form->setSpacing(12); form->setLabelAlignment(Qt::AlignRight);
    form->addRow("Фамилия оператора:", editOperator);
    auto* fileRow = new QHBoxLayout; fileRow->addWidget(btnLoad); fileRow->addWidget(fileName, 1); form->addRow("Инструкция:", fileRow);
    form->addRow("Серийный номер:", editSerial);

    auto* compBox = new QGroupBox("Комплектующие БПЛА (необязательно)");
    compBox->setStyleSheet("QGroupBox { border:1px solid #90A4AE; border-radius:6px; margin-top:8px; padding:10px; background:#FAFAFA; } QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; font-weight:bold; }");
    auto makeCombo = [](const QStringList& items) { auto* cb = new QComboBox; cb->setEditable(true); cb->addItem(""); cb->addItems(items); return cb; };

    cbFrameType = makeCombo({ "1", "2" }); cbMotorSpecs = makeCombo(m_db.getUniqueValues("motor_specs")); cbEscType = makeCombo(m_db.getUniqueValues("esc_type"));
    cbPdbType = makeCombo(m_db.getUniqueValues("pdb_type")); cbFcType = makeCombo(m_db.getUniqueValues("fc_type")); cbRxType = makeCombo(m_db.getUniqueValues("rx_type"));
    cbVtxType = makeCombo(m_db.getUniqueValues("vtx_type")); cbVtxPower = makeCombo(m_db.getUniqueValues("vtx_power")); cbCameraSpecs = makeCombo(m_db.getUniqueValues("camera_specs"));

    auto* compForm = new QFormLayout;
    compForm->addRow("Тип рамы (1/2):", cbFrameType); compForm->addRow("Двигатели:", cbMotorSpecs); compForm->addRow("ESC:", cbEscType);
    compForm->addRow("PDB:", cbPdbType); compForm->addRow("Полетный контроллер:", cbFcType); compForm->addRow("Приемник:", cbRxType);
    compForm->addRow("Видеопередатчик:", cbVtxType); compForm->addRow("Мощность VTX:", cbVtxPower); compForm->addRow("Камера:", cbCameraSpecs);
    compBox->setLayout(compForm);

    auto* equipBox = new QGroupBox("Оборудование (МК)");
    equipBox->setStyleSheet("QGroupBox { border:1px solid #90A4AE; border-radius:6px; margin-top:8px; padding:10px; background:#FAFAFA; } QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; font-weight:bold; }");
    auto* eqLayout = new QGridLayout(equipBox);
    comboPort1 = new QComboBox; comboPort2 = new QComboBox; comboPort3 = new QComboBox;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) { comboPort1->addItem(info.portName()); comboPort2->addItem(info.portName()); comboPort3->addItem(info.portName()); }

    btnAutoDiscover = new QPushButton("Авто-поиск МК"); btnConnectAll = new QPushButton("Подключить всё");
    eqLayout->addWidget(new QLabel("МК1 (X, Y):"), 0, 0); eqLayout->addWidget(comboPort1, 0, 1);
    eqLayout->addWidget(new QLabel("МК2 (Z):"), 0, 2); eqLayout->addWidget(comboPort2, 0, 3);
    eqLayout->addWidget(new QLabel("МК3:"), 1, 0); eqLayout->addWidget(comboPort3, 1, 1);
    eqLayout->addWidget(btnAutoDiscover, 2, 0, 1, 2); eqLayout->addWidget(btnConnectAll, 2, 2, 1, 2);

    btnViewDb = new QPushButton("📁 Просмотр базы данных"); btnViewDb->setStyleSheet("background:#607D8B; color:white; padding:6px; font-weight:bold;"); eqLayout->addWidget(btnViewDb, 3, 0, 1, 4);

    btnStart = new QPushButton("▶   НАЧАТЬ РАБОТУ"); btnStart->setMinimumHeight(50); btnStart->setStyleSheet("QPushButton { background:#1976D2; color:white; font-size:16px; font-weight:bold; border-radius:6px; } QPushButton:hover { background:#1565C0; }");

    main->addWidget(title); main->addSpacing(10); main->addLayout(form); main->addSpacing(10);
    main->addWidget(compBox); main->addSpacing(10); main->addWidget(equipBox); main->addSpacing(15);
    main->addWidget(btnStart); main->addWidget(startStatus); main->addStretch();

    contentWidget->setLayout(main); scrollArea->setWidget(contentWidget);
    auto* pageLayout = new QVBoxLayout(startPage); pageLayout->setContentsMargins(0, 0, 0, 0); pageLayout->addWidget(scrollArea);

    connect(btnLoad, &QPushButton::clicked, this, &MainWindow::onLoadInstruction);
    connect(btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(btnAutoDiscover, &QPushButton::clicked, this, &MainWindow::onAutoDiscoverClicked);
    connect(btnConnectAll, &QPushButton::clicked, this, &MainWindow::onConnectAllClicked);
    connect(btnViewDb, &QPushButton::clicked, this, [this]() { DbViewerWindow dbWin(&m_db, this); dbWin.exec(); refreshComboBoxes(); });
}

// Функция построения рабочего окна
void MainWindow::buildWorkWindow()
{
    workPage = new QWidget; lblSerial = new QLabel("Серийный номер: —"); lblState = new QLabel("IDLE");
    lblState->setStyleSheet("background:#B0BEC5; color:white; padding:6px 16px; border-radius:4px; font-weight:bold; font-size:14px;");
    auto* topBar = new QHBoxLayout; topBar->addWidget(lblSerial); topBar->addStretch(); topBar->addWidget(lblState);

    lblStageTitle = new QLabel("Этап 0 из 0:"); lblStageText = new QLabel("ожидание команды");
    auto* stageRow = new QHBoxLayout; stageRow->addWidget(lblStageTitle); stageRow->addWidget(lblStageText); stageRow->addStretch();
    auto* stageBox = new QGroupBox("Текущий этап"); stageBox->setLayout(stageRow);
    stageBox->setStyleSheet("QGroupBox { background:#FFF8E1; border:1px solid #FFD54F; border-radius:6px; margin-top:8px; padding:10px; } QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color:#5D4037; font-weight:bold; } QLabel { font-size:20px; font-weight:bold; }");
    stageBox->setMinimumHeight(120);

    lblDiagnostics = new QLabel("Система готова"); lblDiagnostics->setStyleSheet("background:#FAFAFA; border:1px solid #B0BEC5; padding:8px 12px; border-radius:6px; font-family: Consolas; font-size:13px;");
    auto* diagBox = new QGroupBox("Сообщения системы"); auto* diagLayout = new QVBoxLayout(diagBox); diagLayout->addWidget(lblDiagnostics);
    diagBox->setStyleSheet("QGroupBox { border-radius:6px; margin-top:8px; padding:10px; } QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; font-weight:bold; }");

    lblX = new QLabel("0.00"); lblY = new QLabel("0.00"); lblZ = new QLabel("0.00");
    QString coordValueStyle = "background:#ECEFF1; border:1px solid #B0BEC5; padding:6px 12px; font-size:20px; font-weight:bold; border-radius:6px; min-width:90px;";
    lblX->setStyleSheet(coordValueStyle); lblY->setStyleSheet(coordValueStyle); lblZ->setStyleSheet(coordValueStyle);
    lblX->setAlignment(Qt::AlignCenter); lblY->setAlignment(Qt::AlignCenter); lblZ->setAlignment(Qt::AlignCenter);

    auto makeAxis = [&](const QString& name, QLabel* value) -> QHBoxLayout* { auto* row = new QHBoxLayout; auto* lbl = new QLabel(name + ":"); lbl->setStyleSheet("font-weight:bold;"); row->addWidget(lbl); row->addWidget(value); return row; };
    auto* coordsRow = new QHBoxLayout; coordsRow->addLayout(makeAxis("X", lblX)); coordsRow->addSpacing(20);
    coordsRow->addLayout(makeAxis("Y", lblY)); coordsRow->addSpacing(20); coordsRow->addLayout(makeAxis("Z", lblZ)); coordsRow->addStretch();
    auto* coordsBox = new QGroupBox("Координаты"); coordsBox->setLayout(coordsRow);

    btnTighten = new QPushButton("🔧 Начать затяжку винтов"); btnSticker = new QPushButton("🏷️ Выдать наклейку");
    btnTighten->setMinimumHeight(45); btnTighten->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; border-radius:6px; background:#FF9800; color:white; } QPushButton:hover { background:#F57C00; } QPushButton:disabled { background:#CFD8DC; color:#90A4AE; }");
    btnSticker->setMinimumHeight(45); btnSticker->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; border-radius:6px; background:#9C27B0; color:white; } QPushButton:hover { background:#7B1FA2; } QPushButton:disabled { background:#CFD8DC; color:#90A4AE; }");
    btnTighten->setEnabled(false); btnSticker->setEnabled(false);
    auto* actionRow = new QHBoxLayout; actionRow->addWidget(btnTighten); actionRow->addWidget(btnSticker);
    connect(btnTighten, &QPushButton::clicked, this, &MainWindow::onTightenClicked);
    connect(btnSticker, &QPushButton::clicked, this, &MainWindow::onStickerClicked);

    btnPrev = new QPushButton("◀  Предыдущая");
    btnNext = new QPushButton("Следующая  ▶"); btnFinish = new QPushButton("⏹  Завершить работу");
    QList<QPushButton*> btns = { btnPrev, btnNext };
    for (auto* b : btns) { b->setMinimumHeight(45); b->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; border-radius:6px; background:#ECEFF1; border:1px solid #90A4AE; } QPushButton:hover { background:#CFD8DC; }"); }
    btnNext->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; border-radius:6px; background:#43A047; color:white; } QPushButton:hover { background:#2E7D32; }");
    btnFinish->setMinimumHeight(45); btnFinish->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; border-radius:6px; background:#E53935; color:white; } QPushButton:hover { background:#C62828; }");
    auto* btnRow = new QHBoxLayout; btnRow->addWidget(btnPrev); btnRow->addWidget(btnNext); btnRow->addWidget(btnFinish);

    auto* main = new QVBoxLayout(workPage); main->setContentsMargins(20, 15, 20, 15); main->addLayout(topBar); main->addSpacing(10);
    main->addWidget(stageBox); main->addWidget(diagBox); main->addWidget(coordsBox); main->addSpacing(10); main->addLayout(actionRow); main->addStretch(); main->addSpacing(10); main->addLayout(btnRow);

    connect(btnNext, &QPushButton::clicked, this, &MainWindow::onNextClicked);
    connect(btnPrev, &QPushButton::clicked, this, &MainWindow::onPrevClicked);
    connect(btnFinish, &QPushButton::clicked, this, &MainWindow::onFinishClicked);
}

// Функция загрузки файла инструкции
void MainWindow::onLoadInstruction()
{
    QString instructionsDir = QApplication::applicationDirPath() + "/instructions"; QDir dir;
    if (!dir.exists(instructionsDir)) dir.mkpath(instructionsDir);
    QString filePath = QFileDialog::getOpenFileName(this, "Выберите файл инструкции", instructionsDir, "Текстовые файлы (*.txt)");
    if (filePath.isEmpty()) return;
    QString errorMsg;
    if (parseInstruction(filePath, errorMsg)) {
        QFileInfo fileInfo(filePath); fileName->setText(fileInfo.fileName()); fileName->setStyleSheet("color: black;");
        QString dbErr; int nextSeq = m_db.getNextSequenceNumber(m_instructionNumber, dbErr);
        QString previewSerial = QString("%1-%2").arg(m_instructionNumber, 3, 10, QChar('0')).arg(nextSeq, 4, 10, QChar('0'));
        editSerial->setText(previewSerial);
        startStatus->setText(QString("Инструкция загружена. Пунктов: %1").arg(steps.size())); startStatus->setStyleSheet("color: #2E7D32;");
    }
    else {
        QMessageBox::warning(this, "Ошибка инструкции", errorMsg);
        fileName->setText("файл не выбран (ошибка)"); fileName->setStyleSheet("color: red;"); editSerial->clear();
    }
}

// Слот запуска работы
void MainWindow::onStartClicked()
{
    if (editOperator->text().trimmed().isEmpty() || steps.isEmpty() || m_instructionNumber == 0) {
        startStatus->setText("Заполните ФИО и загрузите корректную инструкцию!"); startStatus->setStyleSheet("color:#c62828;"); return;
    }
    if (!createNewSessionRecord()) return;
    startStatus->clear(); startWorkSession();
}

// Функция запуска рабочей сессии
void MainWindow::startWorkSession()
{
    lblState->setText("READY"); lblState->setStyleSheet("background:#1976D2; color:white; padding:6px 16px; border-radius:4px; font-weight:bold;");
    currentStep = 0; InstructionStep& firstStep = steps[currentStep];
    lblStageTitle->setText(QString("Этап %1 из %2:").arg(firstStep.number).arg(totalSteps));
    lblStageText->setText(firstStep.type == StepType::Move ? QString("перемещение %1, %2, %3").arg(firstStep.x).arg(firstStep.y).arg(firstStep.z) : firstStep.text);
    logMessage("Работа начата. Используйте джойстик/кнопки для перемещения.");

    if (m_links) {
        if (m_links->isConnected(0)) m_links->sendCommand(0, "START");
        if (m_links->isConnected(1)) m_links->sendCommand(1, "START");
        if (m_links->isConnected(2)) m_links->sendCommand(2, "START");
    }
    updateStepUI(); m_stack->setCurrentWidget(workPage);
}

// Слот перехода к следующему шагу
void MainWindow::onNextClicked() {
    if (currentStep < steps.size() - 1) currentStep++;
    InstructionStep& step = steps[currentStep];
    lblStageTitle->setText(QString("Этап %1 из %2:").arg(step.number).arg(totalSteps));
    if (m_currentRecordId > 0) { QString e; m_db.updateCurrentStep(m_currentRecordId, currentStep, e); }

    if (step.type == StepType::Move) {
        lblStageText->setText(QString("перемещение %1, %2, %3").arg(step.x).arg(step.y).arg(step.z));
        lblX->setText(QString::number(step.x, 'f', 2)); lblY->setText(QString::number(step.y, 'f', 2)); lblZ->setText(QString::number(step.z, 'f', 2));
        logMessage(QString("Ожидается точка: X=%1, Y=%2, Z=%3. Подведите вручную.").arg(step.x).arg(step.y).arg(step.z));
    }
    else {
        lblStageText->setText(step.text); logMessage(QString("Шаг %1: %2").arg(step.number).arg(step.text));
    }
    updateStepUI();
}

// Слот перехода к предыдущему шагу
void MainWindow::onPrevClicked() {
    if (currentStep > 0) currentStep--;
    InstructionStep& step = steps[currentStep];
    lblStageTitle->setText(QString("Этап %1 из %2:").arg(step.number).arg(totalSteps));
    if (m_currentRecordId > 0) { QString e; m_db.updateCurrentStep(m_currentRecordId, currentStep, e); }

    if (step.type == StepType::Move) {
        lblStageText->setText(QString("перемещение %1, %2, %3").arg(step.x).arg(step.y).arg(step.z));
        lblX->setText(QString::number(step.x, 'f', 2)); lblY->setText(QString::number(step.y, 'f', 2)); lblZ->setText(QString::number(step.z, 'f', 2));
        logMessage(QString("Возврат к точке: X=%1, Y=%2, Z=%3").arg(step.x).arg(step.y).arg(step.z));
    }
    else {
        lblStageText->setText(step.text);
        logMessage(QString("Возврат к шагу %1: %2").arg(step.number).arg(step.text));
    }
    updateStepUI();
}

// Слот запуска затяжки винтов
void MainWindow::onTightenClicked()
{
    btnTighten->setEnabled(false); logMessage("Запущена затяжка винтов (МК3)...");
    if (m_links && m_links->isConnected(2)) m_links->sendCommand(2, "TIGHTEN 500");
    else logMessage("Ошибка: МК3 не подключен!", true);
}

// Слот выдачи наклейки
void MainWindow::onStickerClicked()
{
    btnSticker->setEnabled(false); QString serial = editSerial->text();
    if (serial.isEmpty()) { logMessage("Ошибка: Серийный номер пуст!", true); return; }
    generateDataMatrixImage(serial);
    logMessage("Изображение маркировки сохранено.");
    logMessage("ИДЕТ ПЕЧАТЬ..."); QApplication::processEvents(); QThread::msleep(1500); logMessage("Печать завершена.");
    if (m_links && m_links->isConnected(2)) m_links->sendCommand(2, "STICKER");
    else logMessage("Ошибка: МК3 не подключен!", true);
}

// Функция генерации изображения маркировки
void MainWindow::generateDataMatrixImage(const QString& serialNumber)
{
    QString dirPath = QApplication::applicationDirPath() + "/markings"; QDir dir;
    if (!dir.exists(dirPath)) dir.mkpath(dirPath);
    QPixmap pixmap(300, 300); pixmap.fill(Qt::white); QPainter painter(&pixmap);
    painter.setPen(QPen(Qt::black, 2)); painter.drawRect(10, 10, 280, 280);
    painter.setBrush(Qt::black); painter.setPen(Qt::NoPen);
    int hash = qHash(serialNumber); srand(hash);
    for (int i = 0; i < 200; i++) { int x = 20 + rand() % 260; int y = 20 + rand() % 260; painter.drawRect(x, y, 4, 4); }
    painter.setPen(Qt::black); QFont font = painter.font(); font.setPointSize(14); font.setBold(true); painter.setFont(font);
    painter.drawText(QRect(0, 250, 300, 40), Qt::AlignCenter, serialNumber); painter.end();
    pixmap.save(dirPath + "/" + serialNumber + ".png", "PNG");
}

// Слот паузы
void MainWindow::onPauseClicked() {
    lblState->setText("PAUSED"); lblState->setStyleSheet("background:#FB8C00; color:white; padding:6px 16px; border-radius:4px; font-weight:bold;");
}

// Слот продолжения
void MainWindow::onResumeClicked() {
    lblState->setText("RUN"); lblState->setStyleSheet("background:#43A047; color:white; padding:6px 16px; border-radius:4px; font-weight:bold;");
}

// Слот запуска авто-поиска МК
void MainWindow::onAutoDiscoverClicked() { if (m_links) m_links->autoDiscover(); }

// Слот обработки результатов авто-поиска
void MainWindow::onDiscoveryFinished(QString p1, QString p2, QString p3) {
    if (!p1.isEmpty() && comboPort1) comboPort1->setCurrentText(p1);
    if (!p2.isEmpty() && comboPort2) comboPort2->setCurrentText(p2);
    if (!p3.isEmpty() && comboPort3) comboPort3->setCurrentText(p3);
    if (startStatus) {
        startStatus->setText(QString("Найдено: МК1=%1, МК2=%2, МК3=%3").arg(p1.isEmpty() ? "нет" : p1).arg(p2.isEmpty() ? "нет" : p2).arg(p3.isEmpty() ? "нет" : p3));
        startStatus->setStyleSheet("color: #2E7D32;");
    }
}

// Слот подключения всех МК
void MainWindow::onConnectAllClicked() {
    if (!comboPort1 || !comboPort2 || !comboPort3) return;
    QString p1 = comboPort1->currentText(); QString p2 = comboPort2->currentText(); QString p3 = comboPort3->currentText();
    if (p1.isEmpty() || p1 == "-") { QMessageBox::warning(this, "Ошибка", "Не выбран порт для МК1!"); return; }
    QString err;
    if (!m_links->connectMK(0, p1, err)) { QMessageBox::warning(this, "Ошибка", err); return; }
    if (!p2.isEmpty() && p2 != "-" && p2 != p1) { if (!m_links->connectMK(1, p2, err)) logMessage("Не удалось подключить МК2: " + err, true); }
    if (!p3.isEmpty() && p3 != "-" && p3 != p1 && p3 != p2) { if (!m_links->connectMK(2, p3, err)) logMessage("Не удалось подключить МК3: " + err, true); }
    btnConnectAll->setText("Подключено"); btnConnectAll->setStyleSheet("background:#4CAF50; color:white;"); logMessage("Порты открыты.");
}

// Слот обновления состояния МК
void MainWindow::onMkStatusUpdated(int mkIndex, MotorState st)
{
    if (mkIndex == 0) {
        // МК1: st.x = X, st.y = Y
        if (lblX) lblX->setText(QString::number(st.x / 1024.0, 'f', 2));
        if (lblY) lblY->setText(QString::number(st.y / 100.0, 'f', 2));
    }
    else if (mkIndex == 1) {
        // МК2: st.x = Z
        if (lblZ) lblZ->setText(QString::number(st.x / 1024.0, 'f', 2));
    }
}

// Функция вывода сообщения в лог
void MainWindow::logMessage(const QString& msg, bool isError) {
    QString t = QDateTime::currentDateTime().toString("HH:mm:ss");
    if (lblDiagnostics) {
        lblDiagnostics->setText(QString("[%1] %2").arg(t, msg));
        lblDiagnostics->setStyleSheet(isError ? "background:#FFEBEE; color:#C62828; font-weight:bold;" : "background:#FAFAFA; color:#333;");
    }
    else qDebug() << "LOG:" << msg;
}

// Функция разбора файла инструкции
bool MainWindow::parseInstruction(const QString& filePath, QString& errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { errorMsg = "Не удалось открыть файл: " + filePath; return false; }
    QTextStream in(&file); in.setCodec("UTF-8"); steps.clear();
    QString header = in.readLine().trimmed();
    if (header.compare("ИНСТРУКЦИЯ", Qt::CaseInsensitive) != 0) { errorMsg = "Первая строка должна быть 'ИНСТРУКЦИЯ'."; return false; }
    QString numLine = in.readLine().trimmed(); bool ok; m_instructionNumber = numLine.toInt(&ok);
    if (!ok) { errorMsg = "Вторая строка должна содержать номер инструкции (число)."; return false; }

    QRegularExpression stepRe(R"(^(\d+)\.\s*(.+)$)");
    QRegularExpression moveRe(R"(^перемещение\s+([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)$)", QRegularExpression::CaseInsensitiveOption);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed(); if (line.isEmpty()) continue;
        auto m = stepRe.match(line); if (!m.hasMatch()) continue;
        InstructionStep step; step.number = m.captured(1).toInt(); QString rest = m.captured(2).trimmed();
        auto mv = moveRe.match(rest);
        if (mv.hasMatch()) {
            step.type = StepType::Move;
            step.x = mv.captured(1).toDouble(); step.y = mv.captured(2).toDouble(); step.z = mv.captured(3).toDouble();
        }
        else if (rest.compare("начало", Qt::CaseInsensitive) == 0) { step.type = StepType::Start; step.text = rest; }
        else if (rest.compare("конец", Qt::CaseInsensitive) == 0) { step.type = StepType::End; step.text = rest; }
        else {
            step.type = StepType::Text;
            if (rest.startsWith('"') && rest.endsWith('"') && rest.size() >= 2) rest = rest.mid(1, rest.size() - 2);
            step.text = rest;
        }
        steps.append(step);
    }
    if (steps.isEmpty()) { errorMsg = "Не найдено ни одного пункта в файле."; return false; }
    totalSteps = steps.size() - 1; currentStep = 0; return true;
}

// Слот завершения сессии
void MainWindow::onFinishClicked()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Завершение");
    msgBox.setText("Как вы хотите завершить текущую сессию?");

    QPushButton* btnExit = msgBox.addButton("Выйти из программы", QMessageBox::DestructiveRole);
    QPushButton* btnRepeat = msgBox.addButton("Повторить серию", QMessageBox::ActionRole);
    QPushButton* btnChange = msgBox.addButton("Изменить данные", QMessageBox::AcceptRole);
    msgBox.exec();

    // Закрываем запись в БД
    if (m_currentRecordId > 0) {
        QString errMsg;
        m_db.finishRecord(m_currentRecordId, errMsg);
        m_currentRecordId = -1;
    }

    // Отправляем STOP на все МК
    if (m_links) {
        if (m_links->isConnected(0)) m_links->sendCommand(0, "STOP");
        if (m_links->isConnected(1)) m_links->sendCommand(1, "STOP");
        if (m_links->isConnected(2)) m_links->sendCommand(2, "STOP");
    }

    if (msgBox.clickedButton() == btnExit) {
        QApplication::quit();
    }
    else if (msgBox.clickedButton() == btnRepeat) {
        // Создаём новую запись с тем же оператором и компонентами, но новым SN
        if (createNewSessionRecord()) {
            startWorkSession();
            logMessage("Серия повторена. Новый серийный номер: " + editSerial->text());
        }
    }
    else if (msgBox.clickedButton() == btnChange) {
        // Возвращаемся на старт, сохраняя данные
        m_stack->setCurrentWidget(startPage);
        QString dbErr;
        int nextSeq = m_db.getNextSequenceNumber(m_instructionNumber, dbErr);
        QString previewSerial = QString("%1-%2").arg(m_instructionNumber, 3, 10, QChar('0')).arg(nextSeq, 4, 10, QChar('0'));
        editSerial->setText(previewSerial);
        startStatus->setText("Данные сохранены. Вы можете изменить компоненты и нажать 'НАЧАТЬ РАБОТУ'.");
        startStatus->setStyleSheet("color: blue;");
    }
}

// Слот обновления координат оси
void MainWindow::onAxisUpdated(int axisId, long steps)
{
    // 1 = X, 2 = Y, 3 = Z
    if (axisId == 1) {
        if (lblX) lblX->setText(QString::number(steps / 1024.0, 'f', 2));
    }
    else if (axisId == 2) {
        if (lblY) lblY->setText(QString::number(steps / 100.0, 'f', 2));
    }
    else if (axisId == 3) {
        if (lblZ) lblZ->setText(QString::number(steps / 1024.0, 'f', 2));
    }
}