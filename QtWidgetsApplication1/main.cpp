#include <QtWidgets/QApplication>
#include"ChatRoom.h"
#include"Hall.h"
#include"SingleChatRoom.h"
#include<qstackedwidget.h>
#include<WS2tcpip.h>
#include<process.h>
#include<qinputdialog.h>
#include<qmessagebox.h>
#pragma comment(lib,"Ws2_32.lib")

#ifndef CHUNK_SIZE

#define CHUNK_SIZE 255

bool RecvOnce(SOCKET sock, char* buffer, int bufferLen, bool* isExit)
{
    bool flag = true;
    while ((isExit && (!(*isExit))) || (isExit==nullptr))
    {
        int retVal = recv(sock, buffer, CHUNK_SIZE, 0);
        if (retVal == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINVAL);
            else if (err == WSAEISCONN);
            else
            {
                flag = false;
                break;
            }
        }
        else break;
    }
    return flag;
}

bool SendOnce(SOCKET sock, char* buffer, int bufferLen, bool* isExit)
{
    bool flag = true;
    while ((isExit && (!(*isExit))) || (isExit == nullptr))
    {
        int retVal = send(sock, buffer, CHUNK_SIZE, 0);
        if (retVal == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINVAL);
            else if (err == WSAEISCONN);
            else
            {
                flag = false;
                break;
            }
        }
        else break;
    }
    return flag;
}

// \r\n作为休止符
std::string ReplaceString(std::string str)
{
    std::string result;
    for (char ch : str)
    {
        if (ch == ' ')result += "$BLANK$";
        else result += ch;
    }
    return result;
}

std::string DeReplaceString(std::string str)
{
    std::string result;
    char matching[] = "$BLANK$";
    int i = 0, j = 0;
    for (i = 0; i < str.length(); i++)
    {
        if (str[i] == matching[j])j++;
        else
        {
            bool flag = false;
            for (j; j > 0; j--, flag = true)result += str[i - j];
            if (!flag)result += str[i];
            else i--;
        }
        if (j == strlen(matching))
        {
            result += ' ';
            j = 0;
        }
    }
    return result;
}

void SendMessage_STF(std::string str, SOCKET sock, bool* isExit)
{
    // 空格作为填充字符，如果信息中有空格就用$BLANK$替换
    // 末尾以空格做结束符
    char all_ask[] = "ALL";
    char long_ask[] = "LONG_ASK";
    char long_asking[] = "chunk:";
    char long_ask_final[] = "LONG_ASK_FINAL";
    char buf[CHUNK_SIZE + 1] = "";
    if (str.length() < CHUNK_SIZE - sizeof(all_ask) - 2)
    {
        memset(buf, ' ', sizeof(buf));
        memcpy(buf, all_ask, sizeof(char) * strlen(all_ask));
        memcpy(buf + strlen(all_ask) + 1, str.c_str(), sizeof(char) * str.length());
        buf[CHUNK_SIZE] = '\0';
        SendOnce(sock, buf, CHUNK_SIZE, isExit);
    }
    else
    {
        int surplus = str.length();
        memset(buf, ' ', sizeof(buf));
        memcpy(buf, long_ask, sizeof(char) * strlen(long_ask));
        _ltoa_s(surplus, buf + strlen(long_ask) + 1, CHUNK_SIZE - strlen(long_ask) - 1, 10);
        buf[CHUNK_SIZE] = '\0';
        while (strlen(buf) != CHUNK_SIZE)buf[strlen(buf)] = ' ';
        SendOnce(sock, buf, CHUNK_SIZE, isExit);
        do
        {
            memset(buf, ' ', sizeof(buf));
            int tempLen = min(CHUNK_SIZE - sizeof(long_asking) - 2, surplus);       // 2 代表发送消息的首尾两个空格
            memcpy(buf, long_asking, sizeof(char) * strlen(long_asking));
            memcpy(buf + strlen(long_asking) + 1, &str.c_str()[str.length() - surplus], sizeof(char) * tempLen);
            buf[CHUNK_SIZE] = '\0';
            SendOnce(sock, buf, CHUNK_SIZE, isExit);
            surplus -= tempLen;
        } while (surplus != 0);
        memset(buf, ' ', sizeof(buf));
        memcpy(buf, long_ask_final, sizeof(char) * strlen(long_ask_final));
        buf[CHUNK_SIZE] = '\0';
        SendOnce(sock, buf, CHUNK_SIZE, isExit);
    }
}

std::string RecvMessage_STF(SOCKET sock, bool* isExit)
{
    std::string result = "";
    char buf[CHUNK_SIZE + 1] = "";
    if(!RecvOnce(sock, buf, CHUNK_SIZE, isExit))return "";
    char* temp = strchr(buf, ' ');
    if (temp == NULL)return result;
    *temp = '\0';
    temp++;
    if (strcmp(buf, "ALL") == 0)
    {
        *strchr(temp, ' ') = '\0';
        result += temp;
    }
    else if (strcmp(buf, "SECTION") == 0)
    {
        do
        {
            *strchr(temp, ' ') = '\0';
            result += temp;
            if (!RecvOnce(sock, buf, CHUNK_SIZE, isExit))return "";
            temp = strchr(buf, ' ');
            if (temp == NULL)return "";
            *temp = '\0';
            temp++;
        } while (strcmp(buf, "TRANS_FIN"));
    }
    return result;
}

#endif // !CHUNK_SIZE

struct RecvParam
{
    QStackedWidget* widget;
    bool* isExit;
    SOCKET socket;
};

unsigned int __stdcall RecvThreadFunc(void* Param)
{
    RecvParam* param = (RecvParam*)Param;
    while (!(*(param->isExit)))
    {
        std::string str = DeReplaceString(RecvMessage_STF(param->socket, param->isExit));
        if(str!="")
        emit ((NetWorkObj*)(param->widget->currentWidget()))->RecvMessage(QString::fromStdString(str));
    }
    return 0;
}

int main(int argc, char *argv[])
{
    WSAData wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    QApplication a(argc, argv);
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    QString text_add = QInputDialog::getText(nullptr, "input box", "please enter the ip address:");
    QString text_port = QInputDialog::getText(nullptr, "input box", "please enter the port:");
    sockaddr_in serveAddress;
    inet_pton(AF_INET, text_add.toStdString().c_str(), &serveAddress.sin_addr);
    serveAddress.sin_port = htons(text_port.toInt());
    serveAddress.sin_family = AF_INET;
    int retVal = connect(clientSocket, (sockaddr*)&serveAddress, sizeof(serveAddress));
    if (retVal == SOCKET_ERROR)
    {
        QMessageBox::warning(nullptr, "error", "connect failure");
        return -1;
    }
    int iMode = 1;
    ioctlsocket(clientSocket, FIONBIO, (u_long*)&iMode);

    bool isExit = false;
    QStackedWidget stackedWidget;

    Hall* hall=new Hall(nullptr, &stackedWidget,clientSocket);
    ChatRoom* chatRoom = new ChatRoom(nullptr, &stackedWidget, clientSocket);
    SingleChatRoom* singleChatRoom = new SingleChatRoom(nullptr, &stackedWidget, clientSocket);

    stackedWidget.addWidget(hall);
    stackedWidget.addWidget(chatRoom);
    stackedWidget.addWidget(singleChatRoom);
    // 设置初始页面
    stackedWidget.setCurrentIndex(0); // 初始显示第一个页面

    RecvParam recvParam;
    recvParam.widget = &stackedWidget;
    recvParam.isExit = &isExit;;
    recvParam.socket = clientSocket;
    HANDLE RecvThread = (HANDLE)_beginthreadex(NULL, 0, RecvThreadFunc, &recvParam, 0, NULL);
    if (RecvThread == NULL)
    {
        QMessageBox::warning(nullptr, "error", "thread error");
        return -1;
    }

    stackedWidget.show();
    a.exec();
    stackedWidget.removeWidget(hall);
    stackedWidget.removeWidget(chatRoom);
    stackedWidget.removeWidget(singleChatRoom);
    delete hall;
    delete chatRoom;
    delete singleChatRoom;

    isExit = true;
    WaitForSingleObject(RecvThread,INFINITE);
    closesocket(clientSocket);
    WSACleanup();
    CloseHandle(RecvThread);
    return 0;
}
