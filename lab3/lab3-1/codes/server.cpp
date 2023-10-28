#include "Message.h"
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
// #include <ws2tcpip.h>
#include <time.h>

SOCKET ServerSocket;
SOCKADDR_IN ServerAddr;
string ServerIP = "127.0.0.1";
int ServerAddrLen = sizeof(ServerAddr);

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
int Send_ACKSYN(Message &msg, int seq);
bool Server_Initial();
bool Connect();
void Receive_Message();
void Disconnect();
void Exit();

int main()
{
    WSADATA wsaData;

    if(!Server_Initial())
    {
        cout <<"[Server] "<< "Error in Initializing Server!" << endl;
        exit(EXIT_FAILURE);
    }

    cout<<"[Server] "<<"Server is ready! Waiting for connection"<<endl;

    if(!Connect())
    {
        cout <<"[Server] "<< "Error in Connecting!" << endl;
        exit(EXIT_FAILURE);
    }
    while(true)
    {
        int select = 0;
        cout<<"[Server] "<<"Please select the function you want to use:"<<endl;
        cout<<"[Server] "<<"1. Receive File"<<endl;
        cout<<"[Server] "<<"2. Exit"<<endl;
        cin>>select;
        switch(select)
        {
            case 1:
                Receive_Message();
                break;
            case 2:
                Disconnect();
                return 0;
            default:
                cout<<"[Server] "<<"Error in Selecting!"<<endl;
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
    msg.SrcPort = Server_Port;
    msg.DstPort = Router_Port;
    msg.Seq = Seq;
    msg.Ack = seq;
    msg.Set_ACK();
    msg.Set_Check();
    return sendto(ServerSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
int Send_ACKSYN(Message &msg, int seq)
{
    Seq++;
    msg.SrcIP = SrcIP;
    msg.DstIP = DstIP;
    msg.SrcPort = Server_Port;
    msg.DstPort = Router_Port;
    msg.Seq = Seq;
    msg.Ack = seq;
    msg.Set_ACK();
    msg.Set_SYN();
    msg.Set_Check();
    return sendto(ServerSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
int Send_ACKFIN(Message &msg, int seq)
{
    Seq++;
    msg.SrcIP = SrcIP;
    msg.DstIP = DstIP;
    msg.SrcPort = Server_Port;
    msg.DstPort = Router_Port;
    msg.Seq = Seq;
    msg.Ack = seq;
    msg.Set_ACK();
    msg.Set_FIN();
    msg.Set_Check();
    return sendto(ServerSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
bool Server_Initial()
{
    // 初始化DLL
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        perror("[Server] Error in Initializing Socket DLL!\n");
        cout << endl;
        exit(EXIT_FAILURE);
    }
    cout <<"[Server] "<< "Initializing Socket DLL is successful!\n" << endl;

    // 创建服务器端套接字
    ServerSocket = socket(AF_INET, SOCK_DGRAM, 0);
    unsigned long on = 1;
    ioctlsocket(ServerSocket, FIONBIO, &on);
    if (ServerSocket == INVALID_SOCKET)
    {
        cout <<"[Server] "<< "Error in Creating Socket!\n" << endl;
        exit(EXIT_FAILURE);
        return false;
    }
    cout <<"[Server] "<< "Creating Socket is successful!\n" << endl;

    // 绑定服务器地址
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(Server_Port);
    ServerAddr.sin_addr.S_un.S_addr = inet_addr(ServerIP.c_str());
    SrcIP = ServerAddr.sin_addr.S_un.S_addr;
    // if (inet_pton(AF_INET, "127.0.0.1", &(ServerAddr.sin_addr)) != 1)
    // {
    //     cout << "Error in Inet_pton" << endl;
    //     exit(EXIT_FAILURE);
    // }
    if (bind(ServerSocket, (SOCKADDR *)&ServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        cout <<"[Server] "<< "Error in Binding Socket!\n" << endl;
        exit(EXIT_FAILURE);
        return false;
    }
    cout <<"[Server] "<< "Binding Socket to port "<< Server_Port << "is successful!\n" << endl;

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
        if (recvfrom(ServerSocket, (char *)&con_msg[0], sizeof(con_msg[0]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            cout <<"[Server] "<< "Receive Message from Router! —— First-Way Handshake" << endl;
            con_msg[0].Print_Message();
            if (!(con_msg[0].Is_SYN() && con_msg[0].CheckValid() && con_msg[0].Seq == Seq + 1))
            {
                cout <<"[Server] "<< "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            cout<<"[Server] "<<"Error in Receiving Message! —— First-Way Handshake"<<endl;
            exit(EXIT_FAILURE);
        }
        // Seq++;
        // Second-Way Handshake
        int re = Send_ACKSYN(con_msg[1], con_msg[0].Seq);
        clock msg2_Send_Time = clock();
        if (re > 0)
        {
            cout <<"[Server] "<< "Send Message to Router! —— Second-Way Handshake" << endl;
            con_msg[1].Print_Message();
        }
        else
        {
            cout<<"[Server] "<<"Error in Sending Message! —— Second-Way Handshake"<<endl;
            exit(EXIT_FAILURE);
        }
        // Third-Way Handshake
        while(true)
        {
            if ((clock() - msg2_Send_Time) > Wait_Time)
            {
                cout <<"[Server] "<< "Time Out! —— Second-Way Handshake" << endl;
                int re = Send_ACKSYN(con_msg[1], con_msg[0].Seq);
                msg2_Send_Time = clock();
                if (re > 0)
                {
                    cout <<"[Server] "<< "Send Message to Router! —— Second-Way Handshake" << endl;
                    con_msg[1].Print_Message();
                }
                else
                {
                    cout<<"[Server] "<<"Error in Sending Message! —— Second-Way Handshake"<<endl;
                    exit(EXIT_FAILURE);
                }
            }
            if (recvfrom(ServerSocket, (char *)&con_msg[2], sizeof(con_msg[2]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
            {
                cout <<"[Server] "<< "Receive Message from Router! —— Third-Way Handshake" << endl;
                con_msg[2].Print_Message();
                if (!(con_msg[2].Is_ACK() && con_msg[2].CheckValid() && con_msg[2].Seq == Seq + 1 && con_msg[2].Ack == con_msg[1].Seq))
                {
                    cout <<"[Server] "<< "Error Message!" << endl;
                    exit(EXIT_FAILURE);
                }
                cout <<"[Server] "<< "Third-Way Handshake is successful!" << endl;
                // Seq++;
                return true;
            }
            else
            {
                cout<<"[Server] "<<"Error in Receiving Message! —— Third-Way Handshake"<<endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    return false;
}
void Receive_Message()
{
    Message rec_msg;
    while(true)
    {
        if (recvfrom(ServerSocket, (char *)&rec_msg, sizeof(rec_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            cout <<"[Server] "<< "Receive Message from Router!" << endl;
            rec_msg.Print_Message();
            if (rec_msg.Is_CFH() && rec_msg.CheckValid() && rec_msg.Seq == Seq + 1)
            {
                // Seq++;
                file_length = rec_msg.Length;
                memcpy(file_name, rec_msg->Data, sizeof(rec_msg->Data));
                cout <<"[Server] "<< "Receive File Name: " << file_name << " File Size: " << file_length << endl;

                Message reply_msg;
                if(Send_ACK(reply_msg,rec_msg.Seq)>0)
                {
                    cout<<"[Server] "<< "Receive Seq = "<<rec_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                }
            }
            else if (rec_msg.CheckValid() && rec_msg.Seq != Seq + 1)
            {
                Message reply_msg;
                if(Send_Ack(reply_msg,rec_msg.Seq)>0)
                {
                    cout<<"[Server] [!Repeatedly!]"<< "Receive Seq = "<<rec_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                }
            }
            else
            {
                cout <<"[Server] "<< "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            cout<<"[Server] "<<"Error in Receiving Message!"<<endl;
            exit(EXIT_FAILURE);
        }
    }
    int complete_num = file_length / MSS;
    int last_length = file_length % MSS;

    char *file_buffer = new char[file_length];
    cout <<"[Server] "<< "Start Receiving File!" << endl;
    for(int i = 0 ; i <= complete_num ; i++)
    {
        Message data_msg;
        while(true)
        {
            if (recvfrom(ServerSocket, (char *)&data_msg, sizeof(rec_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
            {
                cout <<"[Server] "<< "Receive Message from Router!" << endl;
                data_msg.Print_Message();
                if (data_msg.CheckValid() && data_msg.Seq == Seq + 1)
                {
                    // Seq++;
                    Message reply_msg;
                    if(Send_ACK(reply_msg,data_msg.Seq)>0)
                    {
                        cout<<"[Server] "<< "Receive Seq = "<<data_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                        break;
                    }
                }
                else if (data_msg.CheckValid() && data_msg.Seq != Seq + 1)
                {
                    Message reply_msg;
                    if(Send_ACK(reply_msg,data_msg.Seq)>0)
                    {
                        cout<<"[Server] [!Repeatedly!]"<< "Receive Seq = "<<data_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                    }
                }
                else
                {
                    cout <<"[Server] "<< "Error Message!" << endl;
                    exit(EXIT_FAILURE);
                }
            }
        }
        cout<<"[Server] "<< "Receive Successfully!"<<endl;
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
        cout<<"[Server] "<< "Finish Receiving!"<<endl;
        Recv_File = fopen(file_name, "Recv_File");
        if(file_buffer != NULL)
        {
            fwrite(file_buffer, sizeof(char), file_length, Recv_File);
            fclose(Recv_File);
            cout<<"[Server] "<< "Finish Writing File!"<<endl;
        }
        else
        {
            cout<<"[Server] "<< "Error in Writing File!"<<endl;
            exit(EXIT_FAILURE);
        }
    }
}
void Disconnect()//Router端主动断开连接
{
    Message discon_msg[4];
    while(true)
    {
        // First-Way Wavehand
        if (recvfrom(ServerSocket, (char *)&con_msg[0], sizeof(con_msg[0]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            cout <<"[Server] "<< "Receive Message from Router! —— First-Way Wavehand" << endl;
            discon_msg[0].Print_Message();
            if (!(discon_msg[0].Is_FIN() && discon_msg[0].CheckValid() && discon_msg[0].Seq == Seq + 1))
            {
                cout <<"[Server] "<< "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            cout<<"[Server] "<<"Error in Receiving Message! —— First-Way Wavehand"<<endl;
            exit(EXIT_FAILURE);
        }
        // Seq++;
        // Second-Way Wavehand
        int re = Send_ACK(discon_msg[1], discon_msg[0].Seq);
        // clock dismsg2_Send_Time = clock();
        if (re > 0)
        {
            cout <<"[Server] "<< "Send Message to Router! —— Second-Way Wavehand" << endl;
            discon_msg[1].Print_Message();
        }
        else
        {
            cout<<"[Server] "<<"Error in Sending Message! —— Second-Way Wavehand"<<endl;
            exit(EXIT_FAILURE);
        }
        // Seq++;
        break;
    }
    // Third-Way Wavehand
    int re = Send_ACKFIN(discon_msg[2], discon_msg[1].Seq);
    clock dismsg3_Send_Time = clock();
    if (re > 0)
    {
        cout <<"[Server] "<< "Send Message to Router! —— Third-Way Wavehand" << endl;
        discon_msg[2].Print_Message();
    }
    else
    {
        cout<<"[Server] "<<"Error in Sending Message! —— Third-Way Wavehand"<<endl;
        exit(EXIT_FAILURE);
    }
    // Fourth-Way Wavehand
    while(true)
    {
        if ((clock() - dismsg3_Send_Time) > Wait_Time)
        {
            cout <<"[Server] "<< "Time Out! —— Third-Way Wavehand" << endl;
            int re = Send_ACKFIN(discon_msg[2], discon_msg[1].Seq);
            dismsg3_Send_Time = clock();
            if (re > 0)
            {
                cout <<"[Server] "<< "Send Message to Router! —— Third-Way Wavehand" << endl;
                discon_msg[2].Print_Message();
            }
            else
            {
                cout<<"[Server] "<<"Error in Sending Message! —— Third-Way Wavehand"<<endl;
                exit(EXIT_FAILURE);
            }
        }
        if (recvfrom(ServerSocket, (char *)&discon_msg[3], sizeof(discon_msg[3]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            cout <<"[Server] "<< "Receive Message from Router! —— Fourth-Way Wavehand" << endl;
            discon_msg[3].Print_Message();
            if (!(discon_msg[3].Is_ACK() && discon_msg[3].CheckValid() && discon_msg[3].Seq == Seq + 1 && discon_msg[3].Ack == discon_msg[2].Seq))
            {
                cout <<"[Server] "<< "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            cout <<"[Server] "<< "Fourth-Way Wavehand is successful!" << endl;
            // Seq++;
            break;
        }
        else
        {
            cout<<"[Server] "<<"Error in Receiving Message! —— Fourth-Way Wavehand"<<endl;
            exit(EXIT_FAILURE);
        }
    }
    Exit();
    return;
}
void Exit()
{
    // Sleep(Exit_Time);
    closesocket(ServerSocket);
    WSACleanup();
    cout<<"[Server] "<< "Server is closed!"<<endl;
    system("pause");
}