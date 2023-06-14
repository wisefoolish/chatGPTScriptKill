#pragma once

#include"NetWorkObj.h"
#include<qstackedwidget.h>
#include "ui_ChatRoom.h"
#include<qlabel.h>

class ChatRoom : public NetWorkObj
{
	Q_OBJECT
private:
	enum msg_type {
		GPTMSG, PARTNERMSG,JUDGEMSG
	};
	void SetName(QString name);
	QString TransformToQStr(const char* str);
public:
	ChatRoom(QWidget *parent, QStackedWidget* stackedWidget,SOCKET socket);
	~ChatRoom();
	QLabel* AddNewMessage(msg_type type,std::string msg, QLabel* opeLabel);
	struct RecvParam
	{
		ChatRoom* chatRoom;
		bool* isExit;
		SOCKET theSocket;
	};
public slots:
	void DealWithMessage(const QString& str);

private:
	Ui::ChatRoomClass ui;
	SOCKET my_connect;
	QStackedWidget* stackedWidget;
	QLabel* robotChat;
	QString my_name;
};
