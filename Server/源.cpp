#include<WinSock2.h>
#include<Windows.h>
#include<stdio.h>
#include<process.h>
#include<tchar.h>
#include<vector>
#include<string>
#include<semaphore>
#include<WS2tcpip.h>
#include <mutex>
#include"liboai.h"
#include"ThreadPool.h"
#pragma comment(lib,"Ws2_32.lib")
using namespace liboai;

#define CHUNK_SIZE 255
#define MAX_ROOM_NUM 500
#define BYTE_BITS 8

const char Request_Error[] = "Error: You have given a worng request";
const char Inner_Error[] = "Error: Something wrong in server";
const char LongAsk_Error[] = "Error: Something wrong with long ask";
const char RoomAsk_Error[] = "Error: Something wrong with room ask";

enum msg_type {
    GPTMSG, PARTNERMSG, JUDGEMSG
};

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

class BitMap
{
private:
    char* map;
    int length;
public:
    BitMap() { map = NULL; length = 0; }
    void SetBitMap(unsigned int index, bool flag)
    {
        if (index >= length * BYTE_BITS)
        {
            char* ano = new char[index / BYTE_BITS + 1];
            memset(ano, 0, sizeof(char) * (index / BYTE_BITS + 1));
            if (map != NULL)
            {
                memcpy(ano, map, length * sizeof(char));
                delete[] map;
            }
            length = (index / BYTE_BITS) + 1;
            map = ano;
        }
        if (flag) map[index / BYTE_BITS] = map[index / BYTE_BITS] | (1 << (index % BYTE_BITS));
        else map[index / BYTE_BITS] = map[index / BYTE_BITS] & (char)(~(1 << (index % BYTE_BITS)));
    }
    bool GetBitMap(unsigned int index)
    {
        if (index >= length * BYTE_BITS)
        {
            char* ano = new char[index / BYTE_BITS + 1];
            memset(ano, 0, sizeof(char) * (index / BYTE_BITS + 1));
            if (map != NULL)
            {
                memcpy(ano, map, length * sizeof(char));
                delete[] map;
            }
            length = (index / BYTE_BITS) + 1;
            map = ano;
        }
        return map[index / BYTE_BITS] & (1 << (index % BYTE_BITS));
    }
    ~BitMap() { if (map != NULL)delete[]map; }
};

struct SocketObject         // 24 字节
{
    char* Buffer;
    int BufferLen;
    int surplusLen;
    SOCKET sock;
    int roomID;
};

class Room
{
private:
    Conversation convo;
    std::mutex lock_convo;
public:
    std::vector<SOCKET> members;
    int id;
    bool isSingle;
    Room(int id,bool isSingle=false) :id(id),isSingle(isSingle) {}
    Conversation GetCopyConvo()
    {
        lock_convo.lock();
        Conversation result = this->convo;
        lock_convo.unlock();
        return result;;
    }
    void AddConversation(std::string mySpeech, std::string robotSpeech)
    {
        lock_convo.lock();
        this->convo.AddUserData(mySpeech);
        nlohmann::json robot;
        robot["role"] = "assistant";
        robot["content"] = robotSpeech;
        this->convo.Update(robot.dump());
        lock_convo.unlock();
    }
    ~Room()
    {}
};

class BinarySearchTree
{
private:
    struct TreeNode
    {
        Room room;
        int hack;
        TreeNode* left;
        TreeNode* right;
        TreeNode(int id,bool isSingle=false) :room(id,isSingle) {
            hack = 0;
            left = NULL;
            right = NULL;
        }
    };
    TreeNode* root;
    void ClearRoot()
    {
        if (root == NULL)return;
        std::vector<TreeNode*> child;
        std::vector<TreeNode*> parent;
        parent.push_back(root);
        while (!parent.empty())
        {
            for (TreeNode* i : parent)
            {
                if (i == NULL)continue;
                child.push_back(i->left);
                child.push_back(i->right);
                delete i;
            }
            parent.clear();
            parent = child;
            child.clear();
        }
    }
    bool AddNewNode(TreeNode* node)
    {
        TreeNode* current = root;
        std::vector<TreeNode*> stack;
        if (current == NULL)
        {
            root = node;
            return true;
        }
        while (current != NULL)
        {
            stack.push_back(current);
            if (current->room.id < node->room.id)current = current->right;
            else if (current->room.id < node->room.id)current = current->left;
            else
            {
                return false;
            }
        }
        if (stack.back()->room.id < node->room.id)stack.back()->right = node;
        else stack.back()->left = node;
        while (!stack.empty())
        {
            current = stack.back();
            stack.pop_back();
            int leftdiv = -1, rightdiv = -1;
            if (current->left != NULL)leftdiv = current->left->hack;
            if (current->right != NULL)rightdiv = current->right->hack;
            // 把上一个放下来填补空缺，current就是上一个，把current放下去
            // 就算小的那边加了一最大层数也不变，子节点不需二次遍历，这句存疑
            switch (leftdiv - rightdiv)
            {
            case -2:// 左小于右
            {
                // 右一定存在
                TreeNode* temp = current->right;
                current->right = temp->left;
                temp->left = current;
                if (stack.size() > 0)
                {
                    TreeNode* parent = stack.back();
                    if (parent->hack < current->hack)parent->right = temp;
                    else parent->left = temp;
                }
                else root = temp;
            }
            break;
            case 2:// 左大于右
            {
                // 左一定存在
                TreeNode* temp = current->left;
                current->left = temp->right;
                temp->right = current;
                if (stack.size() > 0)
                {
                    TreeNode* parent = stack.back();
                    if (parent->hack < current->hack)parent->right = temp;
                    else parent->left = temp;
                }
                else root = temp;
            }
            break;
            default:
                break;
            }
            current->hack = max((current->left == NULL) ? -1 : (current->left->hack), (current->right == NULL) ? -1 : (current->right->hack)) + 1;
            return true;
        }
    }
    int GetNewRoomID()
    {
        int result = 0;
        while (AuxiliaryMap.GetBitMap(result))result++;
        return result;
    }
    BitMap AuxiliaryMap;
public:
    BinarySearchTree()
    {
        root = NULL;
    }

    // 为某个客户端设置房间号
    bool AddNewNode(SocketObject& sock, bool isSingle = false)
    {
        int nums = GetNewRoomID();
        TreeNode* node = new TreeNode(nums,isSingle);
        if (AddNewNode(node))
        {
            node->room.members.push_back(sock.sock);
            sock.roomID = nums;                         // 这里客户的房间号被设置了
            AuxiliaryMap.SetBitMap(nums, true);
            return true;
        }
        return false;
    }

    void EraseNode(int roomID)
    {
        TreeNode* current = root;
        TreeNode* goalparent = NULL;    // 要删除节点的父节点

        while (current != NULL)
        {
            if (current->room.id == roomID)break;
            else if (current->room.id > roomID)
            {
                goalparent = current;
                current = current->left;
            }
            else
            {
                goalparent = current;
                current = current->right;
            }
        }
        if (current == NULL)return;     // 首先找到要删除的节点
        AuxiliaryMap.SetBitMap(roomID, false);

        TreeNode* goal = current;       // 要删除的节点
        TreeNode* parent = NULL;        // 要删除节点的父节点
        current = current->right;       // 要删除节点的右节点
        if (current == NULL)            // 要删除节点无右节点
        {
            current = goal->left;       // current变为要删除节点的左节点
            if (current == NULL)        // 要删除节点的左右节点均不存在
            {
                if (goalparent == NULL)root = current;      // 要删除节点无父节点则根节点置空
                else if (goalparent->room.id > goal->room.id)goalparent->left = current;    // 要删除节点有父节点则将要删除节点那一侧置空
                else goalparent->right = current;
                delete goal;
                return;
            }
            while (current->right != NULL)      // 寻找第一个左侧节点的最右节点
            {
                parent = current;
                current = current->right;
            }
            if (parent != NULL)parent->right = current->left;   // 找到要删除节点的上一个节点
            else goal->left = current->left;
        }
        else
        {
            while (current->left != NULL)
            {
                parent = current;
                current = current->left;
            }
            if (parent != NULL)parent->left = current->right;
            else goal->right = current->right;
        }
        if (goalparent == NULL)root = current;
        else if (goalparent->room.id > goal->room.id)goalparent->left = current;
        else goalparent->right = current;
        current->left = goal->left;
        current->right = goal->right;
        delete goal;
    }

    Room* SerachForNode(int roomID)
    {
        TreeNode* current = root;
        while (current != NULL)
        {
            if (current->room.id == roomID)break;
            else if (current->room.id > roomID)current = current->left;
            else current = current->right;
        }
        if (current == NULL)return NULL;
        return &(current->room);
    }

    ~BinarySearchTree()
    {
        ClearRoot();
        printf("安全清除\n");
    }
};

struct AcceptThreadParam
{
    bool* isExit;
    std::vector<SocketObject>* clientArray;
    HANDLE lock_client_arr;
};

unsigned int __stdcall AcceptThread(void* Param)
{
    AcceptThreadParam* param = (AcceptThreadParam*)Param;
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_addr.S_un.S_addr = htonl(ADDR_ANY);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(6000);

    bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));// 绑定

    listen(serverSocket, 1);// 监听
    while (!*(param->isExit))
    {
        sockaddr_in clientAddress = { 0 };
        int clientAddressLen = sizeof(clientAddress);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientAddressLen);
        char ip_address[64] = "";
        printf("接收到连接:%s\n", inet_ntop(AF_INET, (sockaddr*)&clientAddress.sin_addr, ip_address, 64));
        int iMode = 1;
        ioctlsocket(clientSocket, FIONBIO, (u_long*)&iMode);
        SocketObject sobj = { NULL,0,0,clientSocket,-1 };
        WaitForSingleObject(param->lock_client_arr, INFINITE);
        param->clientArray->push_back(sobj);
        ReleaseSemaphore(param->lock_client_arr, 1, NULL);
    }
    WaitForSingleObject(param->lock_client_arr, INFINITE);
    for (SocketObject sock : (*(param->clientArray)))
    {
        closesocket(sock.sock);
        if (sock.Buffer != NULL)delete[] sock.Buffer;
    }
    (*(param->clientArray)).clear();
    ReleaseSemaphore(param->lock_client_arr, 1, NULL);
    closesocket(serverSocket);
    return 0;
}

struct DealWithClientParam
{
    bool* isExit;
    BinarySearchTree* roomTable;
    std::vector<SocketObject>* roomAsk;
    std::vector<SocketObject>* clientArray;
    std::vector<SocketObject>* sendMessage;
    HANDLE lock_client_arr;
    HANDLE lock_send_msg;
    HANDLE lock_room_table;
    HANDLE lock_room_ask;
};

void AddSendMessage(SOCKET sock, std::vector<SocketObject>* sendMessage, HANDLE lock_send_msg, const char* message)
{
    std::string tempStr = ReplaceString(std::string(message));
    SocketObject sobj = { NULL,0,0,sock };
    sobj.Buffer = new char[tempStr.length() + 1];
    memset(sobj.Buffer, 0, sizeof(char) * (tempStr.length() + 1));
    memcpy(sobj.Buffer, tempStr.c_str(), sizeof(char) * tempStr.length());
    sobj.BufferLen = tempStr.length();
    sobj.surplusLen = sobj.BufferLen;
    WaitForSingleObject(lock_send_msg, INFINITE);
    sendMessage->push_back(sobj);
    ReleaseSemaphore(lock_send_msg, 1, NULL);
}

bool Exit_Room(SocketObject& sock, BinarySearchTree* roomTable, HANDLE lock_room_table, int roomID)
{
    WaitForSingleObject(lock_room_table, INFINITE);
    Room* room = roomTable->SerachForNode(roomID);
    if (room == NULL)return false;
    else
    {
        for (int i = 0; i < room->members.size(); i++)
        {
            if (room->members[i] == sock.sock)
            {
                room->members.erase(room->members.begin() + i);
                break;
            }
        }
        sock.roomID = -1;
        if (room->members.empty())roomTable->EraseNode(room->id);
    }
    ReleaseSemaphore(lock_room_table, 1, NULL);
    return true;
}

void DealWithMessage(std::string str, BinarySearchTree* roomTable, HANDLE lock_room_table,
    std::vector<SocketObject>* sendMessage, HANDLE lock_send_msg, std::vector<SocketObject>* roomAsk, HANDLE lock_room_ask, SocketObject& sock)
{
    std::string msg = DeReplaceString(str);
    int index = msg.find(" ");
    std::string command = msg.substr(0, index);
    std::string content = msg.substr(index + 1, msg.length());
    if (command == "create_room")
    {
        if (sock.roomID != -1)AddSendMessage(sock.sock, sendMessage, lock_send_msg, RoomAsk_Error);
        else
        {
            WaitForSingleObject(lock_room_table, INFINITE);
            if (!roomTable->AddNewNode(sock))AddSendMessage(sock.sock, sendMessage, lock_send_msg, RoomAsk_Error);
            else AddSendMessage(sock.sock, sendMessage, lock_send_msg, "complete: enter room succeed");
            ReleaseSemaphore(lock_room_table, 1, NULL);
        }
    }
    else if (command == "enter_room")
    {
        if (sock.roomID != -1)AddSendMessage(sock.sock, sendMessage, lock_send_msg, RoomAsk_Error);
        else
        {
            int roomID = atol(content.c_str());
            WaitForSingleObject(lock_room_table, INFINITE);
            Room* room = roomTable->SerachForNode(roomID);
            if (room == NULL||room->isSingle)AddSendMessage(sock.sock, sendMessage, lock_send_msg, RoomAsk_Error);
            else
            {
                room->members.push_back(sock.sock);
                sock.roomID = roomID;
                AddSendMessage(sock.sock, sendMessage, lock_send_msg, "complete: enter room succeed");
            }
            ReleaseSemaphore(lock_room_table, 1, NULL);
        }
    }
    else if (command == "exit_room")
    {
        int roomID = sock.roomID;
        if (roomID == -1)AddSendMessage(sock.sock, sendMessage, lock_send_msg, RoomAsk_Error);
        else
        {
            if (Exit_Room(sock, roomTable, lock_room_table, roomID))AddSendMessage(sock.sock, sendMessage, lock_send_msg, "complete: exit room succeed");
            else AddSendMessage(sock.sock, sendMessage, lock_send_msg, RoomAsk_Error);
        }
    }
    else if (command == "ask_room")
    {
        SocketObject sobj = { NULL,content.length(),content.length(),sock.sock,sock.roomID };
        sobj.Buffer = new char[sobj.BufferLen + 1];
        memset(sobj.Buffer, 0, sizeof(char) * sobj.BufferLen + 1);
        memcpy(sobj.Buffer, content.c_str(), sizeof(char) * sobj.BufferLen);
        WaitForSingleObject(lock_room_ask, INFINITE);
        roomAsk->push_back(sobj);
        ReleaseSemaphore(lock_room_ask, 1, NULL);
    }
    else if (command == "single_Chat")
    {
        if (sock.roomID != -1)AddSendMessage(sock.sock, sendMessage, lock_send_msg, RoomAsk_Error);
        else
        {
            WaitForSingleObject(lock_room_table, INFINITE);
            if (!roomTable->AddNewNode(sock, true))AddSendMessage(sock.sock, sendMessage, lock_send_msg, RoomAsk_Error);
            else AddSendMessage(sock.sock, sendMessage, lock_send_msg, "complete: single chat succeed");
            ReleaseSemaphore(lock_room_table, 1, NULL);
        }
    }
    else AddSendMessage(sock.sock, sendMessage, lock_send_msg, Request_Error);
}

unsigned int __stdcall DealWithClient(void* Param)
{
    DealWithClientParam* param = (DealWithClientParam*)Param;
    int index = 0;
    char long_ask[] = "LONG_ASK";
    char long_asking[] = "chunk:";
    char long_ask_final[] = "LONG_ASK_FINAL";
    char all_ask[] = "ALL";
    while (!*(param->isExit))
    {
        WaitForSingleObject(param->lock_client_arr, INFINITE);
        if (!param->clientArray->empty())
        {
            index = index % param->clientArray->size();
            SocketObject& sock = (*(param->clientArray))[index];
            char buf[CHUNK_SIZE + 1] = { 0 };
            int retVal = recv(sock.sock, buf, CHUNK_SIZE, 0);
            if (retVal == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINVAL)index++;
                else if (err == WSAEISCONN)index++;
                else
                {
                    printf("接收消息失败，连接断开\n");
                    closesocket(sock.sock);
                    if (sock.Buffer != NULL)delete[]sock.Buffer;
                    if (sock.roomID != -1)
                    {
                        Exit_Room(sock, param->roomTable, param->lock_room_table, sock.roomID);
                        printf("成功退出房间\n");
                    }
                    (*(param->clientArray)).erase((*(param->clientArray)).begin() + index);
                }
            }
            else if (retVal == 0)
            {
                printf("连接断开\n");
                closesocket(sock.sock);
                if (sock.Buffer != NULL)delete[]sock.Buffer;
                if (sock.roomID != -1)
                {
                    Exit_Room(sock, param->roomTable, param->lock_room_table, sock.roomID);
                    printf("成功退出房间\n");
                }
                (*(param->clientArray)).erase((*(param->clientArray)).begin() + index);
            }
            else
            {
                char* temp = strchr(buf, ' ');
                if (temp == NULL)
                {
                    AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, Request_Error);
                }
                else
                {
                    *temp = '\0';
                    temp++;
                    if (strcmp(buf, long_ask) == 0)      // 长连接开始
                    {
                        *strchr(temp, ' ') = '\0';
                        sock.BufferLen = atol(temp);
                        if (sock.Buffer != NULL)delete[] sock.Buffer;
                        sock.Buffer = new char[sock.BufferLen + 1];
                        memset(sock.Buffer, 0, sizeof(char) * (sock.BufferLen + 1));
                        sock.surplusLen = sock.BufferLen;
                        if (sock.Buffer == NULL)
                        {
                            AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, Inner_Error);
                        }
                    }
                    else if (strcmp(buf, long_asking) == 0)     // 长连接进行中
                    {
                        if (sock.Buffer == NULL)
                        {
                            AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, LongAsk_Error);
                        }
                        else
                        {
                            *strchr(temp, ' ') = '\0';
                            if (sock.surplusLen == 0)
                            {
                                AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, LongAsk_Error);
                            }
                            else if (sock.surplusLen < CHUNK_SIZE - sizeof(long_asking) - 2)
                            {
                                memcpy(sock.Buffer + sock.BufferLen - sock.surplusLen, temp, sizeof(char) * sock.surplusLen);
                                sock.surplusLen = 0;
                            }
                            else
                            {
                                memcpy(sock.Buffer + sock.BufferLen - sock.surplusLen, temp, sizeof(char) * (CHUNK_SIZE - sizeof(long_asking) - 2));
                                sock.surplusLen -= CHUNK_SIZE - sizeof(long_asking) - 2;
                            }
                        }
                    }
                    else if (strcmp(buf, long_ask_final) == 0)      // 长连接结束
                    {
                        if (sock.Buffer == NULL)
                        {
                            AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, Inner_Error);
                        }
                        else
                        {
                            DealWithMessage(std::string(sock.Buffer), param->roomTable, param->lock_room_table, param->sendMessage,
                                param->lock_send_msg, param->roomAsk, param->lock_room_ask, sock);
                            delete[] sock.Buffer;
                            sock.Buffer = NULL;
                            sock.BufferLen = sock.surplusLen = 0;
                        }
                    }
                    else if (strcmp(buf, all_ask) == 0)         // 这次就是全部的信息
                    {
                        *strchr(temp, ' ') = '\0';
                        DealWithMessage(std::string(temp), param->roomTable, param->lock_room_table, param->sendMessage,
                            param->lock_send_msg, param->roomAsk, param->lock_room_ask, sock);
                    }
                    else
                    {
                        AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, Request_Error);
                    }
                }
            }
        }
        ReleaseSemaphore(param->lock_client_arr, 1, NULL);
        Sleep(1);           // 让出client_arr队列
    }
    return 0;
}

struct MaintainRoomTableParam
{
    bool* isExit;
    BinarySearchTree* roomTable;
    std::vector<SocketObject>* roomAsk;
    std::vector<SocketObject>* sendMessage;
    HANDLE lock_send_msg;
    HANDLE lock_room_table;
    HANDLE lock_room_ask;
};

char** MatchString(const char* str, int* result)
{
    int resultNum = 0;
    char** tempBuf = NULL;
    char* temp = (char*)malloc(sizeof(char) * strlen(str) + 1);
    if (temp == NULL)
    {
        *result = 0;
        return NULL;
    }
    memset(temp, 0, sizeof(char) * strlen(str) + 1);
    char match[] = "data:";
    int index = 0;
    char flag = 0;
    int bracket = 0;
    char flag_bracket = 0;
    char flag_quotation = 0;
    char readyFor = 0;
    while (*str != '\0')
    {
        if (flag == 0)
        {
            if (*str == match[index])index++;
            else
            {
                index = 0;
                if (*str == match[index])index++;
            }
            if (index == strlen(match))
            {
                index = 0;
                flag = 1;
            }
        }
        else
        {
            temp[index++] = *str;
            if (!flag_quotation && *str == '{')
            {
                bracket++;
                flag_bracket = 1;
            }
            else if (!flag_quotation && *str == '}')bracket--;
            else if (!flag_quotation && !readyFor && *str == '\"')flag_quotation = 1;
            else if (flag_quotation && !readyFor && *str == '\"')flag_quotation = 0;
            if (!readyFor && *str == '\\')readyFor = 1;
            else readyFor = 0;
            if (flag_bracket && bracket == 0)
            {
                temp[index] = '\0';
                index = 0;
                flag_bracket = 0;
                flag = 0;
                char* item = (char*)malloc(sizeof(char) * (strlen(temp) + 1));
                strcpy_s(item, sizeof(char) * (strlen(temp) + 1), temp);
                char** anoArr = (char**)malloc(sizeof(char*) * (resultNum + 1));
                if (tempBuf != NULL)
                {
                    memcpy(anoArr, tempBuf, sizeof(char*) * resultNum);
                    free(tempBuf);
                }
                anoArr[resultNum] = item;
                resultNum++;
                tempBuf = anoArr;
            }
        }
        str++;
    }
    free(temp);
    *result = resultNum;
    return tempBuf;
}

void ClearJBuf(char** buf, int result)
{
    if (buf == NULL)return;
    for (int i = 0; i < result; i++)
    {
        if (buf[i] != NULL)
            free(buf[i]);
    }
    free(buf);
}

struct SendConvoParam
{
    SOCKET sock;
    Room* room;
    std::vector<SocketObject>* sendMessage;
    HANDLE lock_send_msg;
    Conversation convo;
    std::string recvSpeech;
    std::string mySpeech;
    msg_type type;
};

bool stream_function(std::string str, intptr_t userdata)
{
    SendConvoParam* param = (SendConvoParam*)userdata;
    int result = 0;
    char** json_s = MatchString(str.c_str(), &result);
    for (int i = 0; i < result; i++)
    {
        nlohmann::json response = nlohmann::json::parse(json_s[i]);
        nlohmann::json delta = response["choices"][0]["delta"];
        if (delta.contains("content"))
        {
            std::string content = delta["content"];      // here is the response that GPT send.
            param->recvSpeech += content;
            switch (param->type)
            {
            case PARTNERMSG:AddSendMessage(param->sock, param->sendMessage, param->lock_send_msg, ("room_ask !tellChatGPT ING " + content).c_str());
                break;
            case GPTMSG:AddSendMessage(param->sock, param->sendMessage, param->lock_send_msg, ("room_ask !askChatGPT ING " + content).c_str());
                break;
            case JUDGEMSG:AddSendMessage(param->sock, param->sendMessage, param->lock_send_msg, ("room_ask !askChatGPTForJudge ING " + content).c_str());
                break;
            }
        }
    }
    ClearJBuf(json_s, result);
    return true;
}

bool SendConversation(SendConvoParam* param)
{
    OpenAI oai;
    if (oai.auth.SetKeyEnv("$CHATGPTKEY$")) {
        try {
            param->convo.AddUserData(param->mySpeech);
            std::string stream_s;
            Response response = oai.ChatCompletion->create(
                "gpt-3.5-turbo", param->convo, std::nullopt, std::nullopt, std::nullopt, stream_function, std::nullopt, std::nullopt, std::nullopt,
                std::nullopt, std::nullopt, std::nullopt, (intptr_t)param
            );
            return true;
        }
        catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }
    return false;
}

unsigned int __stdcall MaintainRoomTable(void* Param)
{
    ThreadPool pool(5);
    MaintainRoomTableParam* param = (MaintainRoomTableParam*)Param;
    int index = 0;
    while (!*(param->isExit))
    {
        WaitForSingleObject(param->lock_room_ask, INFINITE);
        if (!param->roomAsk->empty())
        {
            index = index % param->roomAsk->size();
            SocketObject& sock = (*(param->roomAsk))[index];
            char* temp = strchr(sock.Buffer, ' ');
            if (temp == NULL)
            {
                AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, RoomAsk_Error);
            }
            else
            {
                *temp = '\0';
                temp++;
                // ? 表示查询
                // ! 表示发送
                // + 表示创建
                // - 表示退出
                if (strcmp(sock.Buffer, "?room_id") == 0)
                {
                    char numBuf[64] = "";
                    _ltoa_s(sock.roomID, numBuf, 10);
                    AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, ("room_ask ?room_id " + std::string(numBuf)).c_str());
                }
                else if (strcmp(sock.Buffer, "!message") == 0)
                {
                    WaitForSingleObject(param->lock_room_table, INFINITE);
                    Room* room = param->roomTable->SerachForNode(sock.roomID);
                    if (room == NULL)
                    {
                        AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, RoomAsk_Error);
                    }
                    else
                    {
                        for (int i = 0; i < room->members.size(); i++)
                        {
                            AddSendMessage(room->members[i], param->sendMessage, param->lock_send_msg, (std::string("room_ask !message ") + temp).c_str());
                        }
                    }
                    ReleaseSemaphore(param->lock_room_table, 1, NULL);
                }
                else if (strcmp(sock.Buffer, "!askChatGPT") == 0)
                {
                    WaitForSingleObject(param->lock_room_table, INFINITE);
                    Room* room = param->roomTable->SerachForNode(sock.roomID);
                    if (room == NULL)
                    {
                        AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, RoomAsk_Error);
                    }
                    else
                    {
                        std::shared_ptr<SendConvoParam> ptr = std::make_shared<SendConvoParam>();
                        ptr->convo = room->GetCopyConvo();
                        ptr->lock_send_msg = param->lock_send_msg;
                        ptr->mySpeech = temp;
                        ptr->recvSpeech = "";
                        ptr->room = room;
                        ptr->sendMessage = param->sendMessage;
                        ptr->sock = sock.sock;
                        ptr->type = GPTMSG;
                        pool.enqueue([](std::shared_ptr<SendConvoParam> p) {
                            AddSendMessage(p->sock, p->sendMessage, p->lock_send_msg, std::string("room_ask !askChatGPT BEGIN").c_str());
                            if (SendConversation(p.get()))
                            {
                                p->room->AddConversation(p->mySpeech, p->recvSpeech);
                                AddSendMessage(p->sock, p->sendMessage, p->lock_send_msg, std::string("room_ask !askChatGPT FIN").c_str());
                            }
                            else
                            {
                                AddSendMessage(p->sock, p->sendMessage, p->lock_send_msg, std::string("room_ask !askChatGPT ERROR").c_str());
                            }
                            },std::move(ptr));
                    }
                    ReleaseSemaphore(param->lock_room_table, 1, NULL);
                }
                else if (strcmp(sock.Buffer, "!askChatGPTForJudge") == 0)
                {
                    WaitForSingleObject(param->lock_room_table, INFINITE);
                    Room* room = param->roomTable->SerachForNode(sock.roomID);
                    if (room == NULL)
                    {
                        AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, RoomAsk_Error);
                    }
                    else
                    {
                        std::shared_ptr<SendConvoParam> ptr = std::make_shared<SendConvoParam>();
                        ptr->convo = room->GetCopyConvo();
                        ptr->lock_send_msg = param->lock_send_msg;
                        ptr->mySpeech = temp;
                        ptr->recvSpeech = "";
                        ptr->room = room;
                        ptr->sendMessage = param->sendMessage;
                        ptr->sock = sock.sock;
                        ptr->type = JUDGEMSG;
                        pool.enqueue([](std::shared_ptr<SendConvoParam> p) {
                            AddSendMessage(p->sock, p->sendMessage, p->lock_send_msg, std::string("room_ask !askChatGPTForJudge BEGIN").c_str());
                            if (SendConversation(p.get()))
                            {
                                AddSendMessage(p->sock, p->sendMessage, p->lock_send_msg, std::string("room_ask !askChatGPTForJudge FIN").c_str());
                            }
                            else
                            {
                                AddSendMessage(p->sock, p->sendMessage, p->lock_send_msg, std::string("room_ask !askChatGPTForJudge ERROR").c_str());
                            }
                            }, std::move(ptr));
                    }
                    ReleaseSemaphore(param->lock_room_table, 1, NULL);
                }
                else if (strcmp(sock.Buffer, "!tellChatGPT") == 0)
                {
                    WaitForSingleObject(param->lock_room_table, INFINITE);
                    Room* room = param->roomTable->SerachForNode(sock.roomID);
                    if (room == NULL)
                    {
                        AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, RoomAsk_Error);
                    }
                    else
                    {
                        std::shared_ptr<SendConvoParam> ptr = std::make_shared<SendConvoParam>();
                        ptr->convo = room->GetCopyConvo();
                        ptr->lock_send_msg = param->lock_send_msg;
                        ptr->mySpeech = temp;
                        ptr->recvSpeech = "";
                        ptr->room = room;
                        ptr->sendMessage = param->sendMessage;
                        ptr->sock = sock.sock;
                        ptr->type = PARTNERMSG;
                        pool.enqueue([](std::shared_ptr<SendConvoParam> p) {
                            AddSendMessage(p->sock, p->sendMessage, p->lock_send_msg, std::string("room_ask !tellChatGPT BEGIN").c_str());
                            if (SendConversation(p.get()))
                            {
                                p->room->AddConversation(p->mySpeech, p->recvSpeech);
                                AddSendMessage(p->sock, p->sendMessage, p->lock_send_msg, std::string("room_ask !tellChatGPT FIN").c_str());
                            }
                            else
                            {
                                AddSendMessage(p->sock, p->sendMessage, p->lock_send_msg, std::string("room_ask !tellChatGPT ERROR").c_str());
                            }
                            }, std::move(ptr));
                    }
                    ReleaseSemaphore(param->lock_room_table, 1, NULL);
                }
                else
                {
                    AddSendMessage(sock.sock, param->sendMessage, param->lock_send_msg, RoomAsk_Error);
                }
            }
            if (sock.Buffer != NULL)delete[] sock.Buffer;
            (*(param->roomAsk)).erase((*(param->roomAsk)).begin() + index);
        }
        ReleaseSemaphore(param->lock_room_ask, 1, NULL);
        Sleep(1);           // 让出room_ask队列
    }
    WaitForSingleObject(param->lock_room_ask, INFINITE);
    for (SocketObject& sock : (*(param->roomAsk)))
    {
        if (sock.Buffer != NULL)delete[] sock.Buffer;
    }
    (*(param->roomAsk)).clear();
    ReleaseSemaphore(param->lock_room_ask, 1, NULL);
    return 0;
}

struct SendMessageParam
{
    bool* isExit;
    std::vector<SocketObject>* sendMessage;
    HANDLE lock_send_msg;
};

unsigned int __stdcall SendMessageToClient(void* Param)
{
    SendMessageParam* param = (SendMessageParam*)Param;
    int index = 0;
    char all_message[] = "ALL";
    char sectional_message[] = "SECTION";
    char transform_final[] = "TRANS_FIN";
    bool isAllMsg = false;
    while (!*(param->isExit))
    {
        WaitForSingleObject(param->lock_send_msg, INFINITE);
        if (!param->sendMessage->empty())
        {
            index = index % param->sendMessage->size();
            SocketObject& sock = (*(param->sendMessage))[index];
            char buf[CHUNK_SIZE + 1] = "";
            int tempLen = 0;
            if (sock.BufferLen < CHUNK_SIZE - sizeof(all_message) - 1)          // 需传输的信息比较少，可以一波传完
            {
                isAllMsg = true;
                memset(buf, ' ', sizeof(buf));
                memcpy(buf, all_message, strlen(all_message));
                memcpy(buf + strlen(all_message) + 1, sock.Buffer, sock.BufferLen);
                tempLen = sock.BufferLen;
            }
            else // 需传输的信息比较大，不能一波传完
            {
                isAllMsg = false;
                if (sock.surplusLen > 0)
                {
                    memset(buf, ' ', sizeof(buf));
                    memcpy(buf, sectional_message, strlen(sectional_message));
                    tempLen = min(CHUNK_SIZE - strlen(sectional_message) - 2, sock.surplusLen);
                    memcpy(buf + strlen(sectional_message) + 1, &sock.Buffer[sock.BufferLen - sock.surplusLen], sizeof(char) * tempLen);
                }
                else
                {
                    memset(buf, ' ', sizeof(buf));
                    memcpy(buf, transform_final, strlen(transform_final));
                }
            }
            buf[CHUNK_SIZE] = '\0';
            int retVal = send(sock.sock, buf, CHUNK_SIZE, 0);
            if (retVal == SOCKET_ERROR)
            {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK || err == WSAEINVAL)index++;
                else if (err == WSAEISCONN)index++;
                else
                {
                    printf("发送消息失败，连接断开\n");
                    if (sock.Buffer != NULL)delete[]sock.Buffer;
                    (*(param->sendMessage)).erase((*(param->sendMessage)).begin() + index);
                }
            }
            else if (retVal == 0)
            {
                printf("连接断开\n");
                if (sock.Buffer != NULL)delete[]sock.Buffer;
                (*(param->sendMessage)).erase((*(param->sendMessage)).begin() + index);
            }
            else
            {
                if (!isAllMsg && sock.surplusLen == 0)
                {
                    if (sock.Buffer != NULL)delete[]sock.Buffer;
                    (*(param->sendMessage)).erase((*(param->sendMessage)).begin() + index);
                }
                else
                {
                    sock.surplusLen -= tempLen;
                    if (isAllMsg && sock.surplusLen == 0)
                    {
                        if (sock.Buffer != NULL)delete[]sock.Buffer;
                        (*(param->sendMessage)).erase((*(param->sendMessage)).begin() + index);
                    }
                }
            }
        }
        ReleaseSemaphore(param->lock_send_msg, 1, NULL);
        Sleep(1);       // 让出sendmessage队列
    }
    WaitForSingleObject(param->lock_send_msg, INFINITE);
    for (SocketObject& sock : (*(param->sendMessage)))
    {
        if (sock.Buffer != NULL)delete[] sock.Buffer;
    }
    (*(param->sendMessage)).clear();
    ReleaseSemaphore(param->lock_send_msg, 1, NULL);
    return 0;
}

int main()
{
    WSAData wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    std::vector<SocketObject> clientArray;
    std::vector<SocketObject> sendMessage;
    std::vector<SocketObject> roomAsk;
    BinarySearchTree roomTable;
    bool isExit = false;
    HANDLE lock_client_array = CreateSemaphore(NULL, 1, 1, _T("clientArray"));
    HANDLE lock_send_msg = CreateSemaphore(NULL, 1, 1, _T("sendMessage"));
    HANDLE lock_room_ask = CreateSemaphore(NULL, 1, 1, _T("roomAsk"));
    HANDLE lock_room_table = CreateSemaphore(NULL, 1, 1, _T("roomTable"));

    AcceptThreadParam AcceptParam;
    AcceptParam.clientArray = &clientArray;
    AcceptParam.isExit = &isExit;
    AcceptParam.lock_client_arr = lock_client_array;
    HANDLE accept = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, &AcceptParam, 0, NULL);

    DealWithClientParam DealWithParam;
    DealWithParam.clientArray = &clientArray;
    DealWithParam.sendMessage = &sendMessage;
    DealWithParam.isExit = &isExit;
    DealWithParam.lock_client_arr = lock_client_array;
    DealWithParam.lock_send_msg = lock_send_msg;
    DealWithParam.lock_room_ask = lock_room_ask;
    DealWithParam.lock_room_table = lock_room_table;
    DealWithParam.roomAsk = &roomAsk;
    DealWithParam.roomTable = &roomTable;
    HANDLE deal_with = (HANDLE)_beginthreadex(NULL, 0, DealWithClient, &DealWithParam, 0, NULL);

    MaintainRoomTableParam MaintainParam;
    MaintainParam.isExit = &isExit;
    MaintainParam.lock_room_ask = lock_room_ask;
    MaintainParam.lock_room_table = lock_room_table;
    MaintainParam.roomAsk = &roomAsk;
    MaintainParam.roomTable = &roomTable;
    MaintainParam.sendMessage = &sendMessage;
    HANDLE maintain = (HANDLE)_beginthreadex(NULL, 0, MaintainRoomTable, &MaintainParam, 0, NULL);

    SendMessageParam SendParam;
    SendParam.isExit = &isExit;
    SendParam.lock_send_msg = lock_send_msg;
    SendParam.sendMessage = &sendMessage;
    HANDLE send_msg = (HANDLE)_beginthreadex(NULL, 0, SendMessageToClient, &SendParam, 0, NULL);

    getchar();
    isExit = true;
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serveAddress;
    inet_pton(AF_INET, "127.0.0.1", &serveAddress.sin_addr);
    serveAddress.sin_port = htons(6000);
    serveAddress.sin_family = AF_INET;
    int retVal = connect(clientSocket, (sockaddr*)&serveAddress, sizeof(serveAddress));

    WaitForSingleObject(accept, INFINITE);
    WaitForSingleObject(deal_with, INFINITE);
    WaitForSingleObject(maintain, INFINITE);
    WaitForSingleObject(send_msg, INFINITE);
    CloseHandle(accept);
    CloseHandle(deal_with);
    CloseHandle(maintain);
    CloseHandle(send_msg);
    CloseHandle(lock_client_array);
    CloseHandle(lock_send_msg);
    CloseHandle(lock_room_ask);
    CloseHandle(lock_room_table);
    WSACleanup();
    return 0;
}