#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QUdpSocket>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QHostAddress>
#include "tcomboboxdelegate.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    //--------------------------------
    // 鼠标按下
    //--------------------------------

    void mousePressEvent(
        QMouseEvent *event) override;

    //--------------------------------
    // 鼠标移动
    //--------------------------------

    void mouseMoveEvent(
        QMouseEvent *event) override;

    //--------------------------------
    // 鼠标释放
    //--------------------------------

    void mouseReleaseEvent(
        QMouseEvent *event) override;

private:
    bool isLocalAddress(const QHostAddress &addr);
    QString GetIp(const QByteArray& json);
    void InitTableInfo();
    bool AddTableItem(const QString& ip);

private slots:
    void on_btnSearchDev_clicked();
    void on_btnModifyIP_clicked();

    //--------------------------------
    // 收到设备回复
    //--------------------------------
    void onReadyRead();

    //--------------------------------
    // 搜索结束
    //--------------------------------
    void onSearchFinished();

    bool ModifyIP(const QString& old_ip, const QString& new_ip);

    QByteArray makePacket(const QString& ip);

    bool parsePacket(const QByteArray& data);

private:
    QLabel* statusLab;
    bool m_searching{false};
    int m_devNum{0};
    const quint16 kDiscoveryPort{19198};
    //--------------------------------
    // UDP socket
    //--------------------------------
    QUdpSocket* m_socket;

    //--------------------------------
    // 搜索超时 timer
    //--------------------------------
    QTimer* m_timer;

    Ui::MainWindow *ui;
    TComboBoxDelegate *camDelegate;         // 前后视
    TComboBoxDelegate *chnDelegate;         // 通道

    //--------------------------------
    // 是否拖动
    //--------------------------------

    bool m_drag = false;

    //--------------------------------
    // 拖动偏移
    //--------------------------------

    QPoint m_dragPos;
};
#endif // MAINWINDOW_H
