#pragma once
#include <QString>
#include <QSqlDatabase>
#include <QStringList>
#include <QList>
#include <QDialog>

class QTableWidget;
class QComboBox;

// ================= СТРУКТУРА ЗАПИСИ О СБОРКЕ =================
// Хранит все данные о конкретном экземпляре БПЛА: кто собирал, какой серийный номер,
// какие комплектующие использовались, на каком шаге остановились.
struct AssemblyRecord {
    int id = -1;                // Уникальный идентификатор в БД (первичный ключ)
    QString serialNumber;       // Серийный номер (например, Model_A5-001)
    QString operatorName;       // ФИО оператора, собиравшего БПЛА
    QString model;              // Модель (название папки с инструкцией)
    QString size;               // Типоразмер (например, 5-inch)
    QString instruction;        // Имя файла инструкции
    QString startDate;          // Дата и время начала сборки
    QString endDate;            // Дата и время окончания (пусто, если не завершено)
    QString status;             // Статус: "Собран", "Требует доработки", "Сборка прервана"
    int currentStep = 0;        // На каком шаге остановились (для незавершенных)

    // ====== КОМПЛЕКТУЮЩИЕ ======
    QString frameType;          // Тип рамы (1 или 2)
    QString motorSpecs;         // Характеристики двигателей
    QString escType;            // Тип ESC
    QString pdbType;            // Тип PDB
    QString fcType;             // Тип полетного контроллера
    QString rxType;             // Тип приемника
    QString vtxType;            // Тип видеопередатчика
    QString vtxPower;           // Мощность VTX
    QString cameraSpecs;        // Характеристики камеры
};

// ================= КЛАСС БАЗЫ ДАННЫХ =================
// Отвечает за все операции с SQLite-базой: открытие, сохранение, чтение, обновление.
class Database
{
public:
    // Открыть базу данных по указанному пути. Если файла нет — создаст его.
    // Возвращает true при успехе, иначе false и текст ошибки в errorMsg.
    bool open(const QString& dbPath, QString& errorMsg);

    // Закрыть соединение с БД.
    void close();

    // Получить следующий порядковый номер для модели и размера.
    // Например, если уже есть 3 записи Model_A 5-inch, вернет 4.
    int  getNextSequenceNumber(const QString& model, const QString& size, QString& errorMsg);

    // Сохранить полную запись в БД (используется при создании новой сессии).
    bool saveFullRecord(const AssemblyRecord& rec, QString& errorMsg);

    // Получить все записи из БД (для окна просмотра).
    QList<AssemblyRecord> getAllRecords();

    // Получить только незавершенные записи (у которых end_date пустое).
    // Используется для выпадающего списка "Незавершенная работа".
    QList<AssemblyRecord> getUnfinishedRecords();

    // Получить список уникальных значений для указанной колонки.
    // Нужно для автодополнения выпадающих списков комплектующих.
    QStringList getUniqueValues(const QString& columnName);

    // Обновить запись (используется в редакторе БД).
    bool updateRecord(int recordId, const AssemblyRecord& rec, QString& errorMsg);

    // Обновить текущий шаг для записи (вызывается при каждом переходе "Далее").
    bool updateCurrentStep(int recordId, int step, QString& errorMsg);

    // Обновить только статус (например, "Сборка прервана").
    bool updateStatus(int recordId, const QString& status, QString& errorMsg);

    // Завершить запись — установить end_date и статус "Собран".
    bool finishRecord(int recordId, QString& errorMsg);

    // Получить ID последней вставленной записи (для m_currentRecordId).
    int lastInsertId() const { return m_lastInsertId; }

private:
    QSqlDatabase m_db;          // Объект подключения к SQLite
    int m_lastInsertId = -1;    // ID последней вставленной записи
};

// ================= ОКНО ПРОСМОТРА БАЗЫ ДАННЫХ =================
// Отдельный диалог, который показывает таблицу со всеми записями.
// Позволяет фильтровать, редактировать статус и экспортировать в CSV.
class DbViewerWindow : public QDialog
{
    Q_OBJECT
public:
    explicit DbViewerWindow(Database* db, QWidget* parent = nullptr);

private slots:
    void loadData();        // Загрузить данные из БД в таблицу
    void saveChanges();     // Сохранить измененные ячейки обратно в БД
    void applyFilters();    // Скрыть строки, не соответствующие фильтрам
    void exportToCsv();     // Экспортировать таблицу в CSV-файл

private:
    Database* m_db = nullptr;           // Указатель на БД
    QTableWidget* table = nullptr;      // Таблица для отображения
    QComboBox* cbFilterOperator = nullptr; // Фильтр по оператору
    QComboBox* cbFilterModel = nullptr;    // Фильтр по модели
};