#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>


#include <qbluetoothaddress.h>
#include <qbluetoothservicediscoveryagent.h>
#include <qbluetoothserviceinfo.h>
#include <qbluetoothlocaldevice.h>
#include <qbluetoothuuid.h>
#include <qbluetoothsocket.h>
#include <qlowenergycontroller.h>
#include <qlowenergyservice.h>
#include <qlowenergycharacteristic.h>
#include <qlowenergycharacteristicdata.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();



public slots:
    void addDevice(QBluetoothDeviceInfo info);
    void addService(QBluetoothServiceInfo info);
    void addServiceError(QBluetoothDeviceDiscoveryAgent::Error);
    void addServiceDone();
    void socketRead();
    void socketConnected();
    void socketDisconnected();
    void socketError();
    void bleServiceDiscovered(const QBluetoothUuid &gatt);
    void bleServiceDiscoveryFinished();
    void bleServiceStateChanged(QLowEnergyService::ServiceState);
    void bleServiceCharacteristic(const QLowEnergyCharacteristic &info,
                                  const QByteArray &value);
    void bleServiceCharacteristicRead(const QLowEnergyCharacteristic &info,
                                      const QByteArray &value);

private slots:
    void on_servicesPushButton_clicked();
    void on_connectPushButton_clicked();
    void on_bleConnectPushButton_clicked();
    void on_bleDisconnectPushButton_clicked();

    void on_bleServiceConnectpushButton_clicked();

    void on_bleCharacteristicReadPushButton_clicked();

    void on_bleCharacteristicWritePushButton_clicked();

private:
    Ui::MainWindow *ui;

    QBluetoothDeviceDiscoveryAgent *mDiscoveryAgent = nullptr;
    QBluetoothServiceDiscoveryAgent *mServiceDiscoveryAgent = nullptr;
    QBluetoothSocket *mSocket = nullptr;

    QLowEnergyController *mBLEControl = nullptr;
    QLowEnergyService    *mBLEService = nullptr;


};
#endif // MAINWINDOW_H
