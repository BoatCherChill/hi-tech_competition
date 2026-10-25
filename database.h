#pragma once
#include <QString>
#include <QSqlDatabase>
#include <QStringList>
#include <QDialog>
#include <QComboBox>

class QTableWidget;

// Структура одной записи о сборке
struct AssemblyRecord {
    int id = -1;
    QString serialNumber;       // Серийный номер
    QString operatorName;       // ФИО оператора
    QString instructionFile;    // Файл инструкции
    int instructionNumber = 0;  // Номер инструкции
    QString startDate;          // Дата начала
    int currentStep = 0;        // Текущий шаг
    QString endDate;            // Дата завершения
    QString quality;            // Качество
    QString notes;              // Примечания

    // Комплектующие БПЛА
    QString frameType;
    QString motorSpecs;
    QString escType;
    QString pdbType;
    QString fcType;
    QString rxType;
    QString vtxType;
    QString vtxPower;
    QString cameraSpecs;
};

// Класс работы с базой данных SQLite
class Database
{
public:
    // Функция открытия БД и создания таблицы
    bool open(const QString& dbPath, QString& errorMsg);

    // Функция закрытия БД
    void close();

    // Функция вставки новой записи
    int insertRecord(const AssemblyRecord& rec, QString& errorMsg);

    // Функция обновления текущего шага
    bool updateCurrentStep(int recordId, int step, QString& errorMsg);

    // Функция завершения записи (проставляет дату и качество)
    bool finishRecord(int recordId, QString& errorMsg);

    // Функция обновления комплектующих и примечаний
    bool updateComponents(int recordId, const AssemblyRecord& rec, QString& errorMsg);

    // Функция получения всех записей
    QList<AssemblyRecord> getAllRecords();

    // Функция получения уникальных значений по колонке
    QStringList getUniqueValues(const QString& columnName);

    // Функция генерации порядкового номера для инструкции
    int getNextSequenceNumber(int instructionNumber, QString& errorMsg);

private:
    QSqlDatabase m_db;
};

// Окно просмотра и редактирования базы
class DbViewerWindow : public QDialog
{
    Q_OBJECT
public:
    // Конструктор: строит интерфейс окна
    explicit DbViewerWindow(Database* db, QWidget* parent = nullptr);
private slots:
    // Функция загрузки данных в таблицу
    void loadData();

    // Функция сохранения изменений в БД
    void saveChanges();

    // Функция применения фильтров
    void applyFilters();
private:
    Database* m_db;
    QTableWidget* table;
    QComboBox* cbFilterOperator;
    QComboBox* cbFilterInstruction;
};