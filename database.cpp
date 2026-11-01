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
#include <QComboBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDebug>

//открыть БД (создает таблицу при первом запуске)
bool Database::open(const QString& dbPath, QString& errorMsg)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        errorMsg = "Не удалось открыть БД: " + m_db.lastError().text();
        return false;
    }

    // Включаем WAL для защиты от одновременной записи
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA journal_mode=WAL;");

    // Создаем таблицу, если ее нет
    QSqlQuery q(m_db);
    QString sql =
        "CREATE TABLE IF NOT EXISTS assemblies ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  serial_number TEXT UNIQUE, operator_name TEXT,"
        "  model TEXT, size TEXT, instruction TEXT,"
        "  frame_type TEXT, motor_specs TEXT, esc_type TEXT, pdb_type TEXT,"
        "  fc_type TEXT, rx_type TEXT, vtx_type TEXT, vtx_power TEXT, camera_specs TEXT,"
        "  start_date TEXT, end_date TEXT, status TEXT,"
        "  current_step INTEGER DEFAULT 0"
        ")";
    if (!q.exec(sql)) {
        errorMsg = "Не удалось создать таблицу: " + q.lastError().text();
        return false;
    }
    return true;
}

//закрыть БД
void Database::close()
{
    if (m_db.isOpen()) m_db.close();
}

//получить следующий порядковый номер для модели/размера
int Database::getNextSequenceNumber(const QString& model, const QString& size, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM assemblies WHERE model = ? AND size = ?");
    q.addBindValue(model);
    q.addBindValue(size);

    if (q.exec() && q.next()) {
        return q.value(0).toInt() + 1;
    }
    errorMsg = q.lastError().text();
    return 1;
}

//сохранить полную запись в БД
bool Database::saveFullRecord(const AssemblyRecord& rec, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO assemblies ("
        "serial_number, operator_name, model, size, instruction, "
        "frame_type, motor_specs, esc_type, pdb_type, fc_type, rx_type, vtx_type, vtx_power, camera_specs, "
        "start_date, end_date, status"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    q.addBindValue(rec.serialNumber);
    q.addBindValue(rec.operatorName);
    q.addBindValue(rec.model);
    q.addBindValue(rec.size);
    q.addBindValue(rec.instruction);
    q.addBindValue(rec.frameType);
    q.addBindValue(rec.motorSpecs);
    q.addBindValue(rec.escType);
    q.addBindValue(rec.pdbType);
    q.addBindValue(rec.fcType);
    q.addBindValue(rec.rxType);
    q.addBindValue(rec.vtxType);
    q.addBindValue(rec.vtxPower);
    q.addBindValue(rec.cameraSpecs);
    q.addBindValue(rec.startDate);
    q.addBindValue(rec.endDate);
    q.addBindValue(rec.status);

    if (!q.exec()) {
        errorMsg = "Ошибка вставки в БД: " + q.lastError().text();
        return false;
    }
    m_lastInsertId = q.lastInsertId().toInt();
    return true;
}

//получить все записи из БД
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
            rec.model = q.value("model").toString();
            rec.size = q.value("size").toString();
            rec.instruction = q.value("instruction").toString();
            rec.startDate = q.value("start_date").toString();
            rec.endDate = q.value("end_date").toString();
            rec.status = q.value("status").toString();
            rec.currentStep = q.value("current_step").toInt();
            rec.frameType = q.value("frame_type").toString();
            rec.motorSpecs = q.value("motor_specs").toString();
            rec.escType = q.value("esc_type").toString();
            rec.pdbType = q.value("pdb_type").toString();
            rec.fcType = q.value("fc_type").toString();
            rec.rxType = q.value("rx_type").toString();
            rec.vtxType = q.value("vtx_type").toString();
            rec.vtxPower = q.value("vtx_power").toString();
            rec.cameraSpecs = q.value("camera_specs").toString();
            list.append(rec);
        }
    }
    return list;
}

//получить только незавершенные записи (end_date пусто)
QList<AssemblyRecord> Database::getUnfinishedRecords()
{
    QList<AssemblyRecord> list;
    QSqlQuery q(m_db);
    if (q.exec("SELECT * FROM assemblies WHERE end_date IS NULL OR end_date = '' ORDER BY id DESC")) {
        while (q.next()) {
            AssemblyRecord rec;
            rec.id = q.value("id").toInt();
            rec.serialNumber = q.value("serial_number").toString();
            rec.operatorName = q.value("operator_name").toString();
            rec.model = q.value("model").toString();
            rec.size = q.value("size").toString();
            rec.instruction = q.value("instruction").toString();
            rec.startDate = q.value("start_date").toString();
            rec.status = q.value("status").toString();
            rec.currentStep = q.value("current_step").toInt();
            rec.frameType = q.value("frame_type").toString();
            rec.motorSpecs = q.value("motor_specs").toString();
            rec.escType = q.value("esc_type").toString();
            rec.pdbType = q.value("pdb_type").toString();
            rec.fcType = q.value("fc_type").toString();
            rec.rxType = q.value("rx_type").toString();
            rec.vtxType = q.value("vtx_type").toString();
            rec.vtxPower = q.value("vtx_power").toString();
            rec.cameraSpecs = q.value("camera_specs").toString();
            list.append(rec);
        }
    }
    return list;
}

//получить уникальные значения для указанной колонки (для ComboBox)
QStringList Database::getUniqueValues(const QString& columnName)
{
    QStringList list;
    QSqlQuery q(m_db);
    q.prepare(QString("SELECT DISTINCT %1 FROM assemblies WHERE %1 IS NOT NULL AND %1 != ''")
        .arg(columnName));
    if (q.exec()) {
        while (q.next()) list.append(q.value(0).toString());
    }
    return list;
}

//обновить запись (для редактора БД)
bool Database::updateRecord(int recordId, const AssemblyRecord& rec, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare(
        "UPDATE assemblies SET "
        "frame_type = ?, motor_specs = ?, esc_type = ?, pdb_type = ?, "
        "fc_type = ?, rx_type = ?, vtx_type = ?, vtx_power = ?, camera_specs = ?, status = ? "
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
    q.addBindValue(rec.status);
    q.addBindValue(recordId);
    if (!q.exec()) { errorMsg = q.lastError().text(); return false; }
    return true;
}

//обновить текущий шаг
bool Database::updateCurrentStep(int recordId, int step, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE assemblies SET current_step = ? WHERE id = ?");
    q.addBindValue(step);
    q.addBindValue(recordId);
    if (!q.exec()) { errorMsg = q.lastError().text(); return false; }
    return true;
}

//обновить только статус
bool Database::updateStatus(int recordId, const QString& status, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE assemblies SET status = ? WHERE id = ?");
    q.addBindValue(status);
    q.addBindValue(recordId);
    if (!q.exec()) { errorMsg = q.lastError().text(); return false; }
    return true;
}

//завершить запись (установить end_date и статус "Собран")
bool Database::finishRecord(int recordId, QString& errorMsg)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE assemblies SET end_date = ?, status = ? WHERE id = ?");
    q.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    q.addBindValue("Собран");
    q.addBindValue(recordId);
    if (!q.exec()) { errorMsg = q.lastError().text(); return false; }
    return true;
}

// ================= ОКНО ПРОСМОТРА БАЗЫ =================
DbViewerWindow::DbViewerWindow(Database* db, QWidget* parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle("Просмотр базы данных");
    resize(1600, 600);

    auto* filterLayout = new QHBoxLayout;
    filterLayout->addWidget(new QLabel("Фильтр по оператору:"));
    cbFilterOperator = new QComboBox;
    cbFilterOperator->addItem("Все");
    cbFilterOperator->addItems(m_db->getUniqueValues("operator_name"));
    filterLayout->addWidget(cbFilterOperator);

    filterLayout->addSpacing(20);
    filterLayout->addWidget(new QLabel("Фильтр по модели:"));
    cbFilterModel = new QComboBox;
    cbFilterModel->addItem("Все");
    cbFilterModel->addItems(m_db->getUniqueValues("model"));
    filterLayout->addWidget(cbFilterModel);
    filterLayout->addStretch();

    table = new QTableWidget;
    table->setColumnCount(17);
    table->setHorizontalHeaderLabels({
        "Серийный номер", "Оператор", "Модель", "Размер",
        "Инструкция", "Начало", "Конец", "Статус",
        "Тип рамы", "Двигатели", "ESC", "PDB",
        "Полетный контр.", "Приемник", "Видеопередатчик", "Мощность VTX", "Камера"
        });
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto* btnSave = new QPushButton("Сохранить изменения");
    btnSave->setMinimumHeight(40);

    auto* btnExport = new QPushButton("📤 Экспорт в CSV");
    btnExport->setMinimumHeight(40);
    btnExport->setStyleSheet("background:#00897B; color:white; font-weight:bold;");

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnExport);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(filterLayout);
    mainLayout->addWidget(table);
    mainLayout->addLayout(btnLayout);

    connect(btnSave, &QPushButton::clicked, this, &DbViewerWindow::saveChanges);
    connect(btnExport, &QPushButton::clicked, this, &DbViewerWindow::exportToCsv);
    connect(cbFilterOperator, &QComboBox::currentTextChanged, this, &DbViewerWindow::applyFilters);
    connect(cbFilterModel, &QComboBox::currentTextChanged, this, &DbViewerWindow::applyFilters);

    loadData();
    applyFilters();
}

//загрузить данные из БД в таблицу
void DbViewerWindow::loadData()
{
    QList<AssemblyRecord> records = m_db->getAllRecords();
    table->setRowCount(records.size());

    for (int i = 0; i < records.size(); ++i) {
        const auto& r = records[i];
        table->setItem(i, 0, new QTableWidgetItem(r.serialNumber));
        table->setItem(i, 1, new QTableWidgetItem(r.operatorName));
        table->setItem(i, 2, new QTableWidgetItem(r.model));
        table->setItem(i, 3, new QTableWidgetItem(r.size));
        table->setItem(i, 4, new QTableWidgetItem(r.instruction));
        table->setItem(i, 5, new QTableWidgetItem(r.startDate));
        table->setItem(i, 6, new QTableWidgetItem(r.endDate));
        table->setItem(i, 7, new QTableWidgetItem(r.status));
        table->setItem(i, 8, new QTableWidgetItem(r.frameType));
        table->setItem(i, 9, new QTableWidgetItem(r.motorSpecs));
        table->setItem(i, 10, new QTableWidgetItem(r.escType));
        table->setItem(i, 11, new QTableWidgetItem(r.pdbType));
        table->setItem(i, 12, new QTableWidgetItem(r.fcType));
        table->setItem(i, 13, new QTableWidgetItem(r.rxType));
        table->setItem(i, 14, new QTableWidgetItem(r.vtxType));
        table->setItem(i, 15, new QTableWidgetItem(r.vtxPower));
        table->setItem(i, 16, new QTableWidgetItem(r.cameraSpecs));

        table->item(i, 0)->setFlags(table->item(i, 0)->flags() & ~Qt::ItemIsEditable);
        table->item(i, 0)->setData(Qt::UserRole, r.id);
    }
}

//применить фильтры (скрыть строки)
void DbViewerWindow::applyFilters()
{
    QString filterOp = cbFilterOperator->currentText();
    QString filterMod = cbFilterModel->currentText();

    for (int i = 0; i < table->rowCount(); ++i) {
        bool showRow = true;
        if (filterOp != "Все" && table->item(i, 1)->text() != filterOp)  showRow = false;
        if (filterMod != "Все" && table->item(i, 2)->text() != filterMod) showRow = false;
        table->setRowHidden(i, !showRow);
    }
}

//сохранить изменения в БД
void DbViewerWindow::saveChanges()
{
    QString err;
    bool anyError = false;
    for (int i = 0; i < table->rowCount(); ++i) {
        if (table->isRowHidden(i)) continue;
        int id = table->item(i, 0)->data(Qt::UserRole).toInt();
        if (id <= 0) continue;

        AssemblyRecord rec;
        rec.status = table->item(i, 7)->text();
        rec.frameType = table->item(i, 8)->text();
        rec.motorSpecs = table->item(i, 9)->text();
        rec.escType = table->item(i, 10)->text();
        rec.pdbType = table->item(i, 11)->text();
        rec.fcType = table->item(i, 12)->text();
        rec.rxType = table->item(i, 13)->text();
        rec.vtxType = table->item(i, 14)->text();
        rec.vtxPower = table->item(i, 15)->text();
        rec.cameraSpecs = table->item(i, 16)->text();

        if (!m_db->updateRecord(id, rec, err)) anyError = true;
    }
    if (anyError)
        QMessageBox::warning(this, "Ошибка", "Часть записей не сохранена.");
    else
        QMessageBox::information(this, "Готово", "Изменения сохранены.");
}

//экспорт в CSV (открывается в Excel без кракозябр)
void DbViewerWindow::exportToCsv()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Экспорт в CSV",
        "assemblies_export.csv", "CSV файлы (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать файл.");
        return;
    }

    QTextStream out(&file);
    out << "\xEF\xBB\xBF"; // BOM для корректного открытия в Excel (кириллица)

    // Записываем заголовки (берем из таблицы)
    for (int col = 0; col < table->columnCount(); ++col) {
        out << "\"" << table->horizontalHeaderItem(col)->text() << "\";";
    }
    out << "\n";

    // Записываем данные (только видимые строки, с учетом фильтров)
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->isRowHidden(row)) continue;
        for (int col = 0; col < table->columnCount(); ++col) {
            out << "\"" << table->item(row, col)->text() << "\";";
        }
        out << "\n";
    }

    file.close();
    QMessageBox::information(this, "Готово", "Данные экспортированы в CSV.");
}