#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>


#include <qbluetoothaddress.h>
#include <qbluetoothservicediscoveryagent.h>
#include <qbluetoothserviceinfo.h>
#include <qbluetoothlocaldevice.h>
#include <qbluetoothuuid.h>

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

public slots:
    void addDevice(QBluetoothDeviceInfo info);
    void addService(QBluetoothServiceInfo info);
    void addServiceDone();
private slots:
    void on_servicesPushButton_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
