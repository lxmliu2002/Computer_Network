#ifndef SERVER_MESSAGE_H
#define SERVER_MESSAGE_H

#include <iostream>
#include <WinSock2.h>
#include <stdlib.h>


using namespace std;

#pragma comment(lib, "Ws2_32.lib")


#define File_Size 1024 * 8
#define MSS 1010
#define RSP 0b1
#define RST 0b10
#define ACK 0b100
#define SYN 0b1000
#define FIN 0b10000

#define _CRT_SECURE_NO_WARNINGS //禁止使用不安全的函数报错
#define _WINSOCK_DEPRECATED_NO_WARNINGS //禁止使用旧版本的函数报错

/* * 消息头部设计
 * |0             7|8             15|
 * ----------------------------------
 * |             SrcIP              |
 * ----------------------------------
 * |             DestIP             |
 * ----------------------------------
 * |    SrcPort    |    DestPort    |
 * ----------------------------------
 * |            SeqNum              |
 * ----------------------------------
 * |            AckNum              |
 * ----------------------------------
 * |            Length              |
 * ----------------------------------
 * |      Flag     |    CheckSum    |
 * ----------------------------------
 * |              Data              |
 * ----------------------------------
 *
 * Flag
 * |0  |1  |2  |3  |4  |5     6     7|
 * -----------------------------------
 * |RSP|RST|ACK|SYN|FIN|             |
 * -----------------------------------
 */
#pragma pack(1)
struct Message
{

    uint16_t SrcIP;
    uint16_t DstIP;
    uint8_t SrcPort;
    uint8_t DstPort;
    uint16_t Seq;
    uint16_t Ack;
    uint16_t Length;
    uint8_t Flag;
    uint8_t Check;
    char *Data[MSS];

    Message() : SrcIP(0), DestIP(0), SrcPort(0), DestPort(0), SeqNum(0), AckNum(0), Length(0), Flag(0), CheckSum(0) { memset(this->Data, 0, MSS); }

    // void Set_SrcIP(uint16_t SrcIP) { this->SrcIP = SrcIP; }
    // void Set_DstIP(uint16_t DstIP) { this->DstIP = DstIP; }
    // void Set_SrcPort(uint8_t SrcPort) { this->SrcPort = SrcPort; }
    // void Set_DstPort(uint8_t DstPort) { this->DstPort = DstPort; }
    // void Set_Seq(uint16_t Seq) { this->Seq = Seq; }
    // void Set_Ack(uint16_t Ack) { this->Ack = Ack; }
    // void Set_Length(uint16_t Length) { this->Length = Length; }
    //uint16_t Get_Length() { return this->Length; }

    void Set_RSP() { this->Flag |= RSP; }
    bool Is_RSP() { return this->Flag & RSP; }
    void Set_RST() { this->Flag |= RST; }
    bool Is_RST() { return this->Flag & RST; }
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
    uint8_t *p = (uint8_t *)this;
    for (int i = 0; i < sizeof(Message) / 2; i++)
    {
        sum += p[i];
        while (sum >> 16)
        {
            sum = (sum & 0xffff) + (sum >> 16);
        }
    }
    this->Check = ~(sum & 0xffff); // 取反并保持其为16位
}
bool Message::CheckValid()
{
    uint32_t sum = 0;
    uint8_t *p = (uint8_t *)this;
    for (int i = 0; i < sizeof(Message) / 2; i++)
    {
        sum += p[i];
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
    memcpy(this->Data, data, Length);
}

void Message::Print_Message()
{
    cout << "Message "
         <<"[SrcIP:"<<this->SrcIP<<"]"
         <<"[DstIP:"<<this->DstIP<<"]"
         <<"[SrcPort:"<<this->SrcPort<<"]"
         <<"[DstPort:"<<this->DstPort<<"]"
         <<"[Seq:"<<this->Seq<<"]"
         <<"[Ack:"<<this->Ack<<"]"
         <<"[Length:"<<this->Length<<"]"
         <<"[RSP:"<<this->Is_RSP()<<"]"
         <<"[RST:"<<this->Is_RST()<<"]"
         <<"[ACK:"<<this->Is_ACK()<<"]"
         <<"[SYN:"<<this->Is_SYN()<<"]"
         <<"[FIN:"<<this->Is_FIN()<<"]"
         <<endl;
}

#endif