#include <cstring>
#include <QTimer>
#include "DanmakuScreen.hpp"

DanmakuScreen::DanmakuScreen(QObject *parent) : QObject(parent),
    _decoder{std::unique_ptr<blilive::DanmakuPacketDecoder>(new blilive::DanmakuPacketDecoder)} {
    _heartbeatTimer.callOnTimeout(this, &DanmakuScreen::sendHeartbeat);
}

void DanmakuScreen::connectLiveRoom(int roomID) {
    this->stopHeartbeat();
    _socket = std::unique_ptr<QWebSocket>(new QWebSocket);

    _decoder.get()->mCommandMessageHandler = [&](blilive::DanmakuCommandPacket *p, std::string c) {
        if (c == blilive::DanmakuCommandPacket::Command::RecvDanmaku) {
            auto danmu = static_cast<blilive::DanmakuCommandDanmuMessagePacket *>(p);
            QString text = QString::fromStdString(danmu->text()).toUtf8();
            QString author = QString::fromStdString(danmu->authorNick()).toUtf8();
            qDebug()<<author<<": "<<text<<"\r\n";
        }
    };

    QObject::connect(_socket.get(), &QWebSocket::connected, this, [&, roomID]() {
       printf("Connected\n");
       blilive::DanmakuAuthPacket auth;
       auth.mRoomID = roomID;
       std::string data = auth.encode();
       this->_socket->sendBinaryMessage(QByteArray(data.c_str(), static_cast<int>(data.size())));
       this->startHeartbeat();
    });

    QObject::connect(_socket.get(), &QWebSocket::binaryMessageReceived, this, [&](const QByteArray &message) {
       this->_decoder.get()->dispatch(std::string(message.data(), static_cast<size_t>(message.size())));
    });

    QObject::connect(_socket.get(), &QWebSocket::disconnected, this, [&]() {
        this->stopHeartbeat();
    });

    _socket->open((QUrl(QString("wss://broadcastlv.chat.bilibili.com:2245/sub"))));
}

void DanmakuScreen::startHeartbeat() {
    _heartbeatTimer.start(std::chrono::seconds(30));
}

void DanmakuScreen::stopHeartbeat() {
    _heartbeatTimer.stop();
}

void DanmakuScreen::sendHeartbeat() {
    blilive::DanmakuHeartbeatPacket heartbeat;
    _socket->sendBinaryMessage(QByteArray::fromStdString(heartbeat.encode()));
}
