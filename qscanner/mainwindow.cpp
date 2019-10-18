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
     QString data = QString("%1;%2").arg(info.address().toString()).arg(info.name());

     QListWidgetItem *it = new QListWidgetItem();
     it->setData(Qt::UserRole, data);
     it->setText(info.name());

     ui->devicesListWidget->addItem(it);
}

void MainWindow::addService(QBluetoothServiceInfo info)
{
    QListWidgetItem *it = new QListWidgetItem();

    QString addruuid = QString("%1;%2").arg(info.device().address().toString()).arg(info.serviceUuid().toString());

    it->setData(Qt::UserRole, QVariant::fromValue(info));
    ui->servicesListWidget->addItem(info.serviceName());
}

void MainWindow::addServiceDone()
{
    ui->servicesPushButton->setEnabled(false);
}

void MainWindow::socketRead()
{
    char buffer[1024];
    qint64 len;

    while (mSocket->bytesAvailable()) {
        len = mSocket->read(buffer, 1024);
        qDebug() << QString(buffer);
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
    if (!it) return;
    qDebug() << "data: " << it->data(Qt::UserRole).toString();
    QStringList addrname = it->data(Qt::UserRole).toString().split(';');

    QBluetoothAddress addr(addrname.at(0));

    mServiceDiscoveryAgent = new QBluetoothServiceDiscoveryAgent();
    mServiceDiscoveryAgent->setRemoteAddress(addr);

    if (mServiceDiscoveryAgent->error()) {
        qDebug() << "wrong device address";
    }

    connect(mServiceDiscoveryAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
            this, SLOT(addService(QBluetoothServiceInfo)));
    connect(mServiceDiscoveryAgent, SIGNAL(finished()),
            this, SLOT(addServiceDone()));

    mServiceDiscoveryAgent->start();
}

void MainWindow::on_pushButton_clicked()
{
    QBluetoothServiceInfo remoteService;

    QListWidgetItem *it = ui->servicesListWidget->currentItem();

    QStringList addruuid=  it->data(Qt::UserRole).toString().split(';');

    qDebug () << addruuid.at(0);
    qDebug () << addruuid.at(1);

    //QBluetoothAddress addr = QBluetoothAddress(addruuid.at(0));
    //QBluetoothDeviceInfo di(addr);

    //remoteService.setDevice(di);
    //remoteService.setServiceUuid();


    if (mSocket)
        return;

    // Connect to service
    mSocket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);
    qDebug() << "Create socket";
    mSocket->connectToService(remoteService);
    qDebug() << "ConnectToService done";



    connect(mSocket, SIGNAL(readyRead), this, SLOT(socketRead));
    connect(mSocket, SIGNAL(connected), this, SLOT(socketConnected));
    connect(mSocket, SIGNAL(disconnected), this, SLOT(socketDisconnected));
    connect(mSocket, SIGNAL(error), this, SLOT(socketError));
}
