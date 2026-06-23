#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkInterface>
#include <QTableWidgetItem>

namespace {
constexpr quint16 kDiscoveryPort = 19198; // 设备发现端口，按协议修改

bool parseIpLastOctet(const QString &ip, int *lastOctet)
{
    const int dotIndex = ip.lastIndexOf(QLatin1Char('.'));
    if (dotIndex < 0 || !lastOctet) {
        return false;
    }

    bool ok = false;
    const int value = ip.mid(dotIndex + 1).toInt(&ok);
    if (!ok || value < 0 || value > 255) {
        return false;
    }

    *lastOctet = value;
    return true;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);

    ui->btnMinimize->setFlat(true);
    ui->btnMinimize->setFixedSize(64, 32);
    ui->btnMinimize->setText(QString());

    ui->btnClose->setFlat(true);
    ui->btnClose->setFixedSize(64, 32);
    ui->btnClose->setText(QString());

    InitTableInfo();

    statusLab = new QLabel(this);
    statusLab->setSizePolicy(QSizePolicy::Expanding,
                             QSizePolicy::Preferred);
    statusLab->setMinimumWidth(250);
    ui->statusBar->addWidget(statusLab, 1);

    //--------------------------------
    // 创建 socket
    //--------------------------------

    m_socket = new QUdpSocket(this);

    //--------------------------------
    // 绑定端口
    //--------------------------------

    bool ok = m_socket->bind(
        QHostAddress::AnyIPv4,
        kDiscoveryPort,
        QUdpSocket::ShareAddress |
            QUdpSocket::ReuseAddressHint);

    if (!ok)
    {
        qDebug() << "bind failed";
    }

    //--------------------------------
    // 收数据
    //--------------------------------

    connect(m_socket,
            &QUdpSocket::readyRead,
            this,
            &MainWindow::onReadyRead);

    //--------------------------------
    // 定时器
    //--------------------------------

    m_timer = new QTimer(this);

    m_timer->setSingleShot(true);

    connect(m_timer,
            &QTimer::timeout,
            this,
            &MainWindow::onSearchFinished);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::InitTableInfo()
{
    ui->tableInfo->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    ui->tableInfo->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    QStringList headerText;
    headerText<<"选择"<<"ip"<<"通道号"<<"前后视";
    ui->tableInfo->setColumnCount(headerText.size());

    camDelegate = new TComboBoxDelegate(this);
    QStringList camList;
    camList <<"前视"<<"后视";
    camDelegate->setItems(camList, false);
    ui->tableInfo->setItemDelegateForColumn(3, camDelegate);     // 前后视

    chnDelegate = new TComboBoxDelegate(this);
    QStringList chnList;
    for(int idx = 1; idx <= 20; idx++) {
        chnList.append(QString::number(idx));
    }
    chnDelegate->setItems(chnList, false);
    ui->tableInfo->setItemDelegateForColumn(2, chnDelegate);     // 通道

    for(int i=0; i < ui->tableInfo->columnCount(); i++) {
        QTableWidgetItem *headItem = new QTableWidgetItem(headerText.at(i));
        QFont font = headItem->font();
        font.setBold(true);
        font.setPointSize(11);
        headItem->setFont(font);
        ui->tableInfo->setHorizontalHeaderItem(i, headItem);
    }
}

bool MainWindow::AddTableItem(const QString &ip)
{
    if (ip.isEmpty()) {
        return false;
    }

    for (int row = 0; row < ui->tableInfo->rowCount(); ++row) {
        const QTableWidgetItem *item = ui->tableInfo->item(row, 1);
        if (item && item->text() == ip) {
            return true;
        }
    }
    const int row = ui->tableInfo->rowCount();
    ui->tableInfo->insertRow(row);

    auto *checkItem = new QTableWidgetItem();
    checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    checkItem->setCheckState(Qt::Unchecked);
    ui->tableInfo->setItem(row, 0, checkItem);

    auto *ipItem = new QTableWidgetItem(ip);
    ipItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->tableInfo->setItem(row, 1, ipItem);

    QString channelText;
    QString viewText;
    int lastOctet = 0;
    if (parseIpLastOctet(ip, &lastOctet)) {
        const int remainder = lastOctet - 126;
        const int channel = remainder / 2 + 1;
        channelText = QString::number(channel);
        viewText = (remainder % 2 == 0) ? QStringLiteral("前视") : QStringLiteral("后视");
    }

    ui->tableInfo->setItem(row, 2, new QTableWidgetItem(channelText));
    ui->tableInfo->setItem(row, 3, new QTableWidgetItem(viewText));
    return true;
}

bool MainWindow::isLocalAddress(const QHostAddress &addr)
{
    if (addr.isLoopback())
        return true;
    const auto peer = addr.toIPv4Address();
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol
                && entry.ip().toIPv4Address() == peer) {
                return true;
            }
        }
    }
    return false;
}

QString MainWindow::GetIp(const QByteArray &json) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(
                    json,
                    &error);

    if (error.error != QJsonParseError::NoError)
    {
        qDebug()
            << "json parse error:"
            << error.errorString() <<", value:" << json;

                return "";
    }

    QJsonObject obj = doc.object();
    return obj["ip"].toString();
  }

void MainWindow::on_btnSearchDev_clicked()
{
    //--------------------------------
    // 防止重复搜索
    //--------------------------------

    if (m_searching)
    {
        return;
    }

    m_searching = true;
    m_devNum = 0;

    ui->tableInfo->clearContents();
    ui->tableInfo->setRowCount(0);

    statusLab->setText("正在检索设备,请耐心等待...");


    //--------------------------------
    // 发送广播
    //--------------------------------

    QByteArray msg = "DISCOVER_DEVICE";
#if 0

    qint64 ret = m_socket->writeDatagram(
        msg,
        QHostAddress::Broadcast,
        kDiscoveryPort);

    if (ret < 0)
    {
        statusLab->setText("发送广播消息失败!");
        return;
    }
#endif
    QList<QNetworkInterface> interfaces =
        QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces)
    {
        if (!(iface.flags() & QNetworkInterface::IsUp))
            continue;

        if (!(iface.flags() & QNetworkInterface::IsRunning))
            continue;

        for (const auto& entry : iface.addressEntries())
        {
            QHostAddress broadcast = entry.broadcast();

            if (broadcast.isNull())
                continue;

            qDebug()
                << iface.humanReadableName()
                << broadcast.toString();

            m_socket->writeDatagram(
                msg,
                broadcast,
                kDiscoveryPort);
        }
    }

    qDebug() << "broadcast sent";
    m_timer->start(5000);
}

void MainWindow::onReadyRead()
{
    //--------------------------------
    // 读取所有 datagram
    //--------------------------------

    while (m_socket->hasPendingDatagrams())
    {
        QByteArray buffer;

        buffer.resize(
            m_socket->pendingDatagramSize());

        QHostAddress sender;

        quint16 senderPort;

        //--------------------------------
        // 读取数据
        //--------------------------------

        m_socket->readDatagram(
            buffer.data(),
            buffer.size(),
            &sender,
            &senderPort);

        //--------------------------------
        // 过滤本机
        //--------------------------------

        if (isLocalAddress(sender))
        {
            continue;
        }

        qDebug()
            << "\n========== DEVICE ==========";

        qDebug()
            << "from ip:"
            << sender.toString();

        qDebug()
            << buffer;

        //--------------------------------
        // 获取IP
        //--------------------------------

        QString ip =
            GetIp(buffer);

        //--------------------------------
        // 加入表格
        //--------------------------------

        bool addRet =
            AddTableItem(ip);

        if (addRet)
        {
            ++m_devNum;
        }
    }
}


void MainWindow::on_btnModifyIP_clicked()
{
    statusLab->clear();

    bool choose{false};
    for (int row = 0;
         row < ui->tableInfo->rowCount();
         ++row)
    {
        //--------------------------------
        // 获取第0列
        //--------------------------------

        QTableWidgetItem* checkItem =
            ui->tableInfo->item(row, 0);
        if (!checkItem)
        {
            continue;
        }

        //--------------------------------
        // 判断是否选中
        //--------------------------------

        if (checkItem->checkState()
            == Qt::Checked)
        {
            choose = true;
            // 获取ip
            QTableWidgetItem* item0 = ui->tableInfo->item(row, 1);
            // 获取通道号
            QTableWidgetItem* item1 = ui->tableInfo->item(row, 2);
            // 获取前后视
            QTableWidgetItem* item2 = ui->tableInfo->item(row, 3);
            if (item0 && item1 && item2)
            {
                QString old_ip = item0->text();
                int chn = item1->text().toInt();
                int visi = (item2->text() == "前视") ? 0 : 1;
                int last_ip = 126 + (chn-1)*2 + visi;
                QString new_ip = QString("192.168.4.%1").arg(last_ip);
                statusLab->setText(QString("开始修改ip, 原ip：%1 修改为：%2,请等待...").arg(old_ip).arg(new_ip));
                auto bret = ModifyIP(old_ip, new_ip);
                if(!bret) {
                    statusLab->setText(QString("修改ip, 原ip：%1 修改为：%2 失败!").arg(old_ip).arg(new_ip));
                } else {
                    statusLab->setText(QString("修改ip, 原ip：%1 修改为：%2 成功!").arg(old_ip).arg(new_ip));
                }
            }
        }
    }

    if(!choose) {
        statusLab->setText("您未选择任何设备! 请选择需要修改ip的设备!");
    }
}

void MainWindow::onSearchFinished()
{
    //--------------------------------
    // 搜索结束
    //--------------------------------

    m_searching = false;

    //--------------------------------
    // 更新状态
    //--------------------------------
    statusLab->setText(
        QString(
            "搜索完毕，共检索到%1个设备")
            .arg(m_devNum));
}

QByteArray MainWindow::makePacket(const QString& ip)
{
    QByteArray packet;

    //--------------------------------
    // 2个字节的包头
    //--------------------------------
    packet.append(char(0xA5));
    packet.append(char(0x5A));

    //--------------------------------
    // 2个地址（2字节）
    // 大端模式
    //--------------------------------
    packet.append(char(0x00));
    packet.append(char(0x01));

    // 2个字节的命令码
    packet.append(char(0x00));
    packet.append(char(0x11));

    //--------------------------------
    // IP字符串
    //--------------------------------
    QByteArray ipData =
        ip.toUtf8();

    //--------------------------------
    // IP长度
    //--------------------------------
    quint16 len =
        ipData.size();

    //--------------------------------
    // 长度（2字节）
    //--------------------------------
    packet.append(
        char((len >> 8) & 0xFF));

    packet.append(
        char(len & 0xFF));

    //--------------------------------
    // IP内容
    //--------------------------------
    packet.append(ipData);

    // 2个字节的crc
    packet.append(char(0x01));
    packet.append(char(0x01));

    // 2个字节的包位
    packet.append(char(0xFF));
    packet.append(char(0xFF));

    return packet;
}

bool MainWindow::ModifyIP(const QString& old_ip, const QString& new_ip)
{
    QUdpSocket socket;

    //--------------------------------
    // 绑定本地端口
    //--------------------------------

    const int udp_port = 19122;
    bool ok =
        socket.bind(
            QHostAddress::AnyIPv4,
            udp_port);

    if (!ok)
    {
        statusLab->setText(QString("绑定端口[%1]失败!").arg(udp_port));
        return false;
    }

    //--------------------------------
    // 发送
    //--------------------------------

    QByteArray data = makePacket(new_ip);

    socket.writeDatagram(
        data,
        QHostAddress(old_ip),
        9193);

    qDebug() << "send success";

    //--------------------------------
    // 等待响应
    //--------------------------------

    bool hasData =
        socket.waitForReadyRead(3000);

    if (!hasData)
    {
        statusLab->setText(QString("接收数据等待超时，ip:[%1]失败!").arg(old_ip));
        return false;
    }

    //--------------------------------
    // 接收
    //--------------------------------

    QByteArray buffer;

    buffer.resize(
        socket.pendingDatagramSize());
    QHostAddress sender;
    quint16 senderPort;
    socket.readDatagram(
        buffer.data(),
        buffer.size(),
        &sender,
        &senderPort);
    return parsePacket(buffer);
}


bool MainWindow::parsePacket(
    const QByteArray& data)
{
    //--------------------------------
    // 修改ip响应包长度11个字节
    //--------------------------------

    if (data.size() < 11)
    {
        qDebug()
            << "packet too short, data size:" << data.size();

        return false;
    }

    //--------------------------------
    // 包头
    //--------------------------------

    quint8 head1 =
        quint8(data[0]);

    quint8 head2 =
        quint8(data[1]);

    if (head1 != 0xA5 ||
        head2 != 0x5A)
    {
        qDebug()
            << "invalid header";

        return false;
    }

    quint8 result =
        quint8(data[8]);

    qDebug() << "result:" << result ;
    return (result == 0x01);
}

void MainWindow::mousePressEvent(
    QMouseEvent *event)
{
    //--------------------------------
    // 左键按下
    //--------------------------------

    if (event->button() == Qt::LeftButton)
    {
        m_drag = true;

        //--------------------------------
        // 记录鼠标相对窗口位置
        //--------------------------------

        m_dragPos =
            event->globalPosition().toPoint()
            - frameGeometry().topLeft();

        event->accept();
    }
}

void MainWindow::mouseMoveEvent(
    QMouseEvent *event)
{
    //--------------------------------
    // 左键拖动
    //--------------------------------

    if (m_drag &&
        (event->buttons()
         & Qt::LeftButton))
    {
        //--------------------------------
        // 移动窗口
        //--------------------------------

        move(
            event->globalPosition().toPoint()
            - m_dragPos);

        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(
    QMouseEvent *event)
{
    Q_UNUSED(event);

    m_drag = false;
}
