#include "Hall.h"
#include<qmessagebox.h>
#include<qinputdialog.h>

Hall::Hall(QWidget *parent, QStackedWidget* stackedWidget,SOCKET socket)
	: NetWorkObj(parent)
{
	ui.setupUi(this);
    this->my_connect = socket;
    this->stackedWidget = stackedWidget;
    connect(ui.createRoom, &QPushButton::clicked, [=]() {
        SendMessage_STF(ReplaceString("create_room"), this->my_connect);
        });
    connect(this, &Hall::RecvMessage, this, &Hall::DealWithMessage);
    connect(ui.btnEnter, &QPushButton::clicked, [=]() {
        QString text = QInputDialog::getText(this, "input box", "please input room number:");
        if(text!="")
        SendMessage_STF(ReplaceString("enter_room " + text.toStdString()), this->my_connect);
        });
    connect(ui.singleChat, &QPushButton::clicked, [=]() {
        SendMessage_STF(ReplaceString("single_Chat"), this->my_connect);
        });
}

Hall::~Hall()
{}

void Hall::DealWithMessage(const QString& strs)
{
    std::string str = strs.toStdString();
    int index = str.find(" ");
    std::string command = str.substr(0, index);
    std::string content = str.substr(index + 1, str.length());
    if (command == "Error:")
    {
        QMessageBox::warning(this, "Error", content.c_str());
    }
    else if (command == "complete:")
    {
        if (content == "enter room succeed")
        {
            this->stackedWidget->setCurrentIndex(1);
            SendMessage_STF(ReplaceString("ask_room ?room_id "), this->my_connect);
        }
        else if (content == "single chat succeed")
        {
            this->stackedWidget->setCurrentIndex(2);
        }
    }
}
