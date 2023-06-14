#include "SingleChatRoom.h"
#include<qpushbutton.h>
#include<qmessagebox.h>
#include<qscrollbar.h>
SingleChatRoom::SingleChatRoom(QWidget *parent, QStackedWidget* stacked, SOCKET sock)
	: NetWorkObj(parent)
{
	ui.setupUi(this);
	this->stackedWidget = stacked;
	this->my_connect = sock;
    this->robotChat = nullptr;
	connect(this, &SingleChatRoom::RecvMessage, this, &SingleChatRoom::DealWithMessage);
	connect(ui.btnSend, &QPushButton::clicked, [=]() {
        if(this->robotChat==nullptr)
		SendMessage_STF(ReplaceString("ask_room !askChatGPT " + ui.inputBox->toPlainText().toStdString()), this->my_connect);
        AddNewMessage("you:\n" + ui.inputBox->toPlainText().toStdString(), nullptr);
        ui.inputBox->setText("");
		});
	connect(ui.btnExit, &QPushButton::clicked, [=]() {
		SendMessage_STF(ReplaceString("exit_room"), this->my_connect);
		});
}

SingleChatRoom::~SingleChatRoom()
{}

void SingleChatRoom::DealWithMessage(const QString& strs)
{
	std::string str = strs.toStdString();
	int index = str.find(" ");
	std::string command = str.substr(0, index);
	std::string content = str.substr(index + 1, str.length());
	if (command == "Error:")
	{
		QMessageBox::warning(this, "Error", content.c_str());
	}
	else if (command == "room_ask")
	{
        index = content.find(" ");
        std::string state = content.substr(0, index);
        std::string theContent = content.substr(index + 1, content.length());
        if (state == "!askChatGPT")
        {
            index = theContent.find(" ");
            command = theContent.substr(0, index);
            content = theContent.substr(index + 1, content.length());
            if (command == "BEGIN" && robotChat == nullptr)
            {
                robotChat = AddNewMessage("robot:\n", nullptr);
            }
            else if (command == "ING" && robotChat != nullptr)
            {
                AddNewMessage(content, robotChat);
            }
            else if (command == "FIN" && robotChat != nullptr)
            {
                robotChat = nullptr;
            }
            else if (command == "ERROR" && robotChat != nullptr)
            {
                QMessageBox::warning(this, "Error", "maybe server have some network error");
                robotChat = nullptr;
                AddNewMessage("Error: chatGPT did not finish answering, need to be asked again",nullptr);
            }
            else
            {
                QMessageBox::warning(this, "Error", "something wrong with ChatGPT ask");
                robotChat = nullptr;
                AddNewMessage("Error: Some network error that needs to be asked again",nullptr);
            }
        }
        
	}
    else if (command == "complete:")
    {
        if (content == "exit room succeed")
        {
            this->stackedWidget->setCurrentIndex(0);
        }
    }
}

QLabel* SingleChatRoom::AddNewMessage(std::string msg, QLabel* opeLabel)
{
    QLabel* label = nullptr;
    if (opeLabel == nullptr)
    {
        label = new QLabel;
        label->setText(QString::fromStdString(msg));
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        ui.MsgBoxLayout->insertWidget(ui.MsgBoxLayout->count() - 1, label);
    }
    else
    {
        label = opeLabel;
        opeLabel->setText(opeLabel->text() + QString::fromStdString(msg));
    }
    ui.scrollArea->verticalScrollBar()->setValue(ui.scrollArea->verticalScrollBar()->maximum());
    return label;
}