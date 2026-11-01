#include "linkmanager.h"
#include <QSerialPortInfo>
#include <QThread>
#include <QDebug>

//конструктор: создаем объект порта и подключаем сигналы
LinkManager::LinkManager(QObject* parent) : QObject(parent)
{
    m_serial = new QSerialPort(this);
    connect(m_serial, &QSerialPort::readyRead, this, &LinkManager::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &LinkManager::onError);
}

//деструктор: закрываем порт
LinkManager::~LinkManager() { disconnectPort(); }

//подключение к COM-порту
bool LinkManager::connectPort(const QString& portName, QString& errorMsg)
{
    if (m_serial->isOpen()) m_serial->close();
    if (portName.isEmpty() || portName == "-") { errorMsg = "Порт не выбран."; return false; }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(QSerialPort::Baud115200);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        errorMsg = "Не удалось открыть " + portName + ": " + m_serial->errorString();
        return false;
    }
    emit logMessage("Mega подключена к " + portName);
    return true;
}

//отключение от порта
void LinkManager::disconnectPort()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit logMessage("Mega отключена");
    }
}

//проверка соединения
bool LinkManager::isConnected() const { return m_serial->isOpen(); }

//отправка команды на Arduino
void LinkManager::sendCommand(const QString& cmd)
{
    if (m_serial->isOpen()) {
        m_serial->write((cmd + "\n").toUtf8());
        m_serial->flush();
    }
}

//авто-поиск Mega
void LinkManager::autoDiscover()
{
    const auto ports = QSerialPortInfo::availablePorts();
    emit logMessage(QString("Поиск Mega среди %1 портов...").arg(ports.size()));
    QString foundPort = "";

    for (const QSerialPortInfo& info : ports) {
        QSerialPort testPort;
        testPort.setPortName(info.portName());
        testPort.setBaudRate(QSerialPort::Baud115200);

        if (testPort.open(QIODevice::ReadWrite)) {
            QThread::msleep(1500); // ждем перезагрузки Arduino
            testPort.clear(); testPort.readAll();
            testPort.write("IDENT\n"); testPort.flush();

            if (testPort.waitForReadyRead(1000)) {
                QString response = QString::fromUtf8(testPort.readAll()).trimmed();
                if (response.contains("MEGA")) {
                    foundPort = info.portName();
                    break;
                }
            }
            testPort.close();
        }
    }
    emit discoveryFinished(foundPort);
}

//чтение данных из порта (построчно)
void LinkManager::onReadyRead()
{
    m_rxBuffer += QString::fromUtf8(m_serial->readAll());
    int idx;
    while ((idx = m_rxBuffer.indexOf('\n')) >= 0) {
        QString line = m_rxBuffer.left(idx).trimmed();
        m_rxBuffer.remove(0, idx + 1);

        if (line.isEmpty()) continue;

        // Проверяем ключевые слова
        if (line.startsWith("ACK") || line.startsWith("ERR") || line.startsWith("DONE") ||
            line.startsWith("LOG") || line.startsWith("PAUSED") || line.startsWith("TIGHTEN_RESULT")) {
            emit rawMessageReceived(line);
        }
        // Если строка содержит координаты "1,1234" — обновляем ось
        else if (line.contains(',')) {
            QStringList parts = line.split(',');
            if (parts.size() == 2) {
                emit axisUpdated(parts[0].toInt(), parts[1].toLong());
            }
        }
    }
}

//обработка ошибок порта (отключение USB)
void LinkManager::onError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        emit logMessage("Ошибка порта: " + m_serial->errorString());
        m_serial->close();
        emit connectionLost(); // сообщаем наверх об аварии
    }
}