#pragma once
#include <QObject>
#include <QSerialPort>
#include <QStringList>

// Состояние двигателей, полученное с МК
struct MotorState {
    int state = 0;
    int x = 0;
    int y = 0;
};

// Класс управления тремя МК по последовательным портам
class LinkManager : public QObject
{
    Q_OBJECT
public:
    // Конструктор: создаёт объекты портов
    explicit LinkManager(QObject* parent = nullptr);

    // Деструктор: закрывает порты
    ~LinkManager();

    // Функция подключения МК к COM-порту
    bool connectMK(int mkIndex, const QString& portName, QString& errorMsg);

    // Функция отключения МК
    void disconnectMK(int mkIndex);

    // Функция отключения всех МК
    void disconnectAll();

    // Функция проверки, подключён ли МК
    bool isConnected(int mkIndex) const;

    // Функция отправки команды на МК
    void sendCommand(int mkIndex, const QString& cmd);

    // Функция авто-поиска МК по COM-портам
    void autoDiscover();

signals:
    // Сигнал обновления состояния двигателей
    void statusUpdated(int mkIndex, MotorState state);

    // Сигнал для вывода сообщений в лог
    void logMessage(const QString& msg);

    // Сигнал о завершении авто-поиска (возвращает найденные порты)
    void discoveryFinished(QString port1, QString port2, QString port3);

    // Сигнал о получении произвольного сообщения от МК
    void rawMessageReceived(int mkIndex, QString msg);

    // Сигнал обновления координат оси
    void axisUpdated(int axisId, long steps);

private slots:
    // Слот приёма данных с МК
    void onReadyRead(int mkIndex);

    // Слот обработки ошибок порта
    void onError(int mkIndex, QSerialPort::SerialPortError error);

private:
    // Данные одного порта: сам порт, буфер приёма, последний идентификатор
    struct PortData {
        QSerialPort* serial = nullptr;
        QString rxBuffer;
        QString lastIdent;
    };

    // Массив из трёх портов (МК1, МК2, МК3)
    PortData m_ports[3];

    // Функция разбора одной строки от МК
    void processLine(int mkIndex, const QString& line);
};