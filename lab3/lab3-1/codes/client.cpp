#include "Mseeage.h"
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
// #include <ws2tcpip.h>
#include <time.h>

#define Router_Port 12345
#define Client_Port 54321
#define Wait_Time 3000

SOCKET ClientSocket;
SOCKADDR_IN ClientAddr;
string ClientIP = "127.0.0.1";
int ClientAddrLen = sizeof(ClientAddr);

SOCKADDR_IN RouterAddr;
string RouterIP = "127.0.0.1";
int RouterAddrLen = sizeof(RouterAddr);

uint16_t SrcIP;
uint16_t DstIP;

uint16_t file_length;
char *file_name;
FILE *Recv_File;
int Seq = 0;


int Send_ACK(Message &msg, int seq);
int Send_SYN(Message &msg);
int Send_ACKSYN(Message &msg, int seq);
int Send_FIN(Message &msg, int seq);
bool Client_Initial();
bool Connect();
void Receive_Message();
void Disconnect();
void Wait_Exit();

int main()
{
    WSADATA wsaData;

    if(!Client_Initial())
    {
        cout <<"[Client] "<< "Error in Initializing Client!" << endl;
        exit(EXIT_FAILURE);
    }

    cout<<"[Client] "<<"Client is ready! Trying to connection"<<endl;

    if(!Connect())
    {
        cout <<"[Client] "<< "Error in Connecting!" << endl;
        exit(EXIT_FAILURE);
    }
    while(true)
    {
        int select = 0;
        cout<<"[Client] "<<"Please select the function you want to use:"<<endl;
        cout<<"[Client] "<<"1. Send File"<<endl;
        cout<<"[Client] "<<"2. Exit"<<endl;
        cin>>select;
        switch(select)
        {
            case 1:
                Send_Message();
                break;
            case 2:
                Disconnect();
                return 0;
            default:
                cout<<"[Client] "<<"Error in Selecting!"<<endl;
                exit(EXIT_FAILURE);
        }
    }
    return 0;
}

int Send_ACK(Message &msg, int seq)
{
    Seq++;
    msg.SrcIP = SrcIP;
    msg.DstIP = DstIP;
    msg.SrcPort = Client_Port;
    msg.DstPort = Router_Port;
    msg.Seq = Seq;
    msg.Ack = seq;
    msg.Set_ACK();
    msg.Set_Check();
    return sendto(ClientSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
int Send_SYN(Message &msg)
{
    Seq++;
    msg.SrcIP = SrcIP;
    msg.DstIP = DstIP;
    msg.SrcPort = Client_Port;
    msg.DstPort = Router_Port;
    msg.Seq = Seq;
    msg.Set_SYN();
    msg.Set_Check();
    return sendto(ClientSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
int Send_FIN(Message &msg, int seq)
{
    Seq++;
    msg.SrcIP = SrcIP;
    msg.DstIP = DstIP;
    msg.SrcPort = Client_Port;
    msg.DstPort = Router_Port;
    msg.Seq = Seq;
    msg.Ack = seq;
    msg.Set_FIN();
    msg.Set_Check();
    return sendto(ClientSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
int Send_ACKSYN(Message &msg, int seq)
{
    Seq++;
    msg.SrcIP = SrcIP;
    msg.DstIP = DstIP;
    msg.SrcPort = Client_Port;
    msg.DstPort = Router_Port;
    msg.Seq = Seq;
    msg.Ack = seq;
    msg.Set_ACK();
    msg.Set_SYN();
    msg.Set_Check();
    return sendto(ClientSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
int Send_ACKFIN(Message &msg, int seq)
{
    Seq++;
    msg.SrcIP = SrcIP;
    msg.DstIP = DstIP;
    msg.SrcPort = Client_Port;
    msg.DstPort = Router_Port;
    msg.Seq = Seq;
    msg.Ack = seq;
    msg.Set_ACK();
    msg.Set_FIN();
    msg.Set_Check();
    return sendto(ClientSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
bool Client_Initial()
{
    // 初始化DLL
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        perror("[Client] Error in Initializing Socket DLL!\n");
        cout << endl;
        exit(EXIT_FAILURE);
    }
    cout <<"[Client] "<< "Initializing Socket DLL is successful!\n" << endl;

    // 创建客户端套接字
    ClientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    unsigned long on = 1;
    ioctlsocket(ClientSocket, FIONBIO, &on);
    if (ClientSocket == INVALID_SOCKET)
    {
        cout <<"[Client] "<< "Error in Creating Socket!\n" << endl;
        exit(EXIT_FAILURE);
        return false;
    }
    cout <<"[Client] "<< "Creating Socket is successful!\n" << endl;

    // 绑定客户端地址
    ClientAddr.sin_family = AF_INET;
    ClientAddr.sin_port = htons(Client_Port);
    ClientAddr.sin_addr.S_un.S_addr = inet_addr(ClientIP.c_str());
    SrcIP = ClientAddr.sin_addr.S_un.S_addr;
    // if (inet_pton(AF_INET, "127.0.0.1", &(ClientAddr.sin_addr)) != 1)
    // {
    //     cout << "Error in Inet_pton" << endl;
    //     exit(EXIT_FAILURE);
    // }
    if (bind(ClientSocket, (SOCKADDR *)&ClientAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        cout <<"[Client] "<< "Error in Binding Socket!\n" << endl;
        exit(EXIT_FAILURE);
        return false;
    }
    cout <<"[Client] "<< "Binding Socket to port "<< Client_Port<<"is successful!\n" << endl;

    // 初始化路由器地址
    RouterAddr.sin_family = AF_INET;
    RouterAddr.sin_port = htons(Router_Port);
    RouterAddr.sin_addr.S_un.S_addr = inet_addr(RouterIP.c_str());
    DstIP = RouterAddr.sin_addr.S_un.S_addr;
    // if (inet_pton(AF_INET, "127.0.0.1", &(RouterAddr.sin_addr)) != 1)
    // {
    //     cout << "Error in Inet_pton" << endl;
    //     exit(EXIT_FAILURE);
    // }

    return true;
}
bool Connect()
{
    //三次握手
    Message con_msg[3];
    while(true)
    {
        // First-Way Handshake
        // Seq++;
        int re = Send_SYN(con_msg[0]);
        clock msg1_Send_Time = clock();
        if (re > 0)
        {
            cout <<"[Client] "<< "Send Message to Router! —— First-Way Handshake" << endl;
            con_msg[0].Print_Message();
        }
        else
        {
            cout<<"[Client] "<<"Error in Sending Message! —— First-Way Handshake"<<endl;
            exit(EXIT_FAILURE);
        }
        // Second-Way Handshake
        while(true)
        {
            if ((clock() - msg1_Send_Time) > Wait_Time)
            {
                cout <<"[Client] "<< "Time Out! —— First-Way Handshake" << endl;
                int re = Send_SYN(con_msg[0]);
                msg1_Send_Time = clock();
                if (re > 0)
                {
                    cout <<"[Client] "<< "Send Message to Router! —— First-Way Handshake" << endl;
                    con_msg[0].Print_Message();
                }
                else
                {
                    cout<<"[Client] "<<"Error in Sending Message! —— First-Way Handshake"<<endl;
                    exit(EXIT_FAILURE);
                }
            }
            if (recvfrom(ClientSocket, (char *)&con_msg[1], sizeof(con_msg[1]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
            {
                cout <<"[Client] "<< "Receive Message from Router! —— Second-Way Handshake" << endl;
                con_msg[1].Print_Message();
                if (!(con_msg[1].Is_ACK() && con_msg[1].Is_SYN() &&con_msg[1].CheckValid() && con_msg[1].Seq == Seq + 1 && con_msg[1].Ack == con_msg[0].Seq))
                {
                    cout <<"[Client] "<< "Error Message!" << endl;
                    exit(EXIT_FAILURE);
                }
                cout <<"[Client] "<< "Third-Way Handshake is successful!" << endl;
                // Seq++;
                return true;
            }
            else
            {
                cout<<"[Client] "<<"Error in Receiving Message! —— Second-Way Handshake"<<endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    // Third-Way Handshake
    int re = Send_ACK(con_msg[2], con_msg[1].Seq);
    if (re > 0)
    {
        cout <<"[Client] "<< "Send Message to Router! —— Third-Way Handshake" << endl;
        con_msg[2].Print_Message();
    }
    else
    {
        cout<<"[Client] "<<"Error in Sending Message! —— Third-Way Handshake"<<endl;
        exit(EXIT_FAILURE);
    }
    cout<<"[Client] "<< "Third-Way Handshake is successful!" << endl;
    return true;
}
// TODO 
void Send_Message()
{
    Message rec_msg;
    while(true)
    {
        if (recvfrom(ClientSocket, (char *)&rec_msg, sizeof(rec_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            cout <<"[Client] "<< "Receive Message from Router!" << endl;
            rec_msg.Print_Message();
            if (rec_msg.Is_CFH() && rec_msg.CheckValid() && rec_msg.Seq == Seq + 1)
            {
                Seq++;
                file_length = rec_msg.Length;
                memcpy(file_name, rec_msg->Data, sizeof(rec_msg->Data));
                cout <<"[Client] "<< "Receive File Name: " << file_name << " File Size: " << file_length << endl;

                Message reply_msg;
                if(Send_ACK(reply_msg,rec_msg.Seq)>0)
                {
                    cout<<"[Client] "<< "Receive Seq = "<<rec_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                }
            }
            else if (rec_msg.CheckValid() && rec_msg.Seq != Seq + 1)
            {
                Message reply_msg;
                if(Send_Ack(reply_msg,rec_msg.Seq)>0)
                {
                    cout<<"[Client] [!Repeatedly!]"<< "Receive Seq = "<<rec_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                }
            }
            else
            {
                cout <<"[Client] "<< "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            cout<<"[Client] "<<"Error in Receiving Message!"<<endl;
            exit(EXIT_FAILURE);
        }
    }
    int complete_num = file_length / MSS;
    int last_length = file_length % MSS;

    char *file_buffer = new char[file_length];
    cout <<"[Client] "<< "Start Receiving File!" << endl;
    for(int i = 0 ; i <= complete_num ; i++)
    {
        Message data_msg;
        while(true)
        {
            if (recvfrom(ClientSocket, (char *)&data_msg, sizeof(rec_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
            {
                cout <<"[Client] "<< "Receive Message from Router!" << endl;
                data_msg.Print_Message();
                if (data_msg.CheckValid() && data_msg.Seq == Seq + 1)
                {
                    Seq++;
                    Message reply_msg;
                    if(Send_ACK(reply_msg,data_msg.Seq)>0)
                    {
                        cout<<"[Client] "<< "Receive Seq = "<<data_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                        break;
                    }
                }
                else if (data_msg.CheckValid() && data_msg.Seq != Seq + 1)
                {
                    Message reply_msg;
                    if(Send_ACK(reply_msg,data_msg.Seq)>0)
                    {
                        cout<<"[Client] [!Repeatedly!]"<< "Receive Seq = "<<data_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                    }
                }
                else
                {
                    cout <<"[Client] "<< "Error Message!" << endl;
                    exit(EXIT_FAILURE);
                }
            }
        }
        cout<<"[Client] "<< "Receive Successfully!"<<endl;
        if (i != complete_num)
        {
            for(int j = 0 ; j < MSS ; j++)
            {
                file_buffer[i * MSS + j] = data_msg.Data[j];
            }
        }
        else
        {
            for(int j = 0 ; j < last_length ; j++)
            {
                file_buffer[complete_num * MSS + j] = data_msg.Data[j];
            }
        }
        cout<<"[Client] "<< "Finish Receiving!"<<endl;
        Recv_File = fopen(file_name, "Recv_File");
        if(file_buffer != NULL)
        {
            fwrite(file_buffer, sizeof(char), file_length, Recv_File);
            fclose(Recv_File);
            cout<<"[Client] "<< "Finish Writing File!"<<endl;
        }
        else
        {
            cout<<"[Client] "<< "Error in Writing File!"<<endl;
            exit(EXIT_FAILURE);
        }
    }
}
void Disconnect()//Client端主动断开连接
{
    Message discon_msg[4];

    // First-Way Wavehand
    int re = Send_FIN(discon_msg[0], Seq);
    clock dismsg0_Send_Time = clock();
    if (re > 0)
    {
        cout <<"[Client] "<< "Send Message to Router! —— First-Way Wavehand" << endl;
        discon_msg[0].Print_Message();
    }
    else
    {
        cout<<"[Client] "<<"Error in Sending Message! —— First-Way Wavehand"<<endl;
        exit(EXIT_FAILURE);
    }

    // Second-Way Wavehand
    while(true)
    {
        if ((clock() - dismsg0_Send_Time) > Wait_Time)
        {
            cout <<"[Client] "<< "Time Out! —— First-Way Wavehand" << endl;
            int re = Send_SYN(con_msg[0]);
            msg1_Send_Time = clock();
            if (re > 0)
            {
                cout <<"[Client] "<< "Send Message to Router! —— First-Way Wavehand" << endl;
                con_msg[0].Print_Message();
            }
            else
            {
                cout<<"[Client] "<<"Error in Sending Message! —— First-Way Wavehand"<<endl;
                exit(EXIT_FAILURE);
            }
        }
        if (recvfrom(ClientSocket, (char *)&discon_msg[1], sizeof(discon_msg[1]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            cout <<"[Client] "<< "Receive Message from Router! —— Second-Way Wavehand" << endl;
            discon_msg[1].Print_Message();
            if (!(discon_msg[1].Is_ACK() && discon_msg[1].CheckValid() && discon_msg[1].Seq == Seq + 1 && discon_msg[1].Ack == discon_msg[0].Seq))
            {
                cout <<"[Client] "<< "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            cout <<"[Client] "<< "Second-Way Wavehand is successful!" << endl;
            // Seq++;
            break;
        }
        else
        {
            cout<<"[Client] "<<"Error in Receiving Message! —— Second-Way Wavehand"<<endl;
            exit(EXIT_FAILURE);
        }
    }
    // Third-Way Wavehand
    while(true)
    {
        if (recvfrom(ClientSocket, (char *)&discon_msg[2], sizeof(discon_msg[2]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            cout <<"[Client] "<< "Receive Message from Router! —— Third-Way Wavehand" << endl;
            discon_msg[2].Print_Message();
            if (!(discon_msg[2].Is_ACK() && discon_msg[2].Is_FIN() && discon_msg[2].CheckValid() && discon_msg[2].Seq == Seq + 1 && discon_msg[2].Ack == discon_msg[1].Seq))
            {
                cout <<"[Client] "<< "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            cout <<"[Client] "<< "Third-Way Wavehand is successful!" << endl;
            // Seq++;
            break;
        }
        else
        {
            cout<<"[Client] "<<"Error in Receiving Message! —— Third-Way Wavehand"<<endl;
            exit(EXIT_FAILURE);
        }
    }
    // Fourth-Way Wavehand
    int re = Send_ACK(discon_msg[3], discon_msg[2].Seq);
    // clock dismsg3_Send_Time = clock();
    if (re > 0)
    {
        cout <<"[Client] "<< "Send Message to Router! —— Fourth-Way Wavehand" << endl;
        discon_msg[3].Print_Message();
    }
    else
    {
        cout<<"[Client] "<<"Error in Sending Message! —— Fourth-Way Wavehand"<<endl;
        exit(EXIT_FAILURE);
    }
    cout<<"[Client] "<< "Fourth-Way Wavehand is successful!" << endl;

    Wait_Exit();
    return;
}
void Wait_Exit()
{
    Message exit_msg;
    clock exit_msg_time = clock();
    while(clock() - exit_msg_time < 2 * Wait_Time)
    {
        if (recvfrom(ClientSocket, (char *)&exit_msg, sizeof(exit_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            Send_ACK(exit_msg, Seq);
        }
    }
    closesocket(ClientSocket);
    WSACleanup();
    cout<<"[Client] "<< "Client is closed!" << endl;
    system("pause");
}