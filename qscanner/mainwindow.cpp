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
    ui->servicesListWidget->addItem(info.serviceName());
}

void MainWindow::addServiceDone()
{
    ui->servicesPushButton->setEnabled(false);
}


void MainWindow::on_servicesPushButton_clicked()
{
    ui->servicesPushButton->setEnabled(false);
    ui->servicesListWidget->clear();

    qDebug() << "Looking for services";

    QListWidgetItem *it = ui->devicesListWidget->currentItem();
    if (!it) return;
    qDebug() << "data: " << it->data(Qt::UserRole).toString();
    QStringList addrname = it->data(Qt::UserRole).toString().split(';');

    qDebug() << addrname.at(0);
    qDebug() << addrname.at(1);


    QBluetoothAddress addr(addrname.at(0));

    qDebug() << "ble addr: " << addr.toString();

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
