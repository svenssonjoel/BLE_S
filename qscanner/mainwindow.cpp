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

typedef enum {
    DEVICE_ADDRESS = 0,
    DEVICE_NAME,
    DEVICE_CORE_CONF,
    DEVICE_RSSI
} table_column;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("BLE-TOOL");

   //QBluetoothLocalDevice localDevice;
   //QBluetoothAddress adapterAddress = localDevice.address();

    mDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent();

    connect(mDiscoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
            this, SLOT(addDevice(QBluetoothDeviceInfo)));
    connect(mDiscoveryAgent, SIGNAL(deviceUpdated(const QBluetoothDeviceInfo, QBluetoothDeviceInfo::Fields)),
            this, SLOT(deviceUpdated(const QBluetoothDeviceInfo, QBluetoothDeviceInfo::Fields)));
    connect(mDiscoveryAgent, SIGNAL(finished()),
            this, SLOT(deviceDiscoveryFinished()));
    connect(mDiscoveryAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)),
            this, SLOT(deviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error)));
    connect(mDiscoveryAgent, SIGNAL(canceled()),
            this, SLOT(deviceDiscoveryCanceled()));


    ui->devicesTableWidget->setColumnCount(4);
    //ui->devicesTableWidget->setRowCount(1);
    QStringList headerLabels;
    headerLabels << "Address" << "Name" << "CoreConf" << "Signal" ;
    ui->devicesTableWidget->setHorizontalHeaderLabels(headerLabels);

    ui->devicesTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->devicesTableWidget->resizeColumnsToContents();

    mDiscoveryAgent->start();
    ui->scanningIndicatorLabel->setText("Scanning");

    ui->consoleOutputTextEdit->setReadOnly(true);
    ui->consoleOutputTextEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    mNRF52SerialPort = new QSerialPort(this);

    connect(mNRF52SerialPort, &QSerialPort::readyRead,
            this, &MainWindow::on_NRF52SerialReadyRead);

    connect(ui->consoleInputLineEdit, &QLineEdit::returnPressed,
            this, &MainWindow::on_consoleSendPushButton_clicked);

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
/*
    if (!(info.name() == QString("NRF52-0101") ||
          info.name() == QString("NRF52-2121"))) {
        name = QString("[Hidden]");
        addr = QString("[Hidden]");
    }
    */

    QString str0 = addr;
    QString str1 = name;

    QBluetoothDeviceInfo::CoreConfigurations cconf = info.coreConfigurations();

    QString str2 = "";

    if (cconf.testFlag(QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) {
        str2.append(" LowEnergy");
    }
    if (cconf.testFlag(QBluetoothDeviceInfo::UnknownCoreConfiguration  )) {
        str2.append(" Unknown");
    }
    if (cconf.testFlag(QBluetoothDeviceInfo::BaseRateCoreConfiguration  )) {
        str2.append(" BaseRate");
    }
    if (cconf.testFlag(QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration  )) {
        str2.append(" BaseRate_&_LowEnergy");
    }

    QString str3 = QString::number(info.rssi(), 10);

    //QListWidgetItem *it = new QListWidgetItem();
    //it->setData(Qt::UserRole, QVariant::fromValue(info));
    //it->setText(str);

    //ui->devicesListWidget->addItem(it);
    QTableWidgetItem *it0 = new QTableWidgetItem();
    it0->setData(Qt::UserRole,QVariant::fromValue(info));
    it0->setText(str0);

    QTableWidgetItem *it1 = new QTableWidgetItem();
    it1->setText(str1);

    QTableWidgetItem *it2 = new QTableWidgetItem();
    it2->setText(str2);

    QTableWidgetItem *it3 = new QTableWidgetItem();
    it3->setText(str3);


    int row = 0;
    bool found = false;

    for (row = 0; row < ui->devicesTableWidget->rowCount(); row ++) {
        if (ui->devicesTableWidget->item(row,0)->text() == str0) {
            found = true;
            break;
        }
    }

    if (!found) {
        int row = ui->devicesTableWidget->rowCount();
        ui->devicesTableWidget->setRowCount(row + 1);
    }
    ui->devicesTableWidget->setItem(row, DEVICE_ADDRESS, it0);
    ui->devicesTableWidget->setItem(row, DEVICE_NAME, it1);
    ui->devicesTableWidget->setItem(row, DEVICE_CORE_CONF, it2);
    ui->devicesTableWidget->setItem(row, DEVICE_RSSI, it3);

}

void MainWindow::deviceUpdated(const QBluetoothDeviceInfo info, QBluetoothDeviceInfo::Fields fields)
{
    QString addrStr = info.address().toString();

    int row = 0;
    bool found = false;

    for (row = 0; row < ui->devicesTableWidget->rowCount(); row ++) {
        if (ui->devicesTableWidget->item(row,0)->text() == addrStr) {
            found = true;
            break;
        }
    }

    if (found && fields & 1) {
        QTableWidgetItem *it = new QTableWidgetItem();
        it->setText(QString::number(info.rssi(),10));
        ui->devicesTableWidget->setItem(row, DEVICE_RSSI, it);
    }
}

void MainWindow::deviceDiscoveryFinished()
{
    ui->scanningIndicatorLabel->setText("Resting");
    qDebug() << "Device discovery done!";

    if (ui->scanPeriodicallyCheckBox->isChecked()) {
        QTimer::singleShot(25000, [this]{
            ui->scanningIndicatorLabel->setText("Scanning");
            mDiscoveryAgent->start();});
    }

}

void MainWindow::deviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    qDebug() << "Device discovery error: " << error;
}

void MainWindow::deviceDiscoveryCanceled()
{
    qDebug() << "Device discovery canceled!";
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
    //QListWidgetItem *it = new QListWidgetItem();
    QTreeWidgetItem *it = new QTreeWidgetItem();

    QLowEnergyService *bleService = mBLEControl->createServiceObject(gatt);

    if (bleService) {
        connect(bleService, &QLowEnergyService::stateChanged,
                this, [this, bleService, it] (QLowEnergyService::ServiceState state) {
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
                for (auto c : bleService->characteristics()) {
                    QTreeWidgetItem *child = new QTreeWidgetItem();

                    child->setData(0,Qt::UserRole, QVariant::fromValue(c));
                    child->setText(0,c.name());
                    it->addChild(child);

                }
                break;
            case QLowEnergyService::LocalService:
                ui->bleCharacteristicsListWidget->addItem("Local Service");
                break;
            }
        });
        connect(bleService, &QLowEnergyService::characteristicChanged,
                this, &MainWindow::bleServiceCharacteristic);
        connect(bleService, &QLowEnergyService::characteristicRead,
                this, &MainWindow::bleServiceCharacteristicRead);
        //mBLEService->characteristicChanged()
        bleService->discoverDetails();
    } else {
        qDebug() << "Error connecting to BLE Service";
    }

    it->setData(0,Qt::UserRole, QVariant::fromValue(gatt));
    it->setData(1,Qt::UserRole, QVariant::fromValue(bleService));
    it->setText(0,gatt.toString());
    //it->setText(str);


    ui->bleServicesTreeWidget->addTopLevelItem(it);
    //ui->bleServicesListWidget->addItem(it);

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

    //QListWidgetItem *it = ui->devicesListWidget->currentItem();
    QTableWidgetItem *it = ui->devicesTableWidget->currentItem();

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
    //QBluetoothDeviceInfo dev = ui->devicesListWidget->currentItem()->data(Qt::UserRole).value<QBluetoothDeviceInfo>();
    int row = ui->devicesTableWidget->currentRow();
    QTableWidgetItem *it = ui->devicesTableWidget->item(row, 0);

    if (!it) {
        qDebug() << "No device selected!";
        return;
    }

    QBluetoothDeviceInfo dev = it->data(Qt::UserRole).value<QBluetoothDeviceInfo>();
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

    //ui->bleServicesListWidget->clear();
    ui->bleServicesTreeWidget->clear();
    //ui->bleCharacteristicsListWidget->clear();
}

void MainWindow::on_bleCharacteristicReadPushButton_clicked()
{
    QTreeWidgetItem *it = ui->bleServicesTreeWidget->currentItem();

    if (!it) return;

    if (it->data(0, Qt::UserRole).canConvert<QLowEnergyCharacteristic>()) {
        QLowEnergyCharacteristic ch = it->data(0,Qt::UserRole).value<QLowEnergyCharacteristic>();
        qDebug() << "Should be ok to convert to characteristic";

        // the top-level parent holds the service
        // TODO: it is not absolute guaranteed that top-level parent holds the service.
        //       Needs a more robust way of finding the enclosing service.
        QTreeWidgetItem *p = it->parent();

        while (p->parent() != nullptr) {
            p = p->parent();
        }

        if (p->data(1, Qt::UserRole).canConvert<QLowEnergyService*>()) {
            QLowEnergyService *s = p->data(1, Qt::UserRole).value<QLowEnergyService*>();
            qDebug() << "Should be ok to convert to a service..";
            s->readCharacteristic(ch);
        }
    }
}

void MainWindow::on_bleCharacteristicWritePushButton_clicked()
{
    qDebug() << "not implemented";
}

void MainWindow::on_scanPeriodicallyCheckBox_clicked(bool checked)
{
    if (!mDiscoveryAgent->isActive() && checked) {
        QTimer::singleShot(1000, [this]{
            ui->scanningIndicatorLabel->setText("Scanning");
            mDiscoveryAgent->start();});
    }
}

void MainWindow::on_ttyConnectPushButton_clicked()
{
    if (mNRF52SerialPort->isOpen()) {
        mNRF52SerialPort->close();
    }
    QString s = ui->ttyLineEdit->text();

    if (s.isNull() || s.isEmpty()) return;

    mNRF52SerialPort->setPortName(s);
    mNRF52SerialPort->setBaudRate(QSerialPort::Baud115200);
    mNRF52SerialPort->setDataBits(QSerialPort::Data8);
    mNRF52SerialPort->setParity(QSerialPort::NoParity);
    mNRF52SerialPort->setStopBits(QSerialPort::OneStop);
    mNRF52SerialPort->setFlowControl(QSerialPort::NoFlowControl);

    if(mNRF52SerialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "NRF52 SERIAL: OK!";
    } else {
        qDebug() << "NRF52 SERIAL: ERROR!";
    }

}

void MainWindow::on_NRF52SerialReadyRead()
{
    QByteArray data = mNRF52SerialPort->readAll();
    QString str = QString(data);
    //ui->consoleOutputTextEdit->setc
    QTextCursor c = ui->consoleOutputTextEdit->textCursor();
    c.movePosition(QTextCursor::End);
    ui->consoleOutputTextEdit->setTextCursor(c);

    ui->consoleOutputTextEdit->insertPlainText(str);
    QScrollBar *sb = ui->consoleOutputTextEdit->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void MainWindow::on_consoleSendPushButton_clicked()
{
    if( mNRF52SerialPort->isOpen()) {

        QString str = ui->consoleInputLineEdit->text();
        ui->consoleInputLineEdit->clear();
        str.append("\n");

        mNRF52SerialPort->write(str.toLocal8Bit());
    }
}

void MainWindow::on_scriptDirBrowsePushButton_clicked()
{
    QString str = QFileDialog::getExistingDirectory(nullptr, ("Select Output Folder"), QDir::currentPath());
    if (!str.isEmpty())
        ui->scriptDirLineEdit->setText(str);
}

void MainWindow::on_bleServicesTreeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    (void) previous;

}
