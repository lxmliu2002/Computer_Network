#include "Defs_GBN.h"

#define Router_Port 54321
#define Server_Port 65432

WSADATA wsaData;

int Seq = 0;
int Msg_Num = 0;
atomic<int> Header_Seq(0);
atomic_int Sleep_Time(0);
float Last_Time = 0;

int Send(Message &msg);
void Server_Initial();
void Connect();
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

    Server_Initial();
    cout << "Server is ready! Waiting for connection" << endl;
    Connect();

    while (true)
    {
        int select = 0;
        cout << "Please select the function you want to use:" << endl;
        cout << "1. Receive File" << endl;
        cout << "2. Exit" << endl;
        cout << "> ";
        cin >> select;
        switch (select)
        {
        case 1:
            cout << "Waiting for Receiving file..." << endl;
            Receive_Message();
            break;
        case 2:
            Disconnect();
            return 0;
        default:
            cout << "Error in Selecting!" << endl;
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
void Server_Initial()
{
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        perror("[Server] Error in Initializing Socket DLL!\n");
        exit(EXIT_FAILURE);
    }
    cout << "Initializing Socket DLL is successful!\n";

    ServerSocket = socket(AF_INET, SOCK_DGRAM, 0);
    unsigned long on = 1;
    ioctlsocket(ServerSocket, FIONBIO, &on);
    if (ServerSocket == INVALID_SOCKET)
    {
        cout << "Error in Creating Socket!\n";
        exit(EXIT_FAILURE);
    }
    cout << "Creating Socket is successful!\n";

    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(Server_Port);
    ServerAddr.sin_addr.S_un.S_addr = inet_addr(ServerIP.c_str());
    if (bind(ServerSocket, (SOCKADDR *)&ServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        cout << "Error in Binding Socket!\n";
        exit(EXIT_FAILURE);
    }
    cout << "Binding Socket to port " << Server_Port << " is successful!\n\n";

    RouterAddr.sin_family = AF_INET;
    RouterAddr.sin_port = htons(Router_Port);
    RouterAddr.sin_addr.S_un.S_addr = inet_addr(RouterIP.c_str());
}
void Connect()
{
    Message con_msg[3];
    while (true)
    {
        // * First-Way Handshake
        if (recvfrom(ServerSocket, (char *)&con_msg[0], sizeof(con_msg[0]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            con_msg[0].Print_Message();
            if (!(con_msg[0].Is_SYN() && con_msg[0].CheckValid() && con_msg[0].Seq == Seq + 1))
            {
                cout << "Error Message!" << endl;
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
                con_msg[1].Print_Message();
                // * Third-Way Handshake
                while (true)
                {
                    if (recvfrom(ServerSocket, (char *)&con_msg[2], sizeof(con_msg[2]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
                    {
                        con_msg[2].Print_Message();
                        if (!(con_msg[2].Is_ACK() && con_msg[2].CheckValid() && con_msg[2].Seq == Seq + 1 && con_msg[2].Ack == con_msg[1].Seq))
                        {
                            cout << "Error Message!" << endl;
                            exit(EXIT_FAILURE);
                        }
                        Seq = con_msg[2].Seq;
                        cout << "Third-Way Handshake is successful!" << endl;
                        return;
                    }
                    if ((clock() - msg2_Send_Time) > Wait_Time)
                    {
                        int re = Send(con_msg[1]);
                        msg2_Send_Time = clock();
                        if (re > 0)
                        {
                            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                            cout << "!Time Out! -- Send Message to Router! -- Second-Way Handshake" << endl;
                            con_msg[1].Print_Message();
                            SetConsoleTextAttribute(hConsole, 7);
                        }
                    }
                }
            }
        }
    }
}
void Receive_Message()
{
    Header_Seq = Seq;
    char file_name[MSS] = {};
    Message rec_msg;
    int Waiting_Seq = Header_Seq + 1;
    while (true)
    {
        if (recvfrom(ServerSocket, (char *)&rec_msg, sizeof(rec_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen))
        {
            if (rec_msg.Seq == 0)
            {
                Message reply_msg;
                reply_msg.Ack = Waiting_Seq - 1;
                reply_msg.Set_ACK();
                for (int j = 0; j < 3; j++)
                {
                    if (Send(reply_msg))
                    {
                        SetConsoleTextAttribute(hConsole, 12);
                        cout << "Zero Seq! Trying to Get New Message! Waiting_Seq = " << Waiting_Seq << endl;
                        SetConsoleTextAttribute(hConsole, 7);
                    }
                }
            }
            else
            {
                if (rec_msg.Seq < Waiting_Seq)
                {
                    Sleep_Time++;
                    continue;
                }
                else
                {
                    rec_msg.Print_Message();
                    if (rec_msg.Is_CFH() && rec_msg.CheckValid() && rec_msg.Seq == Waiting_Seq)
                    {
                        file_length = rec_msg.Length;
                        strcpy(file_name, rec_msg.Data);
                        cout << "Receive File Name: " << file_name << " File Size: " << file_length << endl;
                        Message reply_msg;
                        reply_msg.Ack = rec_msg.Seq;
                        reply_msg.Set_ACK();
                        if (Send(reply_msg) > 0)
                        {
                            Waiting_Seq++;
                            break;
                        }
                    }
                    else if (rec_msg.Is_CFH() && rec_msg.CheckValid() && rec_msg.Seq != Waiting_Seq)
                    {
                        Message reply_msg;
                        reply_msg.Ack = Waiting_Seq - 1;
                        reply_msg.Set_ACK();
                        if (Send(reply_msg))
                        {
                            SetConsoleTextAttribute(hConsole, 12);
                            cout << "Receive_Seq = " << rec_msg.Seq << " Waiting_Seq = " << Waiting_Seq << endl;
                            SetConsoleTextAttribute(hConsole, 7);
                        }
                    }
                    else
                    {
                        cout << "Error Message!" << endl;
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }
    }
    ofstream Recv_File(file_name, ios::binary);
    int complete_num = file_length / MSS;
    int last_length = file_length % MSS;

    if (last_length != 0)
    {
        Msg_Num = complete_num + 1;
    }
    else
    {
        Msg_Num = complete_num;
    }
    cout << "Start to Receive File!" << endl;

    Message *data_msg = new Message[Msg_Num];
    Last_Time = clock();
    for (int i = 0; i < Msg_Num; i++)
    {
        while (true)
        {
            Message tmp_msg;
            if (clock() - Last_Time > Wait_Time)
            {
                Message reply_msg;
                reply_msg.Ack = Waiting_Seq - 1;
                reply_msg.Set_ACK();
                for (int j = 0; j < 3; j++)
                {
                    if (Send(reply_msg))
                    {
                        SetConsoleTextAttribute(hConsole, 12);
                        cout << "Time Out! Trying to Get New Message! Waiting_Seq = " << Waiting_Seq << endl;
                        SetConsoleTextAttribute(hConsole, 7);
                    }
                }
            }
            if (recvfrom(ServerSocket, (char *)&tmp_msg, sizeof(tmp_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen))
            {
                Last_Time = clock();
                if (tmp_msg.Seq < Waiting_Seq)
                {
                    Sleep_Time++;
                    continue;
                }
                else
                {
                    tmp_msg.Print_Message();
                    if (tmp_msg.CheckValid() && tmp_msg.Seq == Waiting_Seq)
                    {
                        Message reply_msg;
                        reply_msg.Ack = tmp_msg.Seq;
                        reply_msg.Set_ACK();
                        if (Send(reply_msg))
                        {
                            reply_msg.Print_Message();
                            data_msg[i] = tmp_msg;
                            cout << "Receive_Seq = " << tmp_msg.Seq << " Waiting_Seq = " << Waiting_Seq << endl;
                            Waiting_Seq++;
                            break;
                        }
                        else
                        {
                            cout << "Error in Sending Message!";
                            exit(EXIT_FAILURE);
                        }
                    }
                    else if (tmp_msg.CheckValid() && tmp_msg.Seq != Waiting_Seq)
                    {
                        Message reply_msg;
                        reply_msg.Ack = Waiting_Seq - 1;
                        reply_msg.Set_ACK();
                        if (Send(reply_msg))
                        {
                            SetConsoleTextAttribute(hConsole, 12);
                            cout << "Receive_Seq = " << tmp_msg.Seq << " Waiting_Seq = " << Waiting_Seq << endl;
                            SetConsoleTextAttribute(hConsole, 7);
                        }
                    }
                    else if (Sleep_Time == 10)
                    {
                        Message reply_msg;
                        reply_msg.Ack = Waiting_Seq - 1;
                        reply_msg.Set_ACK();
                        for (int j = 0; j < 3; j++)
                        {
                            if (Send(reply_msg))
                            {
                                SetConsoleTextAttribute(hConsole, 12);
                                cout << "Time Out! Trying to Get New Message! Waiting_Seq = " << Waiting_Seq << endl;
                                SetConsoleTextAttribute(hConsole, 7);
                            }
                        }
                        Sleep_Time = 0;
                    }
                }
            }
            else
            {
                cout << "Error in Receiving Message!" << endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    for (int i = 0; i < Msg_Num - 1; i++)
    {
        Recv_File.write(data_msg[i].Data, MSS);
    }
    if (last_length)
    {
        Recv_File.write(data_msg[Msg_Num - 1].Data, last_length);
    }
    SetConsoleTextAttribute(hConsole, 10);
    cout << "Finish Receiving!" << endl;
    SetConsoleTextAttribute(hConsole, 7);
    Seq = data_msg[Msg_Num - 1].Seq;
}
void Disconnect() // * Router端主动断开连接
{
    Message discon_msg[4];
    while (true)
    {
        // * First-Way Wavehand
        if (recvfrom(ServerSocket, (char *)&discon_msg[0], sizeof(discon_msg[0]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            discon_msg[0].Print_Message();
            if (!(discon_msg[0].Is_FIN() && discon_msg[0].CheckValid()))
            {
                cout << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Seq = discon_msg[0].Seq;
        }
        // * Second-Way Wavehand
        discon_msg[1].Ack = discon_msg[0].Seq;
        discon_msg[1].Seq = ++Seq;
        discon_msg[1].Set_ACK();
        if (Send(discon_msg[1]))
        {
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
    if (re)
    {
        discon_msg[2].Print_Message();
    }
    // * Fourth-Way Wavehand
    while (true)
    {
        if (recvfrom(ServerSocket, (char *)&discon_msg[3], sizeof(discon_msg[3]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            discon_msg[3].Print_Message();
            if (discon_msg[3].Seq < Seq + 1)
            {
                continue;
            }
            else if (!(discon_msg[3].Is_ACK() && discon_msg[3].CheckValid() && discon_msg[3].Seq == Seq + 1 && discon_msg[3].Ack == discon_msg[2].Seq))
            {
                cout << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Seq = discon_msg[3].Seq;
            cout << "Fourth-Way Wavehand is successful!\n\n";
            break;
        }
        if ((clock() - dismsg3_Send_Time) > Wait_Time)
        {
            int re = Send(discon_msg[2]);
            dismsg3_Send_Time = clock();
            if (re)
            {
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                cout << "!Time Out! -- Send Message to Router! -- Third-Way Wavehand" << endl;
                discon_msg[2].Print_Message();
                SetConsoleTextAttribute(hConsole, 7);
            }
        }
    }
    Exit();
}
void Exit()
{
    closesocket(ServerSocket);
    WSACleanup();
    cout << "Server is closed!" << endl;
}
