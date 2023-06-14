#pragma once

#include"NetWorkObj.h"
#include<qstackedwidget.h>
#include "ui_Hall.h"

class Hall : public NetWorkObj
{
	Q_OBJECT

public:
	Hall(QWidget *parent, QStackedWidget* stackedWidget,SOCKET socket);
	~Hall();
public slots:
	void DealWithMessage(const QString& str);
private:
	Ui::HallClass ui;
	SOCKET my_connect;
	QStackedWidget* stackedWidget;
};
