#include "Defs.h"

// #define Router_Port 54321
// #define Server_Port 65432
// // #define Client_Port 54321

WSADATA wsaData;


int Seq = 0;

int Send(Message &msg);
bool Server_Initial();
bool Connect();
void Receive_Message();
void Disconnect();
void Exit();

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        ServerIP = argv[1];
        RouterIP = argv[2];
    }

    if (!Server_Initial())
    {
        cout << "[Server] "
             << "Error in Initializing Server!" << endl;
        exit(EXIT_FAILURE);
    }

    cout << "[Server] "
         << "Server is ready! Waiting for connection" << endl;

    if (!Connect())
    {
        cout << "[Server] "
             << "Error in Connecting!" << endl;
        exit(EXIT_FAILURE);
    }
    while (true)
    {
        int select = 0;
        cout << "[Server] "
             << "Please select the function you want to use:" << endl;
        cout << "[Server] "
             << "1. Receive File" << endl;
        cout << "[Server] "
             << "2. Exit" << endl;
        cin >> select;
        switch (select)
        {
        case 1:
            cout << "[Server] "
                 << "Waiting for Receiving file..." << endl;
            Receive_Message();
            break;
        case 2:
            Disconnect();
            return 0;
        default:
            cout << "[Server] "
                 << "Error in Selecting!" << endl;
            exit(EXIT_FAILURE);
        }
    }
    system("pause");
    return 0;
}

int Send(Message &msg)
{
    msg.SrcPort = Server_Port;
    msg.DstPort = Router_Port;
    msg.Set_Check();
    return sendto(ServerSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
bool Server_Initial()
{
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        perror("[Server] Error in Initializing Socket DLL!\n");
        exit(EXIT_FAILURE);
    }
    cout << "[Server] "
         << "Initializing Socket DLL is successful!\n";

    ServerSocket = socket(AF_INET, SOCK_DGRAM, 0);
    unsigned long on = 1;
    ioctlsocket(ServerSocket, FIONBIO, &on);
    if (ServerSocket == INVALID_SOCKET)
    {
        cout << "[Server] "
             << "Error in Creating Socket!\n";
        exit(EXIT_FAILURE);
        return false;
    }
    cout << "[Server] "
         << "Creating Socket is successful!\n";

    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(Server_Port);
    ServerAddr.sin_addr.S_un.S_addr = inet_addr(ServerIP.c_str());
    // if (inet_pton(AF_INET, "127.0.0.1", &(ServerAddr.sin_addr)) != 1)
    // {
    //     cout << "Error in Inet_pton" << endl;
    //     exit(EXIT_FAILURE);
    // }
    if (bind(ServerSocket, (SOCKADDR *)&ServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        cout << "[Server] "
             << "Error in Binding Socket!\n";
        exit(EXIT_FAILURE);
        return false;
    }
    cout << "[Server] "
         << "Binding Socket to port " << Server_Port << " is successful!" << endl
         << endl;

    RouterAddr.sin_family = AF_INET;
    RouterAddr.sin_port = htons(Router_Port);
    RouterAddr.sin_addr.S_un.S_addr = inet_addr(RouterIP.c_str());
    // if (inet_pton(AF_INET, "127.0.0.1", &(RouterAddr.sin_addr)) != 1)
    // {
    //     cout << "Error in Inet_pton" << endl;
    //     exit(EXIT_FAILURE);
    // }

    return true;
}
bool Connect()
{
    Message con_msg[3];
    while (true)
    {
        // * First-Way Handshake
        if (recvfrom(ServerSocket, (char *)&con_msg[0], sizeof(con_msg[0]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            // cout <<"[Server] "<< "Receive Message from Router! -- First-Way Handshake" << endl;
            con_msg[0].Print_Message();
            if (!(con_msg[0].Is_SYN() && con_msg[0].CheckValid() && con_msg[0].Seq == Seq + 1))
            {
                cout << "[Server] "
                     << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Seq = con_msg[0].Seq;
            // * Second-Way Handshake
            con_msg[1].Ack = con_msg[0].Seq;
            con_msg[1].Seq = ++Seq;
            con_msg[1].Set_ACK();
            con_msg[1].Set_SYN();
            int re = Send(con_msg[1]);
            float msg2_Send_Time = clock();
            if (re > 0)
            {
                // cout <<"[Server] "<< "Send Message to Router! -- Second-Way Handshake" << endl;
                con_msg[1].Print_Message();
                // * Third-Way Handshake
                while (true)
                {
                    if (recvfrom(ServerSocket, (char *)&con_msg[2], sizeof(con_msg[2]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
                    {
                        // cout <<"[Server] "<< "Receive Message from Router! -- Third-Way Handshake" << endl;
                        con_msg[2].Print_Message();
                        if (!(con_msg[2].Is_ACK() && con_msg[2].CheckValid() && con_msg[2].Seq == Seq + 1 && con_msg[2].Ack == con_msg[1].Seq))
                        {
                            cout << "[Server] "
                                 << "Error Message!" << endl;
                            exit(EXIT_FAILURE);
                        }
                        Seq = con_msg[2].Seq;
                        // cout <<"[Server] "<< "Third-Way Handshake is successful!" << endl;
                        return true;
                    }
                    if ((clock() - msg2_Send_Time) > Wait_Time)
                    {
                        int re = Send(con_msg[1]);
                        msg2_Send_Time = clock();
                        if (re > 0)
                        {
                            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                            cout << "[Server] "
                                 << "Time Out! -- Send Message to Router! -- Second-Way Handshake" << endl;
                            con_msg[1].Print_Message();
                            SetConsoleTextAttribute(hConsole, 7);
                        }
                    }
                }
            }
        }
    }
    return false;
}
void Receive_Message()
{
    char file_name[MSS] = {};
    Message rec_msg;
    while (true)
    {
        if (recvfrom(ServerSocket, (char *)&rec_msg, sizeof(rec_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            // cout <<"[Server] "<< "Receive Message from Router!" << endl;
            rec_msg.Print_Message();
            if (rec_msg.Is_CFH() && rec_msg.CheckValid() && rec_msg.Seq == Seq + 1)
            {
                Seq = rec_msg.Seq;
                file_length = rec_msg.Length;
                for (int i = 0; rec_msg.Data[i]; i++)
                    file_name[i] = rec_msg.Data[i];
                cout << "[Server] "
                     << "Receive File Name: " << file_name << " File Size: " << file_length << endl;
                Message reply_msg;
                reply_msg.Ack = rec_msg.Seq;
                reply_msg.Seq = ++Seq;
                reply_msg.Set_ACK();
                if (Send(reply_msg) > 0)
                {
                    // cout<<"[Server] "<< "Receive Seq = "<<rec_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                    break;
                }
            }
            else if (rec_msg.CheckValid() && rec_msg.Seq != Seq + 1)
            {
                Message reply_msg;
                reply_msg.Ack = rec_msg.Seq;
                // reply_msg.Seq = ++Seq;
                reply_msg.Set_ACK();
                if (Send(reply_msg) > 0)
                {
                    // cout<<"[Server] [!Repeatedly!]"<<" Reply Ack = "<<reply_msg.Ack<<endl;
                }
            }
            else
            {
                cout << "[Server] "
                     << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    ofstream Recv_File(file_name, ios::binary);
    // Recv_File = fopen(file_name, "wb");
    int complete_num = file_length / MSS;
    int last_length = file_length % MSS;

    // char *file_buffer = new char[MSS];
    cout << "[Server] "
         << "Start Receiving File!" << endl;
    for (int i = 0; i <= complete_num; i++)
    {
        while (true)
        {
            Message data_msg;
            if (recvfrom(ServerSocket, (char *)&data_msg, sizeof(rec_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
            {
                // cout <<"[Server] "<< "Receive Message from Router!" << endl;
                data_msg.Print_Message();
                if(data_msg.Seq < Seq + 1)
                {
                    continue;
                }
                if (data_msg.CheckValid() && data_msg.Seq == Seq + 1)
                {
                    Seq = data_msg.Seq;
                    Message reply_msg;
                    reply_msg.Ack = data_msg.Seq;
                    reply_msg.Seq = ++Seq;
                    reply_msg.Set_ACK();
                    if (Send(reply_msg) > 0)
                    {
                        // cout<<"[Server] "<< "Receive Seq = "<<data_msg.Seq<<" Reply Ack = "<<reply_msg.Ack<<endl;
                        if (i != complete_num)
                        {
                            Recv_File.write(data_msg.Data, MSS);
                        }
                        else
                        {
                            Recv_File.write(data_msg.Data, last_length);
                        }
                        break;
                    }
                }
                else if (data_msg.CheckValid() && data_msg.Seq != Seq + 1)
                {
                    Message reply_msg;
                    reply_msg.Ack = data_msg.Seq;
                    // reply_msg.Seq = ++Seq;
                    reply_msg.Set_ACK();
                    if (Send(reply_msg) > 0)
                    {
                        // cout<<"[Server] [!Repeatedly!]"<< " Reply Ack = "<<reply_msg.Ack<<endl;
                    }
                }
                else
                {
                    cout << "[Server] "
                         << "Error Message!" << endl;
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    cout << "[Server] "
         << "Finish Receiving!" << endl;
    // cout<<"[Server] "<< "Start Writing File!"<<endl;
}
void Disconnect() // * Router端主动断开连接
{
    Message discon_msg[4];
    while (true)
    {
        // * First-Way Wavehand
        if (recvfrom(ServerSocket, (char *)&discon_msg[0], sizeof(discon_msg[0]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            // cout <<"[Server] "<< "Receive Message from Router! -- First-Way Wavehand" << endl;
            if(discon_msg[0].Seq < Seq + 1)
            {
                continue;
            }
            if (!(discon_msg[0].Is_FIN() && discon_msg[0].CheckValid() && discon_msg[0].Seq == Seq + 1))
            {
                cout << "[Server] "
                     << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Seq = discon_msg[0].Seq;
        }
        // * Second-Way Wavehand
        discon_msg[1].Ack = discon_msg[0].Seq;
        discon_msg[1].Seq = ++Seq;
        discon_msg[1].Set_ACK();
        int re = Send(discon_msg[1]);
        // clock dismsg2_Send_Time = clock();
        if (re > 0)
        {
            // cout <<"[Server] "<< "Send Message to Router! -- Second-Way Wavehand" << endl;
            discon_msg[1].Print_Message();
        }
        break;
    }
    // * Third-Way Wavehand
    discon_msg[2].Ack = discon_msg[1].Seq;
    discon_msg[2].Seq = ++Seq;
    discon_msg[2].Set_ACK();
    discon_msg[2].Set_FIN();
    int re = Send(discon_msg[2]);
    float dismsg3_Send_Time = clock();
    if (re > 0)
    {
        // cout <<"[Server] "<< "Send Message to Router! -- Third-Way Wavehand" << endl;
        discon_msg[2].Print_Message();
    }
    // * Fourth-Way Wavehand
    while (true)
    {
        if (recvfrom(ServerSocket, (char *)&discon_msg[3], sizeof(discon_msg[3]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            // cout <<"[Server] "<< "Receive Message from Router! -- Fourth-Way Wavehand" << endl;
            discon_msg[3].Print_Message();
            if (discon_msg[3].Seq < Seq + 1)
            {
                continue;
            }
            else if (!(discon_msg[3].Is_ACK() && discon_msg[3].CheckValid() && discon_msg[3].Seq == Seq + 1 && discon_msg[3].Ack == discon_msg[2].Seq))
            {
                cout << "[Server] "
                     << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Seq = discon_msg[3].Seq;
            cout << "[Server] "
                 << "Fourth-Way Wavehand is successful!" << endl;
            break;
        }
        if ((clock() - dismsg3_Send_Time) > Wait_Time)
        {
            int re = Send(discon_msg[2]);
            dismsg3_Send_Time = clock();
            if (re > 0)
            {
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                cout 
                     << "Time Out! -- Send Message to Router! -- Third-Way Wavehand" << endl;
                discon_msg[2].Print_Message();
                SetConsoleTextAttribute(hConsole, 7);
            }
        }
    }
    Exit();
    return;
}
void Exit()
{
    closesocket(ServerSocket);
    WSACleanup();
    cout << "[Server] "
         << "Server is closed!" << endl;
    system("pause");
}
