#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
typedef enum {
    CH_STRING = 0,
    CH_INT8,
    CH_INT16,
    CH_INT32,
    CH_UINT8,
    CH_UINT16,
    CH_UINT32,
    CH_HEX
} ch_type;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

   //QBluetoothLocalDevice localDevice;
   //QBluetoothAddress adapterAddress = localDevice.address();

    mDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent();

    connect(mDiscoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
            this, SLOT(addDevice(QBluetoothDeviceInfo)));

    mDiscoveryAgent->start();

}

MainWindow::~MainWindow()
{
    delete(mDiscoveryAgent);
    delete ui;
}

void MainWindow::addDevice(QBluetoothDeviceInfo info)
{

    QString name = info.name();
    QString addr = info.address().toString();
    if (info.name() != QString("NRF52-0101")) {
        name = QString("[Hidden]");
        addr = QString("[Hidden]");
    }

    QString str = QString("%1 %2").arg(addr).arg(name);

    QBluetoothDeviceInfo::CoreConfigurations cconf = info.coreConfigurations();

    str.append(" [");

    if (cconf.testFlag(QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) {
        str.append(" LowEnergy");
    }
    if (cconf.testFlag(QBluetoothDeviceInfo::UnknownCoreConfiguration  )) {
        str.append(" Unknown");
    }
    if (cconf.testFlag(QBluetoothDeviceInfo::BaseRateCoreConfiguration  )) {
        str.append(" BaseRate");
    }
    if (cconf.testFlag(QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration  )) {
        str.append(" BaseRate_&_LowEnergy");
    }
    str.append(" ]");

    QListWidgetItem *it = new QListWidgetItem();
    it->setData(Qt::UserRole, QVariant::fromValue(info));
    it->setText(str);

    ui->devicesListWidget->addItem(it);
}

void MainWindow::addService(QBluetoothServiceInfo info)
{

    QString str = QString("%1 %2").arg(info.serviceName()).arg(info.serviceUuid().toString());

    QListWidgetItem *it = new QListWidgetItem();

    it->setData(Qt::UserRole, QVariant::fromValue(info));
    it->setText(str); //info.serviceName());

    ui->servicesListWidget->addItem(it);

}

void MainWindow::addServiceError(QBluetoothDeviceDiscoveryAgent::Error error)
{
   qDebug() << error;
}

void MainWindow::addServiceDone()
{
    ui->servicesPushButton->setEnabled(true);
}

void MainWindow::socketRead()
{
    char buffer[1024];
    qint64 len;

    qDebug() << "socket read";

    while (mSocket->bytesAvailable()) {
        len = mSocket->read(buffer, 1024);
        qDebug() << len << " : " << QString(buffer);
    }
}

void MainWindow::socketConnected()
{
    qDebug() << "socket connect";
}

void MainWindow::socketDisconnected()
{
    qDebug() << "socket disconnect";
}

void MainWindow::socketError()
{
    qDebug() << "socket error";
}

void MainWindow::bleServiceDiscovered(const QBluetoothUuid &gatt)
{
    QListWidgetItem *it = new QListWidgetItem();

    it->setData(Qt::UserRole, QVariant::fromValue(gatt));

    QString str = gatt.toString();

    it->setText(str);


    ui->bleServicesListWidget->addItem(it);

}

void MainWindow::bleServiceDiscoveryFinished()
{
    qDebug() << "BLE Discovery done!";
}

void MainWindow::bleServiceStateChanged(QLowEnergyService::ServiceState state)
{
    qDebug() << "BLE Service state changed:" << state;

    switch(state) {
    case QLowEnergyService::InvalidService:
        ui->bleCharacteristicsListWidget->addItem("Invalid Service");
        break;
    case QLowEnergyService::DiscoveryRequired:
        break;
    case QLowEnergyService::DiscoveringServices:
        break;
    case QLowEnergyService::ServiceDiscovered:
        for (auto c : mBLEService->characteristics()) {
            QListWidgetItem *it = new QListWidgetItem();

            it->setData(Qt::UserRole, QVariant::fromValue(c));
            it->setText(c.name());
            ui->bleCharacteristicsListWidget->addItem(it);
        }
        break;
    case QLowEnergyService::LocalService:
        ui->bleCharacteristicsListWidget->addItem("Local Service");
        break;
    }
}

void MainWindow::bleServiceCharacteristic(const QLowEnergyCharacteristic &info, const QByteArray &value)
{
    qDebug() << "Characteristic:" << info.name() << " " << value;
}

void MainWindow::bleServiceCharacteristicRead(const QLowEnergyCharacteristic &info, const QByteArray &value)
{
    (void) info;

    int index = ui->bleCharacteristicReadTypeComboBox->currentIndex();
    QString str;

    switch(index) {
    case CH_STRING:  //String
        str = QString(value);
        break;
    case CH_INT8:  // int8;
        str = QString("%1").number(value.at(0));
        break;
    case CH_INT16:
        break;
    case CH_INT32:
        break;
    case CH_UINT8:
        break;
    case CH_UINT16:
        break;
    case CH_UINT32:
        break;
    case CH_HEX:
        for (auto c: value)  {
            str.append(QString("0x%1").number(c,16));
        }
    }

    ui->outputPlainTextEdit->appendPlainText(str);
}

void MainWindow::on_servicesPushButton_clicked()
{
    ui->servicesPushButton->setEnabled(false);
    ui->servicesListWidget->clear();

    QListWidgetItem *it = ui->devicesListWidget->currentItem();

    QBluetoothDeviceInfo info = it->data(Qt::UserRole).value<QBluetoothDeviceInfo>();

    qDebug() << info.name();
    qDebug() << info.address();


    mServiceDiscoveryAgent = new QBluetoothServiceDiscoveryAgent();
    mServiceDiscoveryAgent->setRemoteAddress(info.address());

    if (mServiceDiscoveryAgent->error()) {
        qDebug() << "wrong device address";
    }

    connect(mServiceDiscoveryAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
            this, SLOT(addService(QBluetoothServiceInfo)));
    connect(mServiceDiscoveryAgent, SIGNAL(finished()),
            this, SLOT(addServiceDone()));
    connect(mServiceDiscoveryAgent, QOverload<QBluetoothServiceDiscoveryAgent::Error>::of(&QBluetoothServiceDiscoveryAgent::error),
        [=](QBluetoothServiceDiscoveryAgent::Error error){ qDebug() << error; ui->servicesPushButton->setEnabled(true); });

    mServiceDiscoveryAgent->start();
}

void MainWindow::on_connectPushButton_clicked()
{
    QBluetoothServiceInfo remoteService;

    QListWidgetItem *it = ui->servicesListWidget->currentItem();

    if (!it) return;

    QBluetoothServiceInfo info = it->data(Qt::UserRole).value<QBluetoothServiceInfo>();

    qDebug () << info.serviceName();
    qDebug () << info.serviceUuid();
    qDebug () << info.serviceDescription();

    //info.setServiceUuid(QBluetoothUuid((quint16)0x2A19));
    qDebug () << info.serviceUuid();

    if (mSocket) {
        qDebug() << "socket exists, deleting";
        disconnect(mSocket, SIGNAL(readyRead()), this, SLOT(socketRead()));
        disconnect(mSocket, SIGNAL(connected()), this, SLOT(socketConnected()));
        disconnect(mSocket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
        //disconnect(mSocket, SIGNAL(error ()), this, SLOT(socketError()));
        mSocket->disconnectFromService();
        delete(mSocket);
    }

    // Connect to service
    mSocket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);

    qDebug() << "Create socket";
    mSocket->connectToService(info);
    qDebug() << "ConnectToService done";

    connect(mSocket, SIGNAL(readyRead()), this, SLOT(socketRead()));
    connect(mSocket, SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(mSocket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
    //connect(mSocket, SIGNAL(error()), this, SLOT(socketError()));
}


void MainWindow::on_bleConnectPushButton_clicked()
{
    QBluetoothDeviceInfo dev = ui->devicesListWidget->currentItem()->data(Qt::UserRole).value<QBluetoothDeviceInfo>();

    mBLEControl = QLowEnergyController::createCentral(dev, this);

    connect(mBLEControl, &QLowEnergyController::serviceDiscovered,
            this, &MainWindow::bleServiceDiscovered);

    connect(mBLEControl, &QLowEnergyController::discoveryFinished,
            this, &MainWindow::bleServiceDiscoveryFinished);

    connect(mBLEControl, &QLowEnergyController::connected, this, [this]() {
        (void)this; qDebug() << "connected to BLE device!";
        mBLEControl->discoverServices();
    });

    connect(mBLEControl, &QLowEnergyController::disconnected, this, [this]() {
        (void) this; qDebug() << "Disconnected from BLE device!";
    });

    mBLEControl->connectToDevice();

}

void MainWindow::on_bleDisconnectPushButton_clicked()
{
    mBLEControl->disconnectFromDevice();
}

void MainWindow::on_bleServiceConnectpushButton_clicked()
{
    QListWidgetItem *it = ui->bleServicesListWidget->currentItem();

    if (!it) return;

    QBluetoothUuid gatt = it->data(Qt::UserRole).value<QBluetoothUuid>();

    mBLEService = mBLEControl->createServiceObject(gatt);

    if (mBLEService) {
        connect(mBLEService, &QLowEnergyService::stateChanged,
                this, &MainWindow::bleServiceStateChanged);
        connect(mBLEService, &QLowEnergyService::characteristicChanged,
                this, &MainWindow::bleServiceCharacteristic);
        connect(mBLEService, &QLowEnergyService::characteristicRead,
                this, &MainWindow::bleServiceCharacteristicRead);
        //mBLEService->characteristicChanged()
        mBLEService->discoverDetails();
    } else {
        qDebug() << "Error connecting to BLE Service";
    }


}

void MainWindow::on_bleCharacteristicReadPushButton_clicked()
{
    QListWidgetItem *it = ui->bleCharacteristicsListWidget->currentItem();

    if (!it) return;

    QLowEnergyCharacteristic ch = it->data(Qt::UserRole).value<QLowEnergyCharacteristic>();

    if (mBLEService)
        mBLEService->readCharacteristic(ch);

}

void MainWindow::on_bleCharacteristicWritePushButton_clicked()
{

}
