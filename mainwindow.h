#pragma once
#include <QMainWindow>
#include <QVector>
#include "linkmanager.h"
#include "database.h"

class QStackedWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QScrollArea;

// Тип шага инструкции
enum class StepType { Start, End, Text, Move };

// Структура одного шага инструкции
struct InstructionStep {
    int number = 0;
    StepType type = StepType::Text;
    QString text;
    double x = 0, y = 0, z = 0;
};

// Главное окно приложения
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // Конструктор: открывает БД, создаёт менеджер портов и строит окна
    explicit MainWindow(QWidget* parent = nullptr);

    // Деструктор: закрывает БД
    ~MainWindow();

private slots:
    // Слот запуска работы (кнопка «НАЧАТЬ РАБОТУ»)
    void onStartClicked();

    // Слот завершения сессии (кнопка «Завершить работу»)
    void onFinishClicked();

    // Слот перехода к следующему шагу
    void onNextClicked();

    // Слот перехода к предыдущему шагу
    void onPrevClicked();

    // Слот паузы
    void onPauseClicked();

    // Слот продолжения
    void onResumeClicked();

    // Слот загрузки файла инструкции
    void onLoadInstruction();

    // Слот запуска авто-поиска МК
    void onAutoDiscoverClicked();

    // Слот подключения всех МК
    void onConnectAllClicked();

    // Слот обработки результатов авто-поиска
    void onDiscoveryFinished(QString p1, QString p2, QString p3);

    // Слот обновления состояния МК
    void onMkStatusUpdated(int mkIndex, MotorState st);

    // Слот запуска затяжки винтов
    void onTightenClicked();

    // Слот выдачи наклейки
    void onStickerClicked();

    // Слот обновления координат оси
    void onAxisUpdated(int axisId, long steps);

private:
    // Функция построения стартового окна
    void buildStartWindow();

    // Функция построения рабочего окна
    void buildWorkWindow();

    // Функция вывода сообщения в лог
    void logMessage(const QString& msg, bool isError = false);

    // Функция разбора файла инструкции
    bool parseInstruction(const QString& filePath, QString& errorMsg);

    // Функция запуска рабочей сессии
    void startWorkSession();

    // Функция создания новой записи в БД
    bool createNewSessionRecord();

    // Функция обновления выпадающих списков комплектующих
    void refreshComboBoxes();

    // Функция обновления доступности кнопок действий по текущему шагу
    void updateStepUI();

    // Функция генерации изображения маркировки
    void generateDataMatrixImage(const QString& serialNumber);

    QStackedWidget* m_stack = nullptr;

    // --- Стартовое окно ---
    QWidget* startPage = nullptr;
    QLineEdit* editOperator = nullptr;
    QLineEdit* editSerial = nullptr;
    QPushButton* btnLoad = nullptr;
    QLabel* fileName = nullptr;
    QPushButton* btnStart = nullptr;
    QLabel* startStatus = nullptr;
    QComboBox* comboPort1 = nullptr;
    QComboBox* comboPort2 = nullptr;
    QComboBox* comboPort3 = nullptr;
    QPushButton* btnAutoDiscover = nullptr;
    QPushButton* btnConnectAll = nullptr;

    // Комплектующие
    QComboBox* cbFrameType = nullptr; QComboBox* cbMotorSpecs = nullptr;
    QComboBox* cbEscType = nullptr; QComboBox* cbPdbType = nullptr;
    QComboBox* cbFcType = nullptr; QComboBox* cbRxType = nullptr;
    QComboBox* cbVtxType = nullptr; QComboBox* cbVtxPower = nullptr;
    QComboBox* cbCameraSpecs = nullptr; QPushButton* btnViewDb = nullptr;

    // --- Рабочее окно ---
    QWidget* workPage = nullptr;
    QLabel* lblSerial = nullptr;
    QLabel* lblState = nullptr;
    QLabel* lblStageTitle = nullptr;
    QLabel* lblStageText = nullptr;
    QLabel* lblDiagnostics = nullptr;
    QLabel* lblX = nullptr; QLabel* lblY = nullptr; QLabel* lblZ = nullptr;
    QPushButton* btnPrev = nullptr; QPushButton* btnPause = nullptr;
    QPushButton* btnResume = nullptr; QPushButton* btnNext = nullptr; QPushButton* btnFinish = nullptr;
    QPushButton* btnTighten = nullptr; QPushButton* btnSticker = nullptr;

    QVector<InstructionStep> steps;
    int currentStep = 0;
    int totalSteps = 0;
    int m_instructionNumber = 0;

    LinkManager* m_links = nullptr;
    Database m_db;
    int m_currentRecordId = -1;

    // Коэффициенты пересчёта шагов в мм
    const double STEPS_PER_MM_X = 100.0;
    const double STEPS_PER_MM_Y = 1024.0;
    const double STEPS_PER_MM_Z = 1024.0;
};