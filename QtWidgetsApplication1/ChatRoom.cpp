#include "ChatRoom.h"
#include<qmessagebox.h>
#include<qlabel.h>
#include<qscrollbar.h>
#include<qinputdialog.h>

void ChatRoom::DealWithMessage(const QString& strs)
{
    std::string str=strs.toStdString();
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
        if (state == "!message")
        {
            this->AddNewMessage(PARTNERMSG,theContent, nullptr);
        }
        else if (state == "?room_id")
        {
            ui.RoomNumber->setText(("room_id:" + theContent).c_str());
        }
        else if (state == "!askChatGPT")
        {
            index = theContent.find(" ");
            command = theContent.substr(0, index);
            content = theContent.substr(index + 1, content.length());
            if (command == "BEGIN" && robotChat==nullptr)
            {
                robotChat = AddNewMessage(GPTMSG,"robot:\n", nullptr);
            }
            else if (command == "ING"&& robotChat != nullptr)
            {
                AddNewMessage(GPTMSG,content, robotChat);
            }
            else if (command == "FIN"&& robotChat != nullptr)
            {
                robotChat = nullptr;
            }
            else if (command == "ERROR" && robotChat != nullptr)
            {
                QMessageBox::warning(this, "Error", "maybe server have some network error");
                AddNewMessage(GPTMSG, "Error: chatGPT did not finish answering, need to be asked again", nullptr);
                robotChat = nullptr;
            }
            else
            {
                QMessageBox::warning(this, "Error", "something wrong with ChatGPT ask");
                AddNewMessage(GPTMSG, "Error: Some network error that needs to be asked again", nullptr);
                robotChat = nullptr;
            }
        }
        else if (state == "!askChatGPTForJudge")
        {
            index = theContent.find(" ");
            command = theContent.substr(0, index);
            content = theContent.substr(index + 1, content.length());
            if (command == "BEGIN" && robotChat == nullptr)
            {
                robotChat = AddNewMessage(JUDGEMSG, "robot:\n", nullptr);
            }
            else if (command == "ING" && robotChat != nullptr)
            {
                AddNewMessage(JUDGEMSG, content, robotChat);
            }
            else if (command == "FIN" && robotChat != nullptr)
            {
                robotChat = nullptr;
            }
            else if (command == "ERROR" && robotChat != nullptr)
            {
                QMessageBox::warning(this, "Error", "maybe server have some network error");
                AddNewMessage(JUDGEMSG, "Error: chatGPT did not finish answering, need to be asked again", nullptr);
                robotChat = nullptr;
            }
            else
            {
                QMessageBox::warning(this, "Error", "something wrong with ChatGPT ask");
                robotChat = nullptr;
                AddNewMessage(JUDGEMSG, "Error: Some network error that needs to be asked again", nullptr);
            }
        }
        else if (state == "!tellChatGPT")
        {
            index = theContent.find(" ");
            command = theContent.substr(0, index);
            content = theContent.substr(index + 1, content.length());
            if (command == "BEGIN" && robotChat == nullptr)
            {
                robotChat = AddNewMessage(PARTNERMSG, "robot:\n", nullptr);
            }
            else if (command == "ING" && robotChat != nullptr)
            {
                AddNewMessage(PARTNERMSG, content, robotChat);
            }
            else if (command == "FIN" && robotChat != nullptr)
            {
                robotChat = nullptr;
            }
            else if (command == "ERROR" && robotChat != nullptr)
            {
                QMessageBox::warning(this, "Error", "maybe server have some network error");
                robotChat = nullptr;
                AddNewMessage(PARTNERMSG, "Error: chatGPT did not finish answering, need to be asked again", nullptr);
            }
            else
            {
                robotChat = nullptr;
                QMessageBox::warning(this, "Error", "something wrong with ChatGPT ask");
                AddNewMessage(PARTNERMSG, "Error: Some network error that needs to be asked again", nullptr);
            }
        }
    }
    else if (command == "complete:")
    {
        if (content == "exit room succeed")
        {
            this->my_name = "";
            this->stackedWidget->setCurrentIndex(0);
        }
    }
}

bool isMatching(QString str)
{
    QRegularExpression regex("\\[(.*?)\\]:(.*)");
    QRegularExpressionMatch match = regex.match(str);
    return match.hasMatch();
}

ChatRoom::ChatRoom(QWidget *parent, QStackedWidget* stackedWidget,SOCKET socket)
	: NetWorkObj(parent)
{
	ui.setupUi(this);
	this->my_connect = socket;
    this->stackedWidget = stackedWidget;
    this->robotChat = nullptr;
    this->my_name = "";
    connect(this, &ChatRoom::RecvMessage, this, &ChatRoom::DealWithMessage);
    connect(ui.btnExit, &QPushButton::clicked, [=]() {
        SendMessage_STF(ReplaceString("exit_room"), this->my_connect);
        });
    connect(ui.btnAskSend, &QPushButton::clicked, [=]() {
        if (this->my_name == "")
        {
            QMessageBox::warning(this, "error", "you have not set your name");
        }
        else if (ui.AskInput->toPlainText() == "")
        {
            QMessageBox::warning(this, "error", "can not send a empty message!");
        }
        else if (this->robotChat != nullptr)
        {
            QMessageBox::warning(this, "error", "robot is still talking");
        }
        else
        {
            if(this->my_name=="god")SendMessage_STF(ReplaceString("ask_room !askChatGPT " + ui.AskInput->toPlainText().toStdString()), this->my_connect);
            else if (!isMatching(ui.AskInput->toPlainText())) QMessageBox::warning(this, "error", "error ask");
            else SendMessage_STF(ReplaceString("ask_room !askChatGPT " + this->my_name.toStdString() + ui.AskInput->toPlainText().toStdString()), this->my_connect);
            AddNewMessage(GPTMSG, this->my_name.toStdString() + "\n" + ui.AskInput->toPlainText().toStdString(), nullptr);
            ui.AskInput->setText("");
        }
        });
    connect(ui.btnChatSend, &QPushButton::clicked, [=]() {
        if (ui.ChatInput->toPlainText() == "")
        {
            QMessageBox::warning(this, "error", "you can not send a empty message");
        }
        else
        {
            if (this->my_name == "")
                SendMessage_STF(ReplaceString("ask_room !message undefine:\n" + ui.ChatInput->toPlainText().toStdString()), this->my_connect);
            else
                SendMessage_STF(ReplaceString("ask_room !message " + this->my_name.toStdString() + ":\n" + ui.ChatInput->toPlainText().toStdString()), this->my_connect);
            ui.ChatInput->setText("");
        }
        });
    connect(ui.btnJudgeSend, &QPushButton::clicked, [=]() {
        if (this->my_name == "")
        {
            QMessageBox::warning(this, "error", "you have not set your name");
            return;
        }
        else if (ui.JudgeInput->toPlainText() == "")
        {
            QMessageBox::warning(this, "error", "can not send a empty message!");
        }
        else if (this->robotChat != nullptr)
        {
            QMessageBox::warning(this, "error", "robot is still talking");
        }
        else
        {
            SendMessage_STF(ReplaceString("ask_room !askChatGPTForJudge " + ui.JudgeInput->toPlainText().toStdString()), this->my_connect);
            AddNewMessage(GPTMSG, this->my_name.toStdString() + ":" + ui.JudgeInput->toPlainText().toStdString(), nullptr);
            ui.JudgeInput->setText("");
        }
        });
    connect(ui.btnSetName, &QPushButton::clicked, [=]() {
        if (this->my_name != "")
        {
            QMessageBox::warning(this, "error", "you have set your name");
        }
        QString text = QInputDialog::getText(this, "input box", "please enter yout name:");
        text=text.trimmed();
        if (text == "")
        {
            QMessageBox::warning(this, "errror", "failure to set name");
        }
        else
        {
            SetName(text);
            ui.MyName->setText(text);
        }
        });
}

ChatRoom::~ChatRoom()
{}

QLabel* ChatRoom::AddNewMessage(msg_type type,std::string msg,QLabel* opeLabel)
{
    QScrollArea* scroll = nullptr;
    QVBoxLayout* layout = nullptr;
    switch (type)
    {
    case GPTMSG:layout = ui.GPTLayout; scroll = ui.AskScroll;
        break;
    case PARTNERMSG:layout = ui.PartnerLayout; scroll = ui.ChatScroll;
        break;
    case JUDGEMSG:layout = ui.JudgeLayout; scroll = ui.JudgeScroll;
        break;
    }
    QLabel* label = nullptr;
    if (opeLabel == nullptr)
    {
        label = new QLabel;
        label->setText(QString::fromStdString(msg));
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->insertWidget(layout->count() - 1, label);
    }
    else if (opeLabel != nullptr)
    {
        label = opeLabel;
        opeLabel->setText(opeLabel->text() + QString::fromStdString(msg));
    }
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    return label;
}

void ChatRoom::SetName(QString name)
{
    this->my_name = name;
}

QString ChatRoom::TransformToQStr(const char* str)
{
    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    wchar_t* transform = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, str, -1, transform, len);
    len = WideCharToMultiByte(CP_ACP, 0, transform, -1, NULL, 0, NULL, NULL);
    char* show = new char[len];
    WideCharToMultiByte(CP_ACP, 0, transform, -1, show, len, NULL, NULL);
    QString result(show);
    delete[] transform;
    delete[] show;
    return result;
}