#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>


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
     QString str = QString("%1 %2").arg(info.address().toString()).arg(info.name());

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
