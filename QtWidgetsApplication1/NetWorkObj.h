#pragma once
#include<qwidget.h>
#include<WinSock2.h>

std::string ReplaceString(std::string str);
std::string DeReplaceString(std::string str);
void SendMessage_STF(std::string str, SOCKET sock, bool* isExit = nullptr);
std::string RecvMessage_STF(SOCKET sock, bool* isExit = nullptr);

class NetWorkObj :public QWidget
{
    Q_OBJECT
public:
    NetWorkObj(QWidget* parent) :QWidget(parent) {}
signals:
	void RecvMessage(const QString& msg);
};