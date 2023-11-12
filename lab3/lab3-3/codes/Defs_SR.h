#ifndef DEFS_H
#define DEFS_H

#include <iostream>
#include <WinSock2.h>
#include <stdlib.h>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <time.h>
#include <cmath>
#include <sys/time.h>
#include <windows.h>
#include <atomic>
#include <mutex>
#include <thread>

using namespace std;

#pragma comment(lib, "Ws2_32.lib")

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

SOCKADDR_IN RouterAddr;
string RouterIP = "127.0.0.1";
int RouterAddrLen = sizeof(RouterAddr);

SOCKET ServerSocket;
SOCKADDR_IN ServerAddr;
string ServerIP = "127.0.0.1";
int ServerAddrLen = sizeof(ServerAddr);

SOCKET ClientSocket;
SOCKADDR_IN ClientAddr;
string ClientIP = "127.0.0.1";
int ClientAddrLen = sizeof(ClientAddr);

uint32_t file_length;

#define MSS 8168
#define FIN 0b1
#define CFH 0b10
#define ACK 0b100
#define SYN 0b1000

// #define Router_Port 12345
// #define Server_Port 65432
// #define Client_Port 54321
#define Wait_Time 1000
#define Windows_Size 20

#define _CRT_SECURE_NO_WARNINGS         // 禁止使用不安全的函数报错
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 禁止使用旧版本的函数报错

/* * 消息头部设计
 * |0            15|16            31|
 * ----------------------------------
 * |            SrcPort             |
 * ----------------------------------
 * |            DstPort             |
 * ----------------------------------
 * |              Seq               |
 * ----------------------------------
 * |              Ack               |
 * ----------------------------------
 * |            Length              |
 * ----------------------------------
 * |      Flag     |      Check     |
 * ----------------------------------
 * |              Data              |
 * ----------------------------------
 *
 * Flag
 * |0  |1  |2  |3  |4              15|
 * -----------------------------------
 * |FIN|CFH|ACK|SYN|                 |
 * -----------------------------------
 */
#pragma pack(1)

struct Message
{
    uint32_t SrcPort;
    uint32_t DstPort;
    uint32_t Seq;
    uint32_t Ack;
    uint32_t Length;
    uint16_t Flag;
    uint16_t Check;
    char Data[MSS];

    Message() : SrcPort(0), DstPort(0), Seq(0), Ack(0), Length(0), Flag(0), Check(0) { memset(this->Data, 0, MSS); }

    void Set_CFH() { this->Flag |= CFH; }
    bool Is_CFH() { return this->Flag & CFH; }
    void Set_ACK() { this->Flag |= ACK; }
    bool Is_ACK() { return this->Flag & ACK; }
    void Set_SYN() { this->Flag |= SYN; }
    bool Is_SYN() { return this->Flag & SYN; }
    void Set_FIN() { this->Flag |= FIN; }
    bool Is_FIN() { return this->Flag & FIN; }

    void Set_Check();
    bool CheckValid();

    void Set_Data(char *data);

    void Print_Message();
};

#pragma pack()

void Message::Set_Check()
{
    this->Check = 0;
    uint32_t sum = 0;
    uint16_t *p = (uint16_t *)this;
    for (int i = 0; i < sizeof(*this) / 2; i++)
    {
        sum += *p++;
        while (sum >> 16)
        {
            sum = (sum & 0xffff) + (sum >> 16);
        }
    }
    this->Check = ~(sum & 0xffff);
}
bool Message::CheckValid()
{
    uint32_t sum = 0;
    uint16_t *p = (uint16_t *)this;
    for (int i = 0; i < sizeof(*this) / 2; i++)
    {
        sum += *p++;
        while (sum >> 16)
        {
            sum = (sum & 0xffff) + (sum >> 16);
        }
    }
    return (sum & 0xffff) == 0xffff;
}

void Message::Set_Data(char *data)
{
    memset(this->Data, 0, MSS);
    memcpy(this->Data, data, sizeof(data));
}

void Message::Print_Message()
{
    std::cout << "Message "
         << "[SrcPort: " << this->SrcPort << " ] "
         << "[DstPort: " << this->DstPort << " ] "
         << "[Length: " << this->Length << " ] "
         << "[Seq: " << this->Seq << " ] ";
    if (this->Is_ACK())
    {
        std::cout << "[Ack: " << this->Ack << " ] ";
        std::cout << " ACK ";
    }
    if (this->Is_CFH())
    {
        std::cout << " CFH ";
    }
    if (this->Is_SYN())
    {
        std::cout << " SYN ";
    }
    if (this->Is_FIN())
    {
        std::cout << " FIN ";
    }
    std::cout << endl;
}

#endif