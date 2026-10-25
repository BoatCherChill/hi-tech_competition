#include "database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QMessageBox>
#include <QLabel>
#include <QDebug>

// ================= БАЗА ДАННЫХ =================

// Функция открытия БД и создания таблицы
bool Database::open(const QString& dbPath, QString& errorMsg)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        errorMsg = "Не удалось открыть БД: " + m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);
    // Таблица из 18 полей
    QString sql =
        "CREATE TABLE IF NOT EXISTS assemblies ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  serial_number TEXT UNIQUE, operator_name TEXT,"
        "  instruction_file TEXT, instruction_number INTEGER, start_date TEXT, end_date TEXT,"
        "  current_step INTEGER DEFAULT 0, quality TEXT, notes TEXT,"
        "  frame_type TEXT, motor_specs TEXT, esc_type TEXT, pdb_type TEXT,"
        "  fc_type TEXT, rx_type TEXT, vtx_type TEXT, vtx_power TEXT, camera_specs TEXT"
        ")";

    if (!q.exec(sql)) {
        errorMsg = "Не удалось создать таблицу: " + q.lastError().text();
        return false;
    }
    return true;
}

// Функция закрытия БД
void Database::close()
{
    if (m_db.isOpen()) m_db.close();
}

// Функция вставки новой записи
int Database::insertRecord(const AssemblyRecord& rec, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO assemblies ("
        "serial_number, operator_name, instruction_file, instruction_number, start_date, end_date, current_step, notes,"
        "frame_type, motor_specs, esc_type, pdb_type, fc_type, rx_type, vtx_type, vtx_power, camera_specs"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    q.addBindValue(rec.serialNumber);
    q.addBindValue(rec.operatorName);
    q.addBindValue(rec.instructionFile);
    q.addBindValue(rec.instructionNumber);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue("");                     // end_date пока пустая
    q.addBindValue(rec.currentStep);
    q.addBindValue(rec.notes);

    q.addBindValue(rec.frameType);
    q.addBindValue(rec.motorSpecs);
    q.addBindValue(rec.escType);
    q.addBindValue(rec.pdbType);
    q.addBindValue(rec.fcType);
    q.addBindValue(rec.rxType);
    q.addBindValue(rec.vtxType);
    q.addBindValue(rec.vtxPower);
    q.addBindValue(rec.cameraSpecs);

    if (!q.exec()) {
        errorMsg = "Ошибка вставки в БД: " + q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

// Функция обновления текущего шага
bool Database::updateCurrentStep(int recordId, int step, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE assemblies SET current_step = ? WHERE id = ?");
    q.addBindValue(step);
    q.addBindValue(recordId);
    if (!q.exec()) { errorMsg = q.lastError().text(); return false; }
    return true;
}

// Функция завершения записи
bool Database::finishRecord(int recordId, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE assemblies SET end_date = ?, quality = 'OK' WHERE id = ?");
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(recordId);
    if (!q.exec()) { errorMsg = q.lastError().text(); return false; }
    return true;
}

// Функция обновления комплектующих и примечаний
bool Database::updateComponents(int recordId, const AssemblyRecord& rec, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare(
        "UPDATE assemblies SET "
        "frame_type = ?, motor_specs = ?, esc_type = ?, pdb_type = ?, "
        "fc_type = ?, rx_type = ?, vtx_type = ?, vtx_power = ?, camera_specs = ?, notes = ? "
        "WHERE id = ?");

    q.addBindValue(rec.frameType);
    q.addBindValue(rec.motorSpecs);
    q.addBindValue(rec.escType);
    q.addBindValue(rec.pdbType);
    q.addBindValue(rec.fcType);
    q.addBindValue(rec.rxType);
    q.addBindValue(rec.vtxType);
    q.addBindValue(rec.vtxPower);
    q.addBindValue(rec.cameraSpecs);
    q.addBindValue(rec.notes);
    q.addBindValue(recordId);

    if (!q.exec()) { errorMsg = q.lastError().text(); return false; }
    return true;
}

// Функция получения всех записей
QList<AssemblyRecord> Database::getAllRecords()
{
    QList<AssemblyRecord> list;
    QSqlQuery q(m_db);
    if (q.exec("SELECT * FROM assemblies ORDER BY id DESC")) {
        while (q.next()) {
            AssemblyRecord rec;
            rec.id = q.value("id").toInt();
            rec.serialNumber = q.value("serial_number").toString();
            rec.operatorName = q.value("operator_name").toString();
            rec.instructionFile = q.value("instruction_file").toString();
            rec.startDate = q.value("start_date").toString();
            rec.endDate = q.value("end_date").toString();
            rec.notes = q.value("notes").toString();

            rec.frameType = q.value("frame_type").toString();
            rec.motorSpecs = q.value("motor_specs").toString();
            rec.escType = q.value("esc_type").toString();
            rec.pdbType = q.value("pdb_type").toString();
            rec.fcType = q.value("fc_type").toString();
            rec.rxType = q.value("rx_type").toString();
            rec.vtxType = q.value("vtx_type").toString();
            rec.cameraSpecs = q.value("camera_specs").toString();
            list.append(rec);
        }
    }
    return list;
}

// Функция получения уникальных значений по колонке
QStringList Database::getUniqueValues(const QString& columnName)
{
    QStringList list;
    QSqlQuery q(m_db);
    q.prepare(QString("SELECT DISTINCT %1 FROM assemblies WHERE %1 IS NOT NULL AND %1 != ''").arg(columnName));
    if (q.exec()) {
        while (q.next()) {
            list.append(q.value(0).toString());
        }
    }
    return list;
}

// Функция генерации порядкового номера для инструкции
int Database::getNextSequenceNumber(int instructionNumber, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM assemblies WHERE instruction_number = ?");
    q.addBindValue(instructionNumber);

    if (q.exec() && q.next()) {
        return q.value(0).toInt() + 1;
    }
    errorMsg = q.lastError().text();
    return 1;
}

// ================= ОКНО ПРОСМОТРА БАЗЫ =================

// Конструктор: строит интерфейс окна
DbViewerWindow::DbViewerWindow(Database* db, QWidget* parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle("Просмотр и редактирование базы данных");
    resize(1300, 600);

    // Блок фильтров
    auto* filterLayout = new QHBoxLayout;
    filterLayout->addWidget(new QLabel("Фильтр по оператору:"));
    cbFilterOperator = new QComboBox;
    cbFilterOperator->addItem("Все");
    cbFilterOperator->addItems(m_db->getUniqueValues("operator_name"));
    filterLayout->addWidget(cbFilterOperator);

    filterLayout->addSpacing(20);
    filterLayout->addWidget(new QLabel("Фильтр по инструкции:"));
    cbFilterInstruction = new QComboBox;
    cbFilterInstruction->addItem("Все");
    cbFilterInstruction->addItems(m_db->getUniqueValues("instruction_file"));
    filterLayout->addWidget(cbFilterInstruction);
    filterLayout->addStretch();

    // Таблица
    table = new QTableWidget;
    table->setColumnCount(14);
    table->setHorizontalHeaderLabels({
        "ID", "Серийный номер", "Оператор", "Инструкция", "Начало", "Конец", "Примечания",
        "Рама", "Двигатели", "ESC", "PDB", "Полетный контроллер", "Приемник", "Видеопередатчик"
        });
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    auto* btnSave = new QPushButton("Сохранить изменения");
    btnSave->setMinimumHeight(40);
    btnSave->setStyleSheet("background:#1976D2; color:white; font-weight:bold;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(filterLayout);
    mainLayout->addWidget(table);
    mainLayout->addWidget(btnSave);

    connect(btnSave, &QPushButton::clicked, this, &DbViewerWindow::saveChanges);
    connect(cbFilterOperator, &QComboBox::currentTextChanged, this, &DbViewerWindow::applyFilters);
    connect(cbFilterInstruction, &QComboBox::currentTextChanged, this, &DbViewerWindow::applyFilters);

    loadData();
    applyFilters();
}

// Функция загрузки данных в таблицу
void DbViewerWindow::loadData()
{
    QList<AssemblyRecord> records = m_db->getAllRecords();
    table->setRowCount(records.size());

    for (int i = 0; i < records.size(); ++i) {
        const auto& r = records[i];

        table->setItem(i, 0, new QTableWidgetItem(QString::number(r.id)));
        table->setItem(i, 1, new QTableWidgetItem(r.serialNumber));
        table->setItem(i, 2, new QTableWidgetItem(r.operatorName));
        table->setItem(i, 3, new QTableWidgetItem(r.instructionFile));
        table->setItem(i, 4, new QTableWidgetItem(r.startDate));
        table->setItem(i, 5, new QTableWidgetItem(r.endDate));
        table->setItem(i, 6, new QTableWidgetItem(r.notes));
        table->setItem(i, 7, new QTableWidgetItem(r.frameType));
        table->setItem(i, 8, new QTableWidgetItem(r.motorSpecs));
        table->setItem(i, 9, new QTableWidgetItem(r.escType));
        table->setItem(i, 10, new QTableWidgetItem(r.pdbType));
        table->setItem(i, 11, new QTableWidgetItem(r.fcType));
        table->setItem(i, 12, new QTableWidgetItem(r.rxType));
        table->setItem(i, 13, new QTableWidgetItem(r.vtxType));

        // ID и SN не редактируем
        table->item(i, 0)->setFlags(table->item(i, 0)->flags() & ~Qt::ItemIsEditable);
        table->item(i, 1)->setFlags(table->item(i, 1)->flags() & ~Qt::ItemIsEditable);
    }
}

// Функция применения фильтров
void DbViewerWindow::applyFilters()
{
    QString filterOp = cbFilterOperator->currentText();
    QString filterInstr = cbFilterInstruction->currentText();

    for (int i = 0; i < table->rowCount(); ++i) {
        bool showRow = true;

        // Фильтр по оператору (колонка 2)
        if (filterOp != "Все") {
            if (table->item(i, 2)->text() != filterOp) showRow = false;
        }

        // Фильтр по инструкции (колонка 3)
        if (filterInstr != "Все") {
            if (table->item(i, 3)->text() != filterInstr) showRow = false;
        }

        table->setRowHidden(i, !showRow);
    }
}

// Функция сохранения изменений в БД
void DbViewerWindow::saveChanges()
{
    QString err;
    for (int i = 0; i < table->rowCount(); ++i) {
        int id = table->item(i, 0)->text().toInt();

        AssemblyRecord rec;
        rec.notes = table->item(i, 6)->text();
        rec.frameType = table->item(i, 7)->text();
        rec.motorSpecs = table->item(i, 8)->text();
        rec.escType = table->item(i, 9)->text();
        rec.pdbType = table->item(i, 10)->text();
        rec.fcType = table->item(i, 11)->text();
        rec.rxType = table->item(i, 12)->text();
        rec.vtxType = table->item(i, 13)->text();

        m_db->updateComponents(id, rec, err);
    }
    QMessageBox::information(this, "Успех", "Изменения успешно сохранены в базе данных!");
}