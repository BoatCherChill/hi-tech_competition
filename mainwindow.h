#pragma once
#include <QMainWindow>
#include <QVector>
#include <QCloseEvent>
#include <QList>
#include "linkmanager.h"
#include "database.h"

class QStackedWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QScrollArea;

// ================= ТИПЫ ШАГОВ ИНСТРУКЦИИ =================
enum class StepType {
    Start,      // Шаг "Начало"
    End,        // Шаг "Конец"
    Text,       // Текстовый шаг (оператор сам выполняет действие)
    Move,       // Перемещение (отправляется MOVE)
    Tighten,    // Затяжка (отправляется TIGHTEN)
    Sticker     // Маркировка (DataMatrix + печать)
};

// ================= СТРУКТУРА ОДНОГО ШАГА =================
// Хранит данные из одной строки файла инструкции.
struct InstructionStep {
    int number = 0;             // Номер шага (1, 2, 3...)
    bool isManual = false;      // true = manual, false = auto
    StepType type = StepType::Text; // Тип действия
    QString displayText;        // Название (то, что в звездочках)
    QString note;               // Примечание в скобках (не показывается)
    double x = 0, y = 0, z = 0; // Координаты для перемещения
    int speed = 100;            // Скорость для перемещения
};

// ================= ГЛАВНОЕ ОКНО ПРИЛОЖЕНИЯ =================
// Управляет всем: стартовым экраном, рабочим экраном, диалогами, БД.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    // Перехват закрытия окна (крестик) — предлагает сохранить прогресс
    void closeEvent(QCloseEvent* event) override;

private slots:
    // ===== СТАРТ =====
    void onStartClicked();                          // Нажата кнопка "Начать работу"
    void onModelChanged(const QString& model);      // Сменилась модель (обновляем размеры)
    void updateInstructionPath();                   // Загружаем инструкцию по выбранной модели/размеру
    void onLoadUnfinishedSession(const QString& serial); // Выбрана незавершенная работа
    void onAutoDiscoverClicked();                   // Нажата кнопка "Авто-поиск"
    void onDiscoveryFinished(QString port);         // Поиск завершен

    // ===== РАБОТА =====
    void onNextClicked();                           // Нажата "Следующая"
    void onPrevClicked();                           // Нажата "Предыдущая"
    void onPauseClicked();                          // Нажата "Пауза"
    void onResumeClicked();                         // Нажата "Продолжить"
    void onTightenClicked();                        // Нажата "Затяжка"
    void onStickerClicked();                        // Нажата "Наклеить маркировку"

    // ===== СВЯЗЬ С ARDUINO =====
    void onAxisUpdated(int axisId, long steps);     // Arduino прислала координаты
    void onRawMessage(QString msg);                 // Arduino прислала DONE/ERR/LOG

private:
    // ===== ПОСТРОЕНИЕ UI =====
    void buildStartWindow();                        // Создание стартового окна
    void buildWorkWindow();                         // Создание рабочего окна

    // ===== ВСПОМОГАТЕЛЬНЫЕ =====
    void logMessage(const QString& msg, bool isError = false); // Вывод в лог (экран + файл)
    void updateLedState(int state);                 // Смена цвета светодиода (0/1/2)
    void updateStepUI();                            // Обновление доступности кнопок
    void refreshComboBoxes();                       // Обновление списков комплектующих
    void refreshUnfinishedList();                   // Обновление списка незавершенных работ
    void scanInstructionsFolders();                 // Сканирование папок с инструкциями
    void startWorkSession(int startStep = 0);       // Запуск рабочей сессии с шага
    void handleEmergency(const QString& reason);    // Обработка аварии (сохранить + выйти)

    // ===== ИНСТРУКЦИИ =====
    bool parseInstruction(const QString& filePath, QString& errorMsg); // Парсинг файла инструкции

    // ===== БД =====
    bool createNewSessionRecord(const QString& finalStatus = "В работе"); // Создание записи в БД

    // ===== МАРКИРОВКА =====
    void showMarkingDialog(const QString& serialNumber); // Окно с DataMatrix и печатью

    // ===== UI ЭЛЕМЕНТЫ =====
    QStackedWidget* m_stack = nullptr; // Стек страниц (старт/работа)

    // --- СТАРТОВОЕ ОКНО ---
    QWidget* startPage = nullptr;
    QComboBox* cbUnfinished = nullptr;              // Список незавершенных работ
    QList<AssemblyRecord> m_unfinishedRecords;      // Данные незавершенных работ
    QLineEdit* editOperator = nullptr;              // Ввод ФИО оператора
    QLineEdit* editSerial = nullptr;                // Серийный номер (только чтение)
    QLabel* fileName = nullptr;                     // Имя загруженного файла инструкции
    QComboBox* cbModel = nullptr;                   // Выбор модели
    QComboBox* cbSize = nullptr;                    // Выбор размера
    QComboBox* cbFrameType = nullptr;               // Тип рамы
    QComboBox* cbMotorSpecs = nullptr;              // Двигатели
    QComboBox* cbEscType = nullptr;                 // ESC
    QComboBox* cbPdbType = nullptr;                 // PDB
    QComboBox* cbFcType = nullptr;                  // Полетный контроллер
    QComboBox* cbRxType = nullptr;                  // Приемник
    QComboBox* cbVtxType = nullptr;                 // Видеопередатчик
    QComboBox* cbVtxPower = nullptr;                // Мощность VTX
    QComboBox* cbCameraSpecs = nullptr;             // Камера
    QPushButton* btnViewDb = nullptr;               // Кнопка "Просмотр БД"
    QComboBox* comboPort1 = nullptr;                // Выбор COM-порта
    QPushButton* btnAutoDiscover = nullptr;         // Кнопка "Авто-поиск"
    QPushButton* btnStart = nullptr;                // Кнопка "Начать работу"
    QLabel* startStatus = nullptr;                  // Статус на стартовом окне

    // --- РАБОЧЕЕ ОКНО ---
    QWidget* workPage = nullptr;
    QLabel* lblSerial = nullptr;                    // Отображение SN в работе
    QLabel* lblState = nullptr;                     // Статус (READY/PAUSED)
    QLabel* lblStageTitle = nullptr;                // "Этап X из Y"
    QLabel* lblStageText = nullptr;                 // Текст текущего шага
    QLabel* lblDiagnostics = nullptr;               // Лог-сообщения
    QLabel* lblX = nullptr;                         // Координата X
    QLabel* lblY = nullptr;                         // Координата Y
    QLabel* lblZ = nullptr;                         // Координата Z
    QLabel* lblTightenStatus = nullptr;             // Индикатор "Успешно"/"Не успешно"
    QLabel* lblLed = nullptr;                       // Светодиод (зеленый/желтый/красный)
    QPushButton* btnPrev = nullptr;                 // "Предыдущая"
    QPushButton* btnPause = nullptr;                // "Пауза"
    QPushButton* btnResume = nullptr;               // "Продолжить"
    QPushButton* btnNext = nullptr;                 // "Следующая"
    QPushButton* btnTighten = nullptr;              // "Затяжка"
    QPushButton* btnSticker = nullptr;              // "Наклеить маркировку"

    // --- ДАННЫЕ СЕССИИ ---
    QVector<InstructionStep> steps;                 // Все шаги из файла инструкции
    int currentStep = 0;                            // Текущий шаг (индекс)
    int totalSteps = 0;                             // Всего шагов
    QString m_instructionFilePath;                  // Путь к загруженной инструкции
    QString m_startTime;                            // Время старта сессии
    int m_instructionNumber = 0;                    // Номер инструкции (из файла)

    // --- СВЯЗЬ И БД ---
    LinkManager* m_links = nullptr;                 // Менеджер COM-порта
    Database m_db;                                  // База данных
    int m_currentRecordId = -1;                     // ID текущей записи (-1 = не создана)

    // --- КОНСТАНТЫ ПЕРЕВОДА ---
    const double STEPS_PER_MM_X = 80.0;             // Шагов на мм по X
    const double STEPS_PER_MM_Y = 80.0;             // Шагов на мм по Y
    const double STEPS_PER_MM_Z = 400.0;            // Шагов на мм по Z
};