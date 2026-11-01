#pragma once
#include <QObject>
#include <QSerialPort>
#include <QStringList>

// ================= МЕНЕДЖЕР СВЯЗИ С ARDUINO =================
// Отвечает за открытие/закрытие COM-порта, отправку команд и прием ответов.
// Работает в асинхронном режиме: при получении данных испускает сигналы.
class LinkManager : public QObject
{
    Q_OBJECT
public:
    explicit LinkManager(QObject* parent = nullptr);
    ~LinkManager();

    // Подключиться к указанному COM-порту. Возвращает true при успехе.
    bool connectPort(const QString& portName, QString& errorMsg);

    // Отключиться от порта.
    void disconnectPort();

    // Проверить, открыт ли порт прямо сейчас.
    bool isConnected() const;

    // Отправить строковую команду на Arduino (добавляет \n в конце).
    void sendCommand(const QString& cmd);

    // Автоматически найти Arduino Mega среди всех COM-портов.
    // Отправляет IDENT и ждет ответа MEGA.
    void autoDiscover();

signals:
    void logMessage(const QString& msg);        // Логирование (для отображения)
    void discoveryFinished(QString port);       // Поиск завершен (порт или пусто)
    void rawMessageReceived(QString msg);       // Пришло сообщение от Arduino (DONE/ERR/LOG)
    void axisUpdated(int axisId, long steps);   // Обновление координат (1/2/3, позиция)
    void connectionLost();                      // Потеря связи с Arduino

private slots:
    void onReadyRead();                         // Обработка входящих данных
    void onError(QSerialPort::SerialPortError); // Обработка ошибок порта

private:
    QSerialPort* m_serial;                      // Объект COM-порта
    QString m_rxBuffer;                         // Буфер для приема данных
};