#include "linkmanager.h"
#include <QSerialPortInfo>
#include <QThread>
#include <QDebug>

// Конструктор: создаёт 3 порта и подписывается на их сигналы
LinkManager::LinkManager(QObject* parent) : QObject(parent)
{
    for (int i = 0; i < 3; ++i) {
        m_ports[i].serial = new QSerialPort(this);
        connect(m_ports[i].serial, &QSerialPort::readyRead, this, [this, i]() { onReadyRead(i); });
        connect(m_ports[i].serial, &QSerialPort::errorOccurred, this, [this, i](QSerialPort::SerialPortError err) { onError(i, err); });
    }
}

// Деструктор: закрывает все открытые порты
LinkManager::~LinkManager() { disconnectAll(); }

// Функция подключения МК к заданному COM-порту
bool LinkManager::connectMK(int mkIndex, const QString& portName, QString& errorMsg)
{
    if (mkIndex < 0 || mkIndex > 2) return false;
    QSerialPort* port = m_ports[mkIndex].serial;

    if (port->isOpen()) port->close();
    if (portName.isEmpty() || portName == "-") { errorMsg = "Порт не выбран."; return false; }

    port->setPortName(portName);
    port->setBaudRate(QSerialPort::Baud115200);

    if (!port->open(QIODevice::ReadWrite)) {
        errorMsg = "Не удалось открыть " + portName + ": " + port->errorString();
        return false;
    }

    // Arduino перезагружается при открытии порта — ждём и сбрасываем мусор загрузчика
    port->clear();
    QThread::msleep(100);
    port->readAll();

    emit logMessage(QString("МК%1 подключен к %2").arg(mkIndex + 1).arg(portName));
    return true;
}

// Функция отключения МК
void LinkManager::disconnectMK(int mkIndex)
{
    if (mkIndex < 0 || mkIndex > 2) return;
    if (m_ports[mkIndex].serial->isOpen()) {
        m_ports[mkIndex].serial->close();
        emit logMessage(QString("МК%1 отключен").arg(mkIndex + 1));
    }
}

// Функция отключения всех МК
void LinkManager::disconnectAll() { for (int i = 0; i < 3; ++i) disconnectMK(i); }

// Функция проверки, подключён ли МК
bool LinkManager::isConnected(int mkIndex) const
{
    if (mkIndex < 0 || mkIndex > 2) return false;
    return m_ports[mkIndex].serial->isOpen();
}

// Функция отправки команды на МК
void LinkManager::sendCommand(int mkIndex, const QString& cmd)
{
    if (mkIndex < 0 || mkIndex > 2 || !m_ports[mkIndex].serial) return;
    if (m_ports[mkIndex].serial->isOpen()) {
        m_ports[mkIndex].serial->write((cmd + "\n").toUtf8());
        m_ports[mkIndex].serial->flush();
        qDebug() << "Отправлено на МК" << (mkIndex + 1) << ":" << cmd;
    }
}

// Функция авто-поиска МК по COM-портам
void LinkManager::autoDiscover()
{
    QString foundPorts[3] = { "", "", "" };
    const auto ports = QSerialPortInfo::availablePorts();
    emit logMessage(QString("Сканирую %1 COM-портов...").arg(ports.size()));

    for (const QSerialPortInfo& info : ports) {
        QSerialPort testPort;
        testPort.setPortName(info.portName());
        testPort.setBaudRate(QSerialPort::Baud115200);

        if (testPort.open(QIODevice::ReadWrite)) {
            QThread::msleep(1500); // ждём перезагрузки Arduino
            bool found = false;
            // Пробуем несколько раз — Arduino может не успеть ответить с первого
            for (int attempt = 0; attempt < 3 && !found; ++attempt) {
                testPort.clear(); testPort.readAll();
                testPort.write("IDENT\n"); testPort.flush();
                if (testPort.waitForReadyRead(600)) {
                    QString response = QString::fromUtf8(testPort.readAll()).trimmed();
                    if (response.contains("MK1")) { foundPorts[0] = info.portName(); found = true; }
                    else if (response.contains("MK2")) { foundPorts[1] = info.portName(); found = true; }
                    else if (response.contains("MK3")) { foundPorts[2] = info.portName(); found = true; }
                }
                if (!found) QThread::msleep(300);
            }
            testPort.close();
        }
    }
    emit discoveryFinished(foundPorts[0], foundPorts[1], foundPorts[2]);
}

// Функция приёма данных с МК (вызывается по сигналу readyRead)
void LinkManager::onReadyRead(int mkIndex)
{
    if (mkIndex < 0 || mkIndex > 2 || !m_ports[mkIndex].serial) return;

    PortData& data = m_ports[mkIndex];
    data.rxBuffer += QString::fromUtf8(data.serial->readAll());

    // Разбираем буфер построчно по '\n'
    int idx;
    while ((idx = data.rxBuffer.indexOf('\n')) >= 0) {
        QString line = data.rxBuffer.left(idx).trimmed();
        data.rxBuffer.remove(0, idx + 1);

        if (!line.isEmpty()) {
            qDebug() << "Получено от МК" << (mkIndex + 1) << ":" << line;
            processLine(mkIndex, line);
        }
    }
}

// Функция разбора одной строки от МК
void LinkManager::processLine(int mkIndex, const QString& line)
{
    if (line == "MK1" || line == "MK2" || line == "MK3") {
        m_ports[mkIndex].lastIdent = line;
        return;
    }
    if (line.startsWith("ACK") || line.startsWith("ERR")) {
        emit logMessage(QString("[МК%1] %2").arg(mkIndex + 1).arg(line));
        return;
    }

    // Координаты: "axisId,steps" (например, "1,1024" или "3,-500")
    if (line.contains(',')) {
        QStringList parts = line.split(',');
        if (parts.size() == 2) {
            int axisId = parts[0].toInt();
            long steps = parts[1].toLong();
            emit axisUpdated(axisId, steps);
        }
    }
}

// Функция обработки ошибок порта
void LinkManager::onError(int mkIndex, QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        if (m_ports[mkIndex].serial) {
            // Блокируем сигналы, чтобы close() не породил повторную ошибку
            m_ports[mkIndex].serial->blockSignals(true);
            emit logMessage(QString("Ошибка порта МК%1: %2").arg(mkIndex + 1).arg(m_ports[mkIndex].serial->errorString()));
            m_ports[mkIndex].serial->close();
            m_ports[mkIndex].serial->blockSignals(false);
        }
    }
}