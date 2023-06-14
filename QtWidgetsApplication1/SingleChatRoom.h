#pragma once

#include "ui_SingleChatRoom.h"
#include"NetWorkObj.h"
#include<qstackedwidget.h>
#include<qlabel.h>
class SingleChatRoom : public NetWorkObj
{
	Q_OBJECT

public:
	SingleChatRoom(QWidget* parent, QStackedWidget* stacked,SOCKET sock);
	~SingleChatRoom();
	QLabel* AddNewMessage(std::string msg, QLabel* opeLabel);
public slots:
	void DealWithMessage(const QString& str);
private:
	Ui::SingleChatRoomClass ui;
	SOCKET my_connect;
	QStackedWidget* stackedWidget;
	QLabel* robotChat;
};
