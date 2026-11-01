#include "mainwindow.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
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
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QComboBox>
#include <QScrollArea>
#include <QPainter>
#include <QPixmap>
#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QDialog>
#include <QPrinter>
#include <QPrintDialog>
#include <QCloseEvent>
#include <QElapsedTimer>

// ================= КОНСТРУКТОР =================
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("Управление сборкой");
    resize(1000, 850);

    // Открываем БД
    QString dbPath = QApplication::applicationDirPath() + "/assemblies.db";
    QString dbErr;
    if (!m_db.open(dbPath, dbErr)) QMessageBox::critical(this, "Ошибка БД", dbErr);

    // Создаем менеджер связи и стек страниц
    m_links = new LinkManager(this);
    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    // Строим интерфейс
    buildStartWindow();
    buildWorkWindow();

    m_stack->addWidget(startPage);
    m_stack->addWidget(workPage);
    m_stack->setCurrentWidget(startPage);

    // Подключаем сигналы от Arduino
    connect(m_links, &LinkManager::logMessage, this, [this](const QString& msg) { logMessage(msg); });
    connect(m_links, &LinkManager::discoveryFinished, this, &MainWindow::onDiscoveryFinished);
    connect(m_links, &LinkManager::axisUpdated, this, &MainWindow::onAxisUpdated);
    connect(m_links, &LinkManager::rawMessageReceived, this, &MainWindow::onRawMessage);

    // Потеря связи
    connect(m_links, &LinkManager::connectionLost, this, [this]() {
        if (m_stack->currentWidget() == workPage) {
            handleEmergency("Потеря связи с Arduino Mega!");
        }
        });
}

// ================= ДЕСТРУКТОР =================
MainWindow::~MainWindow() { m_db.close(); }

// ================= ЛОГИРОВАНИЕ =================
void MainWindow::logMessage(const QString& msg, bool isError)
{
    QString t = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString fullDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // Вывод на экран
    if (lblDiagnostics) {
        lblDiagnostics->setText(QString("[%1] %2").arg(t, msg));
        lblDiagnostics->setStyleSheet(isError
            ? "background:#FFEBEE; color:#C62828; font-weight:bold;"
            : "background:#FAFAFA; color:#333;");
    }

    // Запись в файл (папка logs рядом с .exe)
    QString logDir = QApplication::applicationDirPath() + "/logs";
    QDir dir(logDir);
    if (!dir.exists()) dir.mkpath(logDir);

    QFile file(logDir + "/log.txt");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << "[" << fullDate << "] ";
        if (isError) out << "[ОШИБКА] ";
        out << msg << "\n";
        file.close();
    }
}

// ================= СВЕТОДИОД =================
void MainWindow::updateLedState(int state)
{
    if (!lblLed) return;
    if (state == 0) lblLed->setStyleSheet("background:#43A047; border-radius:10px;"); // Зеленый
    else if (state == 1) lblLed->setStyleSheet("background:#FB8C00; border-radius:10px;"); // Желтый
    else if (state == 2) lblLed->setStyleSheet("background:#E53935; border-radius:10px;"); // Красный
}

// ================= ЗАКРЫТИЕ ОКНА (КРЕСТИК) =================
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_stack->currentWidget() == workPage && !steps.isEmpty()) {
        logMessage(QString("Попытка закрытия окна. Текущий шаг: %1").arg(currentStep + 1));

        QMessageBox::StandardButton reply = QMessageBox::question(this, "Выход",
            "Сохранить текущий прогресс перед выходом?\n(Статус: 'Сборка прервана')",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes) {
            // Сохраняем прогресс
            if (editOperator->text().trimmed().isEmpty()) editOperator->setText("Неизвестно");
            if (m_startTime.isEmpty()) m_startTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

            if (m_currentRecordId > 0) {
                QString e;
                m_db.updateCurrentStep(m_currentRecordId, currentStep, e);
                m_db.updateStatus(m_currentRecordId, "Сборка прервана", e);
                logMessage("Прогресс обновлен. Статус: 'Сборка прервана'.");
            }
            else {
                if (createNewSessionRecord("Сборка прервана")) {
                    if (m_currentRecordId > 0) {
                        QString e;
                        m_db.updateCurrentStep(m_currentRecordId, currentStep, e);
                    }
                    logMessage("Прогресс сохранен (новая запись). Статус: 'Сборка прервана'.");
                }
                else {
                    logMessage("Ошибка: не удалось сохранить прогресс!", true);
                }
            }
            event->accept();
        }
        else if (reply == QMessageBox::No) {
            logMessage("Выход без сохранения. Прогресс утерян.");
            event->accept();
        }
        else {
            logMessage("Выход отменен оператором.");
            event->ignore();
        }
    }
    else {
        event->accept();
    }
}

// ================= АВАРИЙНАЯ СИТУАЦИЯ =================
void MainWindow::handleEmergency(const QString& reason)
{
    updateLedState(2); // Красный
    logMessage(QString("АВАРИЯ: %1. Шаг: %2").arg(reason).arg(currentStep + 1), true);

    if (m_currentRecordId > 0) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Авария",
            "Произошла авария: " + reason + "\n\nСохранить текущий результат?",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            QString e;
            m_db.updateCurrentStep(m_currentRecordId, currentStep, e);
            m_db.updateStatus(m_currentRecordId, "Сборка прервана", e);
            logMessage("Результат сохранен. Статус: 'Сборка прервана'.");
        }
        else {
            logMessage("Авария. Оператор отказался сохранять.", true);
        }
    }
    else {
        logMessage("Авария до создания записи. Прогресс не сохранен.", true);
    }
    QApplication::quit();
}

// ================= НЕЗАВЕРШЕННЫЕ РАБОТЫ =================
void MainWindow::refreshUnfinishedList()
{
    cbUnfinished->blockSignals(true);
    cbUnfinished->clear();
    cbUnfinished->addItem("-- Начать новую работу --");

    m_unfinishedRecords = m_db.getUnfinishedRecords();
    for (const auto& rec : m_unfinishedRecords) {
        cbUnfinished->addItem(rec.serialNumber);
    }
    cbUnfinished->blockSignals(false);
}

void MainWindow::onLoadUnfinishedSession(const QString& serial)
{
    if (serial == "-- Начать новую работу --") {
        // Разблокируем поля
        editOperator->setReadOnly(false);
        cbModel->setEnabled(true);
        cbSize->setEnabled(true);
        cbFrameType->setEnabled(true);
        cbMotorSpecs->setEnabled(true);
        cbEscType->setEnabled(true);
        cbPdbType->setEnabled(true);
        cbFcType->setEnabled(true);
        cbRxType->setEnabled(true);
        cbVtxType->setEnabled(true);
        cbVtxPower->setEnabled(true);
        cbCameraSpecs->setEnabled(true);

        editOperator->clear();
        if (cbModel->count() > 0) cbModel->setCurrentIndex(0);
        m_currentRecordId = -1;
        return;
    }

    for (const auto& rec : m_unfinishedRecords) {
        if (rec.serialNumber == serial) {
            // Блокируем и заполняем
            editOperator->setText(rec.operatorName);
            editOperator->setReadOnly(true);
            if (cbModel->findText(rec.model) != -1) cbModel->setCurrentText(rec.model);
            cbModel->setEnabled(false);
            if (cbSize->findText(rec.size) != -1) cbSize->setCurrentText(rec.size);
            cbSize->setEnabled(false);

            cbFrameType->setCurrentText(rec.frameType); cbFrameType->setEnabled(false);
            cbMotorSpecs->setCurrentText(rec.motorSpecs); cbMotorSpecs->setEnabled(false);
            cbEscType->setCurrentText(rec.escType); cbEscType->setEnabled(false);
            cbPdbType->setCurrentText(rec.pdbType); cbPdbType->setEnabled(false);
            cbFcType->setCurrentText(rec.fcType); cbFcType->setEnabled(false);
            cbRxType->setCurrentText(rec.rxType); cbRxType->setEnabled(false);
            cbVtxType->setCurrentText(rec.vtxType); cbVtxType->setEnabled(false);
            cbVtxPower->setCurrentText(rec.vtxPower); cbVtxPower->setEnabled(false);
            cbCameraSpecs->setCurrentText(rec.cameraSpecs); cbCameraSpecs->setEnabled(false);

            editSerial->setText(rec.serialNumber);
            m_currentRecordId = rec.id;
            m_startTime = rec.startDate;

            startStatus->setText("Загружена незавершенная сессия. Можно продолжить.");
            startStatus->setStyleSheet("color: blue;");
            break;
        }
    }
}

// ================= СКАНИРОВАНИЕ ПАПОК =================
void MainWindow::scanInstructionsFolders()
{
    QString baseDir = QApplication::applicationDirPath() + "/instructions";
    QDir dir(baseDir);

    cbModel->clear();
    cbSize->clear();
    cbModel->addItem("-- Выберите модель --");
    cbSize->addItem("-- Выберите размер --");

    if (!dir.exists()) {
        dir.mkpath(baseDir);
        return;
    }

    QStringList models = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    cbModel->addItems(models);
}

void MainWindow::onModelChanged(const QString& model)
{
    cbSize->clear();
    if (model.isEmpty() || model == "-- Выберите модель --") {
        cbSize->addItem("-- Выберите размер --");
        editSerial->clear();
        fileName->setText("файл не выбран");
        steps.clear();
        return;
    }

    QString modelDirPath = QApplication::applicationDirPath() + "/instructions/" + model;
    QDir modelDir(modelDirPath);
    QStringList sizes = modelDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    if (sizes.isEmpty()) cbSize->addItem("-- Нет размеров --");
    else {
        cbSize->addItem("-- Выберите размер --");
        cbSize->addItems(sizes);
    }
}

// ================= ЗАГРУЗКА ИНСТРУКЦИИ =================
void MainWindow::updateInstructionPath()
{
    if (!fileName || !startStatus || !editSerial) return;

    QString model = cbModel->currentText();
    QString size = cbSize->currentText();
    if (model.isEmpty() || model == "-- Выберите модель --" ||
        size.isEmpty() || size == "-- Выберите размер --") return;

    QString baseDir = QApplication::applicationDirPath() + "/instructions/" + model + "/" + size + "/";
    QString expectedFileName = model + "_" + size + ".txt";
    QString filePath = baseDir + expectedFileName;

    if (!QFile::exists(filePath)) {
        startStatus->setText("Ошибка: не найден файл " + expectedFileName);
        startStatus->setStyleSheet("color: red;");
        editSerial->clear();
        fileName->setText("файл не найден");
        steps.clear();
        return;
    }

    m_instructionFilePath = filePath;
    QString errorMsg;

    if (parseInstruction(filePath, errorMsg)) {
        fileName->setText(expectedFileName);
        startStatus->setText(QString("Инструкция загружена. Пунктов: %1").arg(steps.size()));
        startStatus->setStyleSheet("color: #2E7D32;");

        // Генерируем предварительный SN
        if (m_currentRecordId == -1) {
            QString sizeNum = size;
            sizeNum.remove(QRegularExpression("[^0-9]"));
            if (sizeNum.isEmpty()) sizeNum = "0";
            QString prefix = model + sizeNum;

            QString dbErr;
            int nextSeq = m_db.getNextSequenceNumber(model, size, dbErr);
            QString previewSerial = QString("%1-%2").arg(prefix).arg(nextSeq, 3, 10, QChar('0'));
            editSerial->setText(previewSerial);
        }
    }
    else {
        startStatus->setText("Ошибка в файле: " + errorMsg);
        startStatus->setStyleSheet("color: red;");
        editSerial->clear();
        fileName->setText(expectedFileName + " (ошибка)");
        steps.clear();
    }
}

// ================= ПАРСИНГ ИНСТРУКЦИИ =================
bool MainWindow::parseInstruction(const QString& filePath, QString& errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = "Не удалось открыть файл: " + filePath;
        return false;
    }
    QTextStream in(&file);
    in.setCodec("UTF-8");
    steps.clear();
    in.readLine(); // Пропускаем первую строку (кол-во команд)

    // Регулярка для парсинга: "1. auto *Название* X Y Z speed (примечание)"
    QRegularExpression stepRe(
        R"(^(\d+)\.\s*(auto|manual)\s*\*(.*?)\*(?:\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)(?:\s+(\d+))?)?(?:\s*\((.*?)\))?$)",
        QRegularExpression::CaseInsensitiveOption
    );

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        auto m = stepRe.match(line);
        if (!m.hasMatch()) {
            errorMsg = "Ошибка синтаксиса в строке: " + line;
            return false;
        }

        InstructionStep step;
        step.number = m.captured(1).toInt();
        step.isManual = (m.captured(2).toLower() == "manual");
        step.displayText = m.captured(3);
        step.note = m.captured(8);

        QString titleLower = step.displayText.toLower();
        if (titleLower.contains("начало")) step.type = StepType::Start;
        else if (titleLower.contains("конец")) step.type = StepType::End;
        else if (titleLower.contains("затяжка")) step.type = StepType::Tighten;
        else if (titleLower.contains("маркировка")) step.type = StepType::Sticker;
        else if (titleLower.contains("перемещение")) {
            step.type = StepType::Move;
            step.x = m.captured(4).toDouble();
            step.y = m.captured(5).toDouble();
            step.z = m.captured(6).toDouble();
            if (!m.captured(7).isEmpty()) step.speed = m.captured(7).toInt();
        }
        else step.type = StepType::Text;

        steps.append(step);
    }

    if (steps.isEmpty()) {
        errorMsg = "Не найдено ни одного пункта в файле.";
        return false;
    }

    totalSteps = steps.size();
    currentStep = 0;
    return true;
}

// ================= СОЗДАНИЕ ЗАПИСИ В БД =================
bool MainWindow::createNewSessionRecord(const QString& finalStatus)
{
    if (steps.isEmpty()) return false;

    QString model = cbModel->currentText();
    QString size = cbSize->currentText();
    QString errMsg;

    AssemblyRecord rec;
    rec.serialNumber = editSerial->text();
    rec.operatorName = editOperator->text().trimmed();
    rec.model = model;
    rec.size = size;
    rec.instruction = fileName->text();
    rec.startDate = m_startTime;

    if (finalStatus == "Собран" || finalStatus == "Требует доработки") {
        rec.endDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    }
    else {
        rec.endDate = "";
    }

    rec.status = finalStatus;
    rec.frameType = cbFrameType->currentText();
    rec.motorSpecs = cbMotorSpecs->currentText();
    rec.escType = cbEscType->currentText();
    rec.pdbType = cbPdbType->currentText();
    rec.fcType = cbFcType->currentText();
    rec.rxType = cbRxType->currentText();
    rec.vtxType = cbVtxType->currentText();
    rec.vtxPower = cbVtxPower->currentText();
    rec.cameraSpecs = cbCameraSpecs->currentText();

    // Пытаемся сохранить (вставка)
    if (!m_db.saveFullRecord(rec, errMsg)) {
        // Если ошибка "UNIQUE constraint", значит запись уже есть — обновляем её
        if (errMsg.contains("UNIQUE constraint failed")) {
            logMessage("Запись с таким SN уже есть. Обновляем...");
            // Находим ID этой записи и обновляем
            QList<AssemblyRecord> all = m_db.getAllRecords();
            for (const auto& r : all) {
                if (r.serialNumber == rec.serialNumber) {
                    m_currentRecordId = r.id;
                    m_db.updateStatus(m_currentRecordId, finalStatus, errMsg);
                    m_db.finishRecord(m_currentRecordId, errMsg);
                    return true;
                }
            }
        }
        QMessageBox::warning(this, "Ошибка БД", errMsg);
        return false;
    }
    m_currentRecordId = m_db.lastInsertId();
    return true;
}

// ================= ОБНОВЛЕНИЕ КОМБОБОКСОВ =================
void MainWindow::refreshComboBoxes()
{
    auto updateCombo = [](QComboBox* cb, const QStringList& items) {
        QString currentText = cb->currentText();
        cb->clear();
        cb->addItem("");
        cb->addItems(items);
        cb->setCurrentText(currentText);
        };
    updateCombo(cbMotorSpecs, m_db.getUniqueValues("motor_specs"));
    updateCombo(cbEscType, m_db.getUniqueValues("esc_type"));
    updateCombo(cbPdbType, m_db.getUniqueValues("pdb_type"));
    updateCombo(cbFcType, m_db.getUniqueValues("fc_type"));
    updateCombo(cbRxType, m_db.getUniqueValues("rx_type"));
    updateCombo(cbVtxType, m_db.getUniqueValues("vtx_type"));
    updateCombo(cbVtxPower, m_db.getUniqueValues("vtx_power"));
    updateCombo(cbCameraSpecs, m_db.getUniqueValues("camera_specs"));
}

// ================= ОБНОВЛЕНИЕ ДОСТУПНОСТИ КНОПОК =================
void MainWindow::updateStepUI()
{
    if (currentStep < 0 || currentStep >= steps.size()) return;
    InstructionStep& step = steps[currentStep];

    if (btnTighten) btnTighten->setEnabled(false);
    if (btnSticker) btnSticker->setEnabled(false);

    if (step.type == StepType::Tighten) {
        if (btnTighten) btnTighten->setEnabled(true);
    }
    else if (step.type == StepType::Sticker) {
        if (btnSticker) btnSticker->setEnabled(true);
    }
}

// ================= ПОСТРОЕНИЕ СТАРТОВОГО ОКНА =================
void MainWindow::buildStartWindow()
{
    startPage = new QWidget;
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);

    auto* contentWidget = new QWidget;
    auto* main = new QVBoxLayout(contentWidget);
    main->setContentsMargins(20, 20, 20, 20);
    main->setSpacing(10);

    // Заголовок
    auto* title = new QLabel("Начало работы");
    QFont f = title->font();
    f.setPointSize(18);
    f.setBold(true);
    title->setFont(f);
    title->setAlignment(Qt::AlignCenter);

    // Поля ввода
    editOperator = new QLineEdit;
    editOperator->setPlaceholderText("Иванов");

    editSerial = new QLineEdit;
    editSerial->setReadOnly(true);
    editSerial->setPlaceholderText("выберите модель и размер");
    editSerial->setStyleSheet("background-color: #F5F5F5; color: #333; font-weight: bold;");

    cbModel = new QComboBox;
    cbSize = new QComboBox;

    fileName = new QLabel("файл не выбран");
    fileName->setStyleSheet("color: gray;");

    startStatus = new QLabel;
    startStatus->setStyleSheet("color: #c62828;");

    // Форма
    auto* form = new QFormLayout;
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignRight);

    cbUnfinished = new QComboBox;
    form->addRow("Незавершенная работа:", cbUnfinished);
    connect(cbUnfinished, &QComboBox::currentTextChanged, this, &MainWindow::onLoadUnfinishedSession);

    form->addRow("Фамилия оператора:", editOperator);
    form->addRow("Модель БПЛА:", cbModel);
    form->addRow("Типоразмер:", cbSize);
    form->addRow("Файл инструкции:", fileName);
    form->addRow("Серийный номер:", editSerial);

    // Блок комплектующих
    auto* compBox = new QGroupBox("Комплектующие БПЛА (необязательно)");
    compBox->setStyleSheet("QGroupBox { border:1px solid #90A4AE; border-radius:6px; margin-top:8px; padding:10px; background:#FAFAFA; } QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; font-weight:bold; }");

    auto makeCombo = [](const QStringList& items) {
        auto* cb = new QComboBox;
        cb->setEditable(true);
        cb->addItem("");
        cb->addItems(items);
        return cb;
        };

    cbFrameType = makeCombo({ "1", "2" });
    cbMotorSpecs = makeCombo(m_db.getUniqueValues("motor_specs"));
    cbEscType = makeCombo(m_db.getUniqueValues("esc_type"));
    cbPdbType = makeCombo(m_db.getUniqueValues("pdb_type"));
    cbFcType = makeCombo(m_db.getUniqueValues("fc_type"));
    cbRxType = makeCombo(m_db.getUniqueValues("rx_type"));
    cbVtxType = makeCombo(m_db.getUniqueValues("vtx_type"));
    cbVtxPower = makeCombo(m_db.getUniqueValues("vtx_power"));
    cbCameraSpecs = makeCombo(m_db.getUniqueValues("camera_specs"));

    auto* compForm = new QFormLayout;
    compForm->addRow("Тип рамы (1/2):", cbFrameType);
    compForm->addRow("Двигатели:", cbMotorSpecs);
    compForm->addRow("ESC:", cbEscType);
    compForm->addRow("PDB:", cbPdbType);
    compForm->addRow("Полетный контроллер:", cbFcType);
    compForm->addRow("Приемник:", cbRxType);
    compForm->addRow("Видеопередатчик:", cbVtxType);
    compForm->addRow("Мощность VTX:", cbVtxPower);
    compForm->addRow("Камера:", cbCameraSpecs);
    compBox->setLayout(compForm);

    // Блок оборудования
    auto* equipBox = new QGroupBox("Оборудование (Arduino Mega)");
    equipBox->setStyleSheet("QGroupBox { border:1px solid #90A4AE; border-radius:6px; margin-top:8px; padding:10px; background:#FAFAFA; } QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; font-weight:bold; }");

    auto* eqLayout = new QHBoxLayout(equipBox);
    comboPort1 = new QComboBox;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts())
        comboPort1->addItem(info.portName());

    btnAutoDiscover = new QPushButton("Авто-поиск и подключение");
    btnViewDb = new QPushButton("📁 Просмотр базы данных");
    btnViewDb->setStyleSheet("background:#607D8B; color:white; padding:6px; font-weight:bold;");

    eqLayout->addWidget(new QLabel("Порт:"));
    eqLayout->addWidget(comboPort1);
    eqLayout->addWidget(btnAutoDiscover);
    eqLayout->addStretch();
    eqLayout->addWidget(btnViewDb);

    // Кнопка старта
    btnStart = new QPushButton("▶   НАЧАТЬ РАБОТУ");
    btnStart->setMinimumHeight(50);
    btnStart->setStyleSheet("QPushButton { background:#1976D2; color:white; font-size:16px; font-weight:bold; border-radius:6px; } QPushButton:hover { background:#1565C0; }");

    // Компоновка
    main->addWidget(title);
    main->addSpacing(10);
    main->addLayout(form);
    main->addSpacing(10);
    main->addWidget(compBox);
    main->addSpacing(10);
    main->addWidget(equipBox);
    main->addSpacing(15);
    main->addWidget(btnStart);
    main->addWidget(startStatus);
    main->addStretch();

    contentWidget->setLayout(main);
    scrollArea->setWidget(contentWidget);

    auto* pageLayout = new QVBoxLayout(startPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scrollArea);

    // Подключения
    connect(cbModel, &QComboBox::currentTextChanged, this, &MainWindow::onModelChanged);
    connect(cbSize, &QComboBox::currentTextChanged, this, &MainWindow::updateInstructionPath);
    connect(btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(btnAutoDiscover, &QPushButton::clicked, this, &MainWindow::onAutoDiscoverClicked);
    connect(btnViewDb, &QPushButton::clicked, this, [this]() {
        DbViewerWindow dbWin(&m_db, this);
        dbWin.exec();
        refreshComboBoxes();
        });

    scanInstructionsFolders();
    refreshUnfinishedList();
}

// ================= ПОСТРОЕНИЕ РАБОЧЕГО ОКНА =================
void MainWindow::buildWorkWindow()
{
    workPage = new QWidget;

    // Верхняя панель
    lblSerial = new QLabel("Серийный номер: —");
    lblState = new QLabel("IDLE");
    lblState->setStyleSheet("background:#B0BEC5; color:white; padding:6px 16px; border-radius:4px; font-weight:bold; font-size:14px;");

    auto* topBar = new QHBoxLayout;
    topBar->addWidget(lblSerial);
    topBar->addStretch();

    // Светодиод
    lblLed = new QLabel;
    lblLed->setFixedSize(20, 20);
    lblLed->setStyleSheet("background:#43A047; border-radius:10px;");
    topBar->addWidget(new QLabel("Статус:"));
    topBar->addWidget(lblLed);
    topBar->addSpacing(10);
    topBar->addWidget(lblState);

    // Текущий этап
    lblStageTitle = new QLabel("Этап 0 из 0:");
    lblStageText = new QLabel("ожидание команды");
    auto* stageRow = new QHBoxLayout;
    stageRow->addWidget(lblStageTitle);
    stageRow->addWidget(lblStageText);
    stageRow->addStretch();

    auto* stageBox = new QGroupBox("Текущий этап");
    stageBox->setLayout(stageRow);
    stageBox->setStyleSheet("QGroupBox { background:#FFF8E1; border:1px solid #FFD54F; border-radius:6px; margin-top:8px; padding:10px; } QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color:#5D4037; font-weight:bold; } QLabel { font-size:20px; font-weight:bold; }");
    stageBox->setMinimumHeight(120);

    // Лог
    lblDiagnostics = new QLabel("Система готова");
    lblDiagnostics->setStyleSheet("background:#FAFAFA; border:1px solid #B0BEC5; padding:8px 12px; border-radius:6px; font-family: Consolas; font-size:13px;");

    auto* diagBox = new QGroupBox("Сообщения системы");
    auto* diagLayout = new QVBoxLayout(diagBox);
    diagLayout->addWidget(lblDiagnostics);
    diagBox->setStyleSheet("QGroupBox { border-radius:6px; margin-top:8px; padding:10px; } QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; font-weight:bold; }");

    // Координаты
    lblX = new QLabel("0.00");
    lblY = new QLabel("0.00");
    lblZ = new QLabel("0.00");
    QString coordValueStyle = "background:#ECEFF1; border:1px solid #B0BEC5; padding:6px 12px; font-size:20px; font-weight:bold; border-radius:6px; min-width:90px;";
    lblX->setStyleSheet(coordValueStyle);
    lblY->setStyleSheet(coordValueStyle);
    lblZ->setStyleSheet(coordValueStyle);
    lblX->setAlignment(Qt::AlignCenter);
    lblY->setAlignment(Qt::AlignCenter);
    lblZ->setAlignment(Qt::AlignCenter);

    auto makeAxis = [&](const QString& name, QLabel* value) -> QHBoxLayout* {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(name + ":");
        lbl->setStyleSheet("font-weight:bold;");
        row->addWidget(lbl);
        row->addWidget(value);
        return row;
        };

    auto* coordsRow = new QHBoxLayout;
    coordsRow->addLayout(makeAxis("X", lblX));
    coordsRow->addSpacing(20);
    coordsRow->addLayout(makeAxis("Y", lblY));
    coordsRow->addSpacing(20);
    coordsRow->addLayout(makeAxis("Z", lblZ));
    coordsRow->addStretch();

    auto* coordsBox = new QGroupBox("Координаты");
    coordsBox->setLayout(coordsRow);

    // Кнопки затяжки / маркировки
    btnTighten = new QPushButton("🔧 Начать затяжку винтов");
    btnSticker = new QPushButton("🏷️ Наклеить маркировку");
    btnTighten->setMinimumHeight(45);
    btnTighten->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; border-radius:6px; background:#FF9800; color:white; } QPushButton:hover { background:#F57C00; } QPushButton:disabled { background:#CFD8DC; color:#90A4AE; }");
    btnSticker->setMinimumHeight(45);
    btnSticker->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; border-radius:6px; background:#9C27B0; color:white; } QPushButton:hover { background:#7B1FA2; } QPushButton:disabled { background:#CFD8DC; color:#90A4AE; }");
    btnTighten->setEnabled(false);
    btnSticker->setEnabled(false);

    // Индикатор затяжки
    lblTightenStatus = new QLabel("");
    lblTightenStatus->setAlignment(Qt::AlignCenter);
    lblTightenStatus->setStyleSheet("font-weight:bold; font-size:14px;");

    auto* tightenLayout = new QVBoxLayout;
    tightenLayout->addWidget(btnTighten);
    tightenLayout->addWidget(lblTightenStatus);

    auto* actionRow = new QHBoxLayout;
    actionRow->addLayout(tightenLayout);
    actionRow->addWidget(btnSticker);

    connect(btnTighten, &QPushButton::clicked, this, &MainWindow::onTightenClicked);
    connect(btnSticker, &QPushButton::clicked, this, &MainWindow::onStickerClicked);

    // Кнопки навигации
    btnPrev = new QPushButton("◀  Предыдущая");
    btnPause = new QPushButton("⏸ Пауза");
    btnResume = new QPushButton("▶ Продолжить");
    btnNext = new QPushButton("Следующая  ▶");

    QList<QPushButton*> btns = { btnPrev, btnPause, btnResume, btnNext };
    for (auto* b : btns) {
        b->setMinimumHeight(45);
        b->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; border-radius:6px; background:#ECEFF1; border:1px solid #90A4AE; } QPushButton:hover { background:#CFD8DC; } QPushButton:disabled { background:#CFD8DC; color:#90A4AE; }");
    }
    btnNext->setStyleSheet("QPushButton { font-size:14px; font-weight:bold; border-radius:6px; background:#43A047; color:white; } QPushButton:hover { background:#2E7D32; } QPushButton:disabled { background:#CFD8DC; color:#90A4AE; }");

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(btnPrev);
    btnRow->addWidget(btnPause);
    btnRow->addWidget(btnResume);
    btnRow->addWidget(btnNext);

    // Компоновка рабочего окна
    auto* main = new QVBoxLayout(workPage);
    main->setContentsMargins(20, 15, 20, 15);
    main->addLayout(topBar);
    main->addSpacing(10);
    main->addWidget(stageBox);
    main->addWidget(diagBox);
    main->addWidget(coordsBox);
    main->addSpacing(10);
    main->addLayout(actionRow);
    main->addStretch();
    main->addSpacing(10);
    main->addLayout(btnRow);

    // Подключение кнопок навигации
    connect(btnNext, &QPushButton::clicked, this, &MainWindow::onNextClicked);
    connect(btnPrev, &QPushButton::clicked, this, &MainWindow::onPrevClicked);
    connect(btnPause, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(btnResume, &QPushButton::clicked, this, &MainWindow::onResumeClicked);
}

// ================= СТАРТ РАБОТЫ =================
void MainWindow::onStartClicked()
{
    // Проверка заполнения
    if (editOperator->text().trimmed().isEmpty() || steps.isEmpty() ||
        cbModel->currentText() == "-- Выберите модель --" ||
        cbSize->currentText() == "-- Выберите размер --") {
        startStatus->setText("Заполните ФИО, выберите модель и размер!");
        startStatus->setStyleSheet("color:#c62828;");
        return;
    }

    if (m_currentRecordId == -1) {
        // Новая работа
        m_startTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        startWorkSession(0);
    }
    else {
        // Продолжение незавершенной
        int savedStep = 0;
        for (const auto& rec : m_unfinishedRecords) {
            if (rec.serialNumber == editSerial->text()) {
                savedStep = rec.currentStep;
                break;
            }
        }
        startWorkSession(savedStep);
    }
}

// ================= ЗАПУСК РАБОЧЕЙ СЕССИИ =================
void MainWindow::startWorkSession(int startStep)
{
    lblState->setText("READY");
    lblState->setStyleSheet("background:#1976D2; color:white; padding:6px 16px; border-radius:4px; font-weight:bold;");
    lblSerial->setText("Серийный номер: " + editSerial->text());
    updateLedState(0); // Зеленый

    currentStep = startStep;
    if (currentStep >= steps.size()) currentStep = 0;

    InstructionStep& current = steps[currentStep];
    lblStageTitle->setText(QString("Этап %1 из %2:").arg(current.number).arg(totalSteps));

    if (current.type == StepType::Move) {
        lblStageText->setText(QString("%1 (X=%2, Y=%3, Z=%4)").arg(current.displayText).arg(current.x).arg(current.y).arg(current.z));
    }
    else {
        lblStageText->setText(current.displayText);
    }

    logMessage("Работа начата. Шаг: " + QString::number(currentStep + 1));

    if (m_links && m_links->isConnected())
        m_links->sendCommand("START");

    updateStepUI();
    m_stack->setCurrentWidget(workPage);
}

// ================= ПЕРЕХОД К СЛЕДУЮЩЕМУ ШАГУ =================
void MainWindow::onNextClicked()
{
    if (currentStep < steps.size() - 1) currentStep++;
    InstructionStep& step = steps[currentStep];

    lblStageTitle->setText(QString("Этап %1 из %2:").arg(step.number).arg(totalSteps));

    if (m_currentRecordId > 0) {
        QString e;
        m_db.updateCurrentStep(m_currentRecordId, currentStep, e);
    }

    if (step.type == StepType::Move) {
        lblStageText->setText(QString("%1 (X=%2, Y=%3, Z=%4)").arg(step.displayText).arg(step.x).arg(step.y).arg(step.z));
        lblX->setText(QString::number(step.x, 'f', 2));
        lblY->setText(QString::number(step.y, 'f', 2));
        lblZ->setText(QString::number(step.z, 'f', 2));

        if (m_links && m_links->isConnected()) {
            long sX = (long)(step.x * STEPS_PER_MM_X);
            long sY = (long)(step.y * STEPS_PER_MM_Y);
            long sZ = (long)(step.z * STEPS_PER_MM_Z);

            m_links->sendCommand(QString("MOVE %1 %2 %3 %4").arg(sX).arg(sY).arg(sZ).arg(step.speed));
            logMessage(QString("Отправлено: MOVE %1 %2 %3 (Скорость: %4)").arg(sX).arg(sY).arg(sZ).arg(step.speed));

            if (btnNext) btnNext->setEnabled(false);
        }
        else {
            logMessage(QString("Mega не подключена. Ожидается точка: X=%1, Y=%2, Z=%3").arg(step.x).arg(step.y).arg(step.z), true);
        }
    }
    else if (step.type == StepType::Sticker) {
        lblStageText->setText(step.displayText);

        QString serial = editSerial->text();
        if (serial.isEmpty()) {
            logMessage("Ошибка: Серийный номер пуст!", true);
            return;
        }

        logMessage("Генерация DataMatrix...");
        showMarkingDialog(serial);

        if (m_links && m_links->isConnected()) {
            m_links->sendCommand("STICKER");
        }

        if (btnSticker) {
            btnSticker->setEnabled(true);
            logMessage("Ожидание подтверждения наклейки...");
        }
    }
    else if (step.type == StepType::End) {
        lblStageText->setText(step.displayText);
        logMessage("Инструкция выполнена успешно.");

        if (m_currentRecordId > 0) {
            QString e;
            m_db.updateCurrentStep(m_currentRecordId, currentStep, e);
        }

        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Работа завершена");
        msgBox.setText("Инструкция выполнена успешно!");

        QPushButton* btnExit = msgBox.addButton("Выйти из программы", QMessageBox::DestructiveRole);
        QPushButton* btnStartPage = msgBox.addButton("В стартовое меню", QMessageBox::AcceptRole);
        QPushButton* btnRepeat = msgBox.addButton("Повторить серию", QMessageBox::ActionRole);
        msgBox.exec();

        if (msgBox.clickedButton() == btnExit) {
            QApplication::quit();
        }
        else if (msgBox.clickedButton() == btnRepeat) {
            m_currentRecordId = -1;
            m_startTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

            // Пересчет SN
            QString model = cbModel->currentText();
            QString size = cbSize->currentText();
            QString sizeNum = size;
            sizeNum.remove(QRegularExpression("[^0-9]"));
            if (sizeNum.isEmpty()) sizeNum = "0";
            QString prefix = model + sizeNum;

            QString dbErr;
            int nextSeq = m_db.getNextSequenceNumber(model, size, dbErr);
            QString newSerial = QString("%1-%2").arg(prefix).arg(nextSeq, 3, 10, QChar('0'));

            editSerial->setText(newSerial);
            lblSerial->setText("Серийный номер: " + newSerial);

            startWorkSession(0);
        }
        else if (msgBox.clickedButton() == btnStartPage) {
            m_stack->setCurrentWidget(startPage);
            refreshUnfinishedList();
            onLoadUnfinishedSession("-- Начать новую работу --");
        }
    }
    else {
        lblStageText->setText(step.displayText);
        logMessage(QString("Шаг %1: %2").arg(step.number).arg(step.displayText));
    }
    updateStepUI();
}

// ================= ПЕРЕХОД К ПРЕДЫДУЩЕМУ ШАГУ =================
void MainWindow::onPrevClicked()
{
    if (currentStep > 0) currentStep--;
    InstructionStep& step = steps[currentStep];

    lblStageTitle->setText(QString("Этап %1 из %2:").arg(step.number).arg(totalSteps));
    if (m_currentRecordId > 0) {
        QString e;
        m_db.updateCurrentStep(m_currentRecordId, currentStep, e);
    }

    if (step.type == StepType::Move) {
        lblStageText->setText(QString("%1 (X=%2, Y=%3, Z=%4)").arg(step.displayText).arg(step.x).arg(step.y).arg(step.z));
        lblX->setText(QString::number(step.x, 'f', 2));
        lblY->setText(QString::number(step.y, 'f', 2));
        lblZ->setText(QString::number(step.z, 'f', 2));
        logMessage(QString("Возврат к точке: X=%1, Y=%2, Z=%3").arg(step.x).arg(step.y).arg(step.z));
    }
    else if (step.type == StepType::Sticker) {
        lblStageText->setText(step.displayText);
        logMessage("Возврат к шагу маркировки.");
    }
    else {
        lblStageText->setText(step.displayText);
        logMessage(QString("Возврат к шагу %1: %2").arg(step.number).arg(step.displayText));
    }
    updateStepUI();
}

// ================= ПАУЗА =================
void MainWindow::onPauseClicked()
{
    lblState->setText("PAUSED");
    lblState->setStyleSheet("background:#FB8C00; color:white; padding:6px 16px; border-radius:4px; font-weight:bold;");
    btnPause->setEnabled(false);
    btnResume->setEnabled(true);
    updateLedState(1); // Желтый

    if (m_links && m_links->isConnected()) {
        m_links->sendCommand("PAUSE");
    }
    logMessage(QString("Работа приостановлена оператором. Шаг: %1").arg(currentStep + 1));
}

// ================= ПРОДОЛЖЕНИЕ =================
void MainWindow::onResumeClicked()
{
    lblState->setText("RUN");
    lblState->setStyleSheet("background:#43A047; color:white; padding:6px 16px; border-radius:4px; font-weight:bold;");
    btnPause->setEnabled(true);
    btnResume->setEnabled(false);
    updateLedState(0); // Зеленый

    if (m_links && m_links->isConnected()) {
        m_links->sendCommand("RESUME");
    }
    logMessage(QString("Работа возобновлена. Шаг: %1").arg(currentStep + 1));
}

// ================= ЗАПУСК ЗАТЯЖКИ =================
void MainWindow::onTightenClicked()
{
    // Блокировка всех кнопок
    if (btnTighten) btnTighten->setEnabled(false);
    if (btnNext) btnNext->setEnabled(false);
    if (btnPrev) btnPrev->setEnabled(false);
    if (btnPause) btnPause->setEnabled(false);

    if (lblTightenStatus) {
        lblTightenStatus->setText("Затяжка...");
        lblTightenStatus->setStyleSheet("color: #FF9800;");
    }

    logMessage("Запущена затяжка винтов...");
    updateLedState(1); // Желтый

    if (m_links && m_links->isConnected()) {
        m_links->sendCommand("TIGHTEN 500");
    }
    else {
        logMessage("Ошибка: Mega не подключена!", true);
        if (btnTighten) btnTighten->setEnabled(true);
        if (btnNext) btnNext->setEnabled(true);
        if (btnPrev) btnPrev->setEnabled(true);
        if (btnPause) btnPause->setEnabled(true);
        if (lblTightenStatus) {
            lblTightenStatus->setText("Ошибка связи");
            lblTightenStatus->setStyleSheet("color: #C62828;");
        }
        updateLedState(2); // Красный
    }
}

// ================= НАКЛЕЙКА И СОЗДАНИЕ ЗАПИСИ =================
//наклеить маркировку и создать запись в БД
void MainWindow::onStickerClicked()
{
    if (btnSticker) btnSticker->setEnabled(false); // Защита от двойного клика

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Завершение",
        "Добавить запись в базу данных?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QMessageBox statusBox(this);
        statusBox.setWindowTitle("Завершение сборки");
        statusBox.setText("Выберите статус сборки:");
        QPushButton* btnOk = statusBox.addButton("Собран", QMessageBox::AcceptRole);
        QPushButton* btnErr = statusBox.addButton("Требует доработки", QMessageBox::DestructiveRole);
        statusBox.exec();

        QString finalStatus = "Собран";
        if (statusBox.clickedButton() == btnErr) {
            finalStatus = "Требует доработки";
        }

        // Проверяем, есть ли УЖЕ запись с таким SN в базе
        QString serial = editSerial->text();
        bool recordExists = false;
        for (const auto& rec : m_unfinishedRecords) {
            if (rec.serialNumber == serial) {
                recordExists = true;
                break;
            }
        }

        bool success = false;
        if (recordExists && m_currentRecordId > 0) {
            // Если запись уже есть, мы просто обновляем её статус
            QString e;
            m_db.updateStatus(m_currentRecordId, finalStatus, e);
            m_db.finishRecord(m_currentRecordId, e);
            success = true;
            logMessage("Существующая запись обновлена. Статус: " + finalStatus);
        }
        else {
            // Если записи нет, создаем новую
            success = createNewSessionRecord(finalStatus);
        }

        if (success) {
            logMessage("Запись сохранена (Статус: " + finalStatus + ").");
        }
        else {
            logMessage("Ошибка при сохранении записи в БД.", true);
            if (btnSticker) btnSticker->setEnabled(true); // Разблокируем, если ошибка
        }
    }
    else {
        logMessage("Запись в БД не добавлена (пропущено оператором).");
        if (btnSticker) btnSticker->setEnabled(true);
    }
}

// ================= АВТО-ПОИСК MEGA =================
void MainWindow::onAutoDiscoverClicked()
{
    if (startStatus) {
        startStatus->setText("Идёт поиск Arduino Mega...");
        startStatus->setStyleSheet("color: #FB8C00;");
    }
    if (m_links) m_links->autoDiscover();
}

void MainWindow::onDiscoveryFinished(QString port)
{
    if (port.isEmpty()) {
        if (startStatus) {
            startStatus->setText("Mega не найдена. Проверьте USB.");
            startStatus->setStyleSheet("color: #c62828;");
        }
        return;
    }

    if (comboPort1) comboPort1->setCurrentText(port);

    QString err;
    if (m_links->connectPort(port, err)) {
        if (startStatus) {
            startStatus->setText("Mega успешно подключена к " + port);
            startStatus->setStyleSheet("color: #2E7D32;");
        }
        m_links->sendCommand("START");
    }
    else {
        if (startStatus) {
            startStatus->setText(err);
            startStatus->setStyleSheet("color: #c62828;");
        }
    }
}

// ================= ОБРАБОТКА СООБЩЕНИЙ ОТ MEGA =================
void MainWindow::onRawMessage(QString msg)
{
    msg = msg.trimmed();

    if (msg == "DONE MOVE") {
        logMessage("Перемещение завершено.");
        if (btnNext) btnNext->setEnabled(true);
    }
    else if (msg == "DONE TIGHTEN" || msg.startsWith("TIGHTEN_RESULT")) {
        bool success = true;
        if (msg.startsWith("TIGHTEN_RESULT")) {
            float current = msg.mid(15).trimmed().toFloat();
            logMessage(QString("Ток затяжки: %1 А").arg(current, 0, 'f', 3));
            if (current > 0.5) success = false;
        }

        if (success) {
            if (lblTightenStatus) {
                lblTightenStatus->setText("✅ Успешно");
                lblTightenStatus->setStyleSheet("color: #2E7D32; font-weight:bold;");
            }
            updateLedState(0); // Зеленый
        }
        else {
            if (lblTightenStatus) {
                lblTightenStatus->setText("❌ Не успешно");
                lblTightenStatus->setStyleSheet("color: #C62828; font-weight:bold;");
            }
            updateLedState(2); // Красный
            logMessage("АВАРИЯ: Превышен ток затяжки!", true);
            handleEmergency("Срыв резьбы или заклинивание (ток > 0.5 А)");
        }

        if (btnTighten) btnTighten->setEnabled(false);
        if (btnNext) btnNext->setEnabled(true);
        if (btnPrev) btnPrev->setEnabled(true);
        if (btnPause) btnPause->setEnabled(true);
    }
    else if (msg == "DONE STICKER") {
        logMessage("Наклейка нанесена.");
    }
    else if (msg.startsWith("ERR")) {
        logMessage(msg, true);
        handleEmergency(msg);
    }
    else if (msg.startsWith("LOG")) {
        logMessage(msg.mid(3).trimmed());
    }
    else if (msg.startsWith("PAUSED")) {
        logMessage("Mega подтвердила паузу.");
    }
    else if (msg.startsWith("ACK")) {
        logMessage("Получено подтверждение: " + msg);
    }
}

// ================= ОБНОВЛЕНИЕ КООРДИНАТ =================
void MainWindow::onAxisUpdated(int axisId, long steps)
{
    if (axisId == 1) { if (lblX) lblX->setText(QString::number(steps / STEPS_PER_MM_X, 'f', 2)); }
    if (axisId == 2) { if (lblY) lblY->setText(QString::number(steps / STEPS_PER_MM_Y, 'f', 2)); }
    if (axisId == 3) { if (lblZ) lblZ->setText(QString::number(steps / STEPS_PER_MM_Z, 'f', 2)); }
}

// ================= ВСПЛЫВАЮЩЕЕ ОКНО МАРКИРОВКИ =================
void MainWindow::showMarkingDialog(const QString& serialNumber)
{
    QString pythonScript = QApplication::applicationDirPath() + "/generateDataMatrix.py";
    QStringList arguments;
    arguments << pythonScript << serialNumber;

    QProcess process;
    process.start("python", arguments);
    process.waitForFinished();

    QString imagePath = QApplication::applicationDirPath() + "/markings/" + serialNumber + ".png";

    if (!QFile::exists(imagePath)) {
        logMessage("Ошибка: файл маркировки не создан! Проверьте Python.", true);
        return;
    }

    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Маркировка");
    dialog->setMinimumSize(400, 500);

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    QLabel* imageLabel = new QLabel;
    QPixmap pixmap(imagePath);
    imageLabel->setPixmap(pixmap.scaled(300, 300, Qt::KeepAspectRatio, Qt::FastTransformation));
    imageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(imageLabel);

    QPushButton* btnPrint = new QPushButton("🖨️ Отправить на печать");
    btnPrint->setMinimumHeight(40);

    connect(btnPrint, &QPushButton::clicked, this, [this, imagePath]() {
        QPrinter printer(QPrinter::HighResolution);
        QPrintDialog printDialog(&printer, this);

        if (printDialog.exec() == QDialog::Accepted) {
            QPainter painter;
            painter.begin(&printer);
            QPixmap pixmap(imagePath);

            QRect rect = painter.viewport();
            QSize size = pixmap.size();
            size.scale(rect.size(), Qt::KeepAspectRatio);
            painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
            painter.setWindow(pixmap.rect());
            painter.drawPixmap(0, 0, pixmap);
            painter.end();

            logMessage("Документ отправлен на печать.");
        }
        });
    layout->addWidget(btnPrint);

    QPushButton* btnDone = new QPushButton("Готово");
    btnDone->setMinimumHeight(40);
    connect(btnDone, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(btnDone);

    dialog->exec();
    dialog->deleteLater();
}