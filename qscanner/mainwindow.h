#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>


#include <qbluetoothaddress.h>
#include <qbluetoothservicediscoveryagent.h>
#include <qbluetoothserviceinfo.h>
#include <qbluetoothlocaldevice.h>
#include <qbluetoothuuid.h>
#include <qbluetoothsocket.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QBluetoothDeviceDiscoveryAgent *mDiscoveryAgent;
    QBluetoothServiceDiscoveryAgent *mServiceDiscoveryAgent;
    QBluetoothSocket *mSocket;

public slots:
    void addDevice(QBluetoothDeviceInfo info);
    void addService(QBluetoothServiceInfo info);
    void addServiceDone();
    void socketRead();
    void socketConnected();
    void socketDisconnected();
    void socketError();

private slots:
    void on_servicesPushButton_clicked();

    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
