#include "Defs.h"

// #define Router_Port 65432
// // #define Server_Port 65432
// #define Client_Port 54321

WSADATA wsaData;

// char *file_name;

int Seq = 0;

int Send(Message &msg);
bool Client_Initial();
bool Connect();
void Send_Message(string file_path);
void Disconnect();
void Wait_Exit();

ofstream time_txt("time.txt");

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        ClientIP = argv[1];
        RouterIP = argv[2];
    }

    if (!Client_Initial())
    {
        cout << "[Client] "
             << "Error in Initializing Client!" << endl;
        exit(EXIT_FAILURE);
    }
    cout << "[Client] "
         << "Client is ready! Trying to connection" << endl;

    if (!Connect())
    {
        cout << "[Client] "
             << "Error in Connecting!" << endl;
        exit(EXIT_FAILURE);
    }
    while (true)
    {
        int select = 0;
        cout << "[Client] "
             << "Please select the function you want to use:" << endl;
        cout << "[Client] "
             << "1. Send File" << endl;
        cout << "[Client] "
             << "2. Exit" << endl;
        cin >> select;
        string file_path;
        switch (select)
        {
        case 1:
            cout << "[Client] "
                 << "Please input the file path:" << endl;
            cin >> file_path;
            Send_Message(file_path);
            break;
        case 2:
            Disconnect();
            return 0;
        default:
            cout << "[Client] "
                 << "Error in Selecting!" << endl;
            exit(EXIT_FAILURE);
        }
    }
    system("pause");
    return 0;
}

int Send(Message &msg)
{
    msg.SrcPort = Client_Port;
    msg.DstPort = Router_Port;
    msg.Set_Check();
    return sendto(ClientSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
bool Client_Initial()
{
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        perror("[Client] Error in Initializing Socket DLL!\n");
        exit(EXIT_FAILURE);
    }
    cout << "[Client] "
         << "Initializing Socket DLL is successful!\n";

    ClientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    unsigned long on = 1;
    ioctlsocket(ClientSocket, FIONBIO, &on);
    if (ClientSocket == INVALID_SOCKET)
    {
        cout << "[Client] "
             << "Error in Creating Socket!\n";
        exit(EXIT_FAILURE);
        return false;
    }
    cout << "[Client] "
         << "Creating Socket is successful!\n";

    ClientAddr.sin_family = AF_INET;
    ClientAddr.sin_port = htons(Client_Port);
    ClientAddr.sin_addr.S_un.S_addr = inet_addr(ClientIP.c_str());

    if (bind(ClientSocket, (SOCKADDR *)&ClientAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        cout << "[Client] "
             << "Error in Binding Socket!\n";
        exit(EXIT_FAILURE);
        return false;
    }
    cout << "[Client] "
         << "Binding Socket to port " << Client_Port << " is successful!\n\n";

    RouterAddr.sin_family = AF_INET;
    RouterAddr.sin_port = htons(Router_Port);
    RouterAddr.sin_addr.S_un.S_addr = inet_addr(RouterIP.c_str());
    return true;
}
bool Connect()
{
    Message con_msg[3];
    // * First-Way Handshake
    con_msg[0].Seq = ++Seq;
    con_msg[0].Set_SYN();

    int re = Send(con_msg[0]);
    float msg1_Send_Time = clock();
    if (re > 0)
    {
        // cout <<"[Client] "<< "Send Message to Router! -- First-Way Handshake" << endl;
        con_msg[0].Print_Message();
        // * Second-Way Handshake
        while (true)
        {
            if (recvfrom(ClientSocket, (char *)&con_msg[1], sizeof(con_msg[1]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
            {
                // cout <<"[Client] "<< "Receive Message from Router! -- Second-Way Handshake" << endl;
                con_msg[1].Print_Message();
                if (!(con_msg[1].Is_ACK() && con_msg[1].Is_SYN() && con_msg[1].CheckValid() && con_msg[1].Ack == con_msg[0].Seq))
                {
                    cout << "[Client] "
                         << "Error Message!" << endl;
                    exit(EXIT_FAILURE);
                }
                Seq = con_msg[1].Seq;
                break;
            }
            if ((clock() - msg1_Send_Time) > Wait_Time)
            {
                int re = Send(con_msg[0]);
                msg1_Send_Time = clock();
                if (re > 0)
                {
                    SetConsoleTextAttribute(hConsole, 12);
                    cout << "[Client] "
                         << "Time Out! -- Send Message to Router! -- First-Way Handshake" << endl;
                    con_msg[0].Print_Message();
                    SetConsoleTextAttribute(hConsole, 7);
                }
            }
        }
    }
    // * Third-Way Handshake
    con_msg[2].Ack = con_msg[1].Seq;
    con_msg[2].Seq = ++Seq;
    con_msg[2].Set_ACK();
    re = Send(con_msg[2]);
    if (re > 0)
    {
        // cout <<"[Client] "<< "Send Message to Router! -- Third-Way Handshake" << endl;
        con_msg[2].Print_Message();
    }
    cout << "[Client] "
         << "Third-Way Handshake is successful!" << endl
         << endl;
    return true;
}
void Send_Message(string file_path)
{
    // strcpy(file_name, "");
    size_t found = file_path.find_last_of("/\\");
    string file_name = file_path.substr(found + 1);

    ifstream file(file_path, ios::binary);
    if (!file.is_open())
    {
        cout << "[Client] "
             << "Error in Opening File!" << endl;
        exit(EXIT_FAILURE);
    }
    // char *file_buffer = new char[File_Size];
    file.seekg(0, ios::end);
    file_length = file.tellg();
    file.seekg(0, ios::beg);
    // cout<<"[Client] "<< "Finish Reading File!" << endl;
    if (file_length > pow(2, 32))
    {
        cout << "[Client] "
             << "File is too large!" << endl;
        exit(EXIT_FAILURE);
    }

    Message send_msg;
    strcpy(send_msg.Data, file_name.c_str());
    send_msg.Data[strlen(send_msg.Data)] = '\0';
    send_msg.Length = file_length;
    send_msg.Seq = ++Seq;
    send_msg.Set_CFH();
    float last_time;
    int re = Send(send_msg);
    float msg1_Send_Time = clock();
    if (re > 0)
    {
        cout << "[Client] "
             << "Send Message to Router! -- File Header" << endl;
        send_msg.Print_Message();
    }

    while (true)
    {
        Message tmp;
        if (recvfrom(ClientSocket, (char *)&tmp, sizeof(tmp), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            cout << "[Client] "
                 << "Receive Message from Router! -- File Header" << endl;
            tmp.Print_Message();
            if (tmp.Is_ACK() && tmp.CheckValid() && tmp.Seq == Seq + 1)
            {
                Seq = tmp.Seq;
                last_time = clock() - msg1_Send_Time;
                // cout <<"[Client] "<< "Receive ACK!" << endl;
                break;
            }
            else if (tmp.CheckValid() && tmp.Seq != Seq + 1)
            {
                Message reply_msg;
                reply_msg.Ack = tmp.Seq;
                reply_msg.Set_ACK();
                // reply_msg.Seq = ++Seq;
                if (Send(reply_msg) > 0)
                {
                    cout << "!Repeatedly! [Client]"
                         << "Receive Seq = " << tmp.Seq << " Reply Ack = " << reply_msg.Ack << endl;
                }
            }
        }
        else if (clock() - msg1_Send_Time > Wait_Time)
        {
            int re = sendto(ClientSocket, (char *)&send_msg, sizeof(send_msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
            msg1_Send_Time = clock();
            if (re > 0)
            {
                SetConsoleTextAttribute(hConsole, 12);
                cout << "[Client] "
                     << "Time Out! -- Send Message to Router! -- File Header" << endl;
                send_msg.Print_Message();
                SetConsoleTextAttribute(hConsole, 7);
            }
            else
            {
                cout << "[Client] "
                     << "Error in Sending Message! -- File Header" << endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    struct timeval complete_time_start, complete_time_end;
    gettimeofday(&complete_time_start, NULL);
    float complete_time = clock();
    // file.read(file_buffer, file_length);
    int complete_num = file_length / MSS;
    int last_length = file_length % MSS;
    cout << "[Client] "
         << "Start to Send Message to Router! -- File" << endl;
    for (int i = 0; i <= complete_num; i++)
    {
        Message data_msg;
        if (i != complete_num)
        {
            file.read(data_msg.Data, MSS);
            // for(int j=0;j<MSS;j++)
            // {
            //     data_msg.Data[j] = file_buffer[i * MSS + j];
            // }
            data_msg.Length = MSS;
            data_msg.Seq = ++Seq;
            int re = Send(data_msg);
            struct timeval every_time_start, every_time_end;
            long long every_time_usec;
            gettimeofday(&every_time_start, NULL);
            // float every_time_start = clock();
            float time = clock();
            if (re > 0)
            {
                // cout <<"[Client] "<< "Send Message to Router! Part " << i << "-- File" << endl;
                data_msg.Print_Message();
                Message tmp;
                while (true)
                {
                    if (recvfrom(ClientSocket, (char *)&tmp, sizeof(tmp), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
                    {
                        // cout <<"[Client] "<< "Receive Message from Router! Part " << i << "-- File" << endl;
                        tmp.Print_Message();
                        if (tmp.Is_ACK() && tmp.CheckValid() && tmp.Seq == Seq + 1)
                        {
                            Seq = tmp.Seq;
                            gettimeofday(&every_time_end, NULL);
                            every_time_usec = (every_time_end.tv_usec - every_time_start.tv_usec);
                            if (i % 1 == 0)
                            {
                                gettimeofday(&complete_time_end, NULL);
                                long long complete_time_usec = (complete_time_end.tv_usec - complete_time_start.tv_usec);
                                // float every_time = clock() - every_time_start;
                                time_txt << (complete_time_usec) << "," << (every_time_usec) << "," << ((double)(MSS * i) / (complete_time_usec)*1000) << endl;
                            }
                            // cout <<"[Client] "<< "Receive ACK!" << endl;
                            break;
                        }
                        else if (tmp.CheckValid() && tmp.Seq != Seq + 1)
                        {
                            Message reply_msg;
                            reply_msg.Ack = tmp.Seq;
                            reply_msg.Set_ACK();
                            // reply_msg.Seq = ++Seq;
                            if (Send(reply_msg) > 0)
                            {
                                cout << "!Repeatedly! [Client]"
                                     << "Receive Seq = " << tmp.Seq << " Reply Ack = " << reply_msg.Ack << endl;
                            }
                        }
                    }
                    else if (clock() - time > Wait_Time)
                    {
                        int re = sendto(ClientSocket, (char *)&data_msg, sizeof(data_msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
                        time = clock();
                        if (re > 0)
                        {
                            SetConsoleTextAttribute(hConsole, 12);
                            cout << "[Client] "
                                 << "Time Out! -- Send Message to Router! Part " << i << "-- File" << endl;
                            data_msg.Print_Message();
                            SetConsoleTextAttribute(hConsole, 7);
                        }
                        else
                        {
                            cout << "[Client] "
                                 << "Error in Sending Message! Part " << i << " -- File" << endl;
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            }
        }
        else
        {
            Message data_msg;
            file.read(data_msg.Data, last_length);
            // for(int j=0;j<last_length;j++)
            // {
            //     data_msg.Data[j] = file_buffer[complete_num * MSS + j];
            // }
            data_msg.Length = last_length;
            data_msg.Seq = ++Seq;
            int re = Send(data_msg);
            // float every_time_start = clock();
            struct timeval every_time_start, every_time_end;
            long long every_time_usec;
            gettimeofday(&every_time_start, NULL);
            float time = clock();
            if (re > 0)
            {
                // cout <<"[Client] "<< "Send Message to Router! Part " << i << "-- File" << endl;
                data_msg.Print_Message();
                Message tmp;
                while (true)
                {
                    if (recvfrom(ClientSocket, (char *)&tmp, sizeof(tmp), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
                    {
                        // cout <<"[Client] "<< "Receive Message from Router! Part " << i << "-- File" << endl;
                        tmp.Print_Message();
                        if (tmp.Is_ACK() && tmp.CheckValid() && tmp.Seq == Seq + 1)
                        {
                            Seq = tmp.Seq;
                            gettimeofday(&every_time_end, NULL);
                            every_time_usec = (every_time_end.tv_usec - every_time_start.tv_usec);
                            if (i % 1 == 0)
                            {
                                gettimeofday(&complete_time_end, NULL);
                                long long complete_time_usec = (complete_time_end.tv_usec - complete_time_start.tv_usec);
                                // float every_time = clock() - every_time_start;
                                time_txt << (complete_time_usec) << "," << (every_time_usec) << "," << ((double)(last_length) / (complete_time_usec)*1000);
                            }
                            // cout <<"[Client] "<< "Receive ACK!" << endl;
                            break;
                        }
                        else if (tmp.CheckValid() && tmp.Seq != Seq + 1)
                        {
                            Message reply_msg;
                            reply_msg.Ack = tmp.Seq;
                            reply_msg.Set_ACK();
                            // reply_msg.Seq = ++Seq;
                            if (Send(reply_msg) > 0)
                            {
                                cout << "!Repeatedly! [Client]"
                                     << "Receive Seq = " << tmp.Seq << " Reply Ack = " << reply_msg.Ack << endl;
                            }
                        }
                    }
                    else if (clock() - time > Wait_Time)
                    {
                        int re = sendto(ClientSocket, (char *)&data_msg, sizeof(data_msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
                        time = clock();
                        if (re > 0)
                        {
                            SetConsoleTextAttribute(hConsole, 12);
                            cout << "[Client] "
                                 << "Time Out! -- Send Message to Router! Part " << i << "-- File" << endl;
                            data_msg.Print_Message();
                            SetConsoleTextAttribute(hConsole, 7);
                        }
                        else
                        {
                            cout << "[Client] "
                                 << "Error in Sending Message! Part " << i << " -- File" << endl;
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            }
        }
    }
    float send_time = clock() - complete_time;
    file.close();
    cout << "[Client] "
         << "Finish Sending File!" << endl;
    cout << "[Client] "
         << "Send Time: " << send_time << " ms" << endl;
    cout << "[Client] "
         << "Send Speed: " << file_length / send_time << " Byte/ms" << endl
         << endl;
}
void Disconnect() // * Client端主动断开连接
{
    Message discon_msg[4];

    // * First-Way Wavehand
    discon_msg[0].Seq = ++Seq;
    discon_msg[0].Set_FIN();
    int re = Send(discon_msg[0]);
    float dismsg0_Send_Time = clock();
    if (re > 0)
    {
        // cout <<"[Client] "<< "Send Message to Router! -- First-Way Wavehand" << endl;
        discon_msg[0].Print_Message();
    }

    // * Second-Way Wavehand
    while (true)
    {
        if (recvfrom(ClientSocket, (char *)&discon_msg[1], sizeof(discon_msg[1]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            // cout <<"[Client] "<< "Receive Message from Router! -- Second-Way Wavehand" << endl;
            discon_msg[1].Print_Message();
            if (!(discon_msg[1].Is_ACK() && discon_msg[1].CheckValid() && discon_msg[1].Seq == Seq + 1 && discon_msg[1].Ack == discon_msg[0].Seq))
            {
                cout << "[Client] "
                     << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Seq = discon_msg[1].Seq;
            // cout <<"[Client] "<< "Second-Way Wavehand is successful!" << endl;
            break;
        }
        if ((clock() - dismsg0_Send_Time) > Wait_Time)
        {
            SetConsoleTextAttribute(hConsole, 12);
            cout << "[Client] "
                 << "Time Out! -- First-Way Wavehand" << endl;
            int re = Send(discon_msg[0]);
            dismsg0_Send_Time = clock();
            if (re > 0)
            {
                // cout <<"[Client] "<< "Send Message to Router! -- First-Way Wavehand" << endl;
                discon_msg[0].Print_Message();
                SetConsoleTextAttribute(hConsole, 7);
            }
        }
    }
    // * Third-Way Wavehand
    while (true)
    {
        if (recvfrom(ClientSocket, (char *)&discon_msg[2], sizeof(discon_msg[2]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            // cout <<"[Client] "<< "Receive Message from Router! -- Third-Way Wavehand" << endl;
            discon_msg[2].Print_Message();
            if (!(discon_msg[2].Is_ACK() && discon_msg[2].Is_FIN() && discon_msg[2].CheckValid() && discon_msg[2].Seq == Seq + 1 && discon_msg[2].Ack == discon_msg[1].Seq))
            {
                cout << "[Client] "
                     << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Seq = discon_msg[2].Seq;
            // cout <<"[Client] "<< "Third-Way Wavehand is successful!" << endl;
            break;
        }
    }
    // * Fourth-Way Wavehand
    discon_msg[3].Ack = discon_msg[2].Seq;
    discon_msg[3].Set_ACK();
    discon_msg[3].Seq = ++Seq;
    re = Send(discon_msg[3]);
    // clock dismsg3_Send_Time = clock();
    if (re > 0)
    {
        // cout <<"[Client] "<< "Send Message to Router! -- Fourth-Way Wavehand" << endl;
        discon_msg[3].Print_Message();
    }
    cout << "[Client] "
         << "Fourth-Way Wavehand is successful!" << endl;

    Wait_Exit();
    return;
}
void Wait_Exit()
{
    Message exit_msg;
    float exit_msg_time = clock();
    while (clock() - exit_msg_time < 2 * Wait_Time)
    {
        if (recvfrom(ClientSocket, (char *)&exit_msg, sizeof(exit_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            Seq = exit_msg.Seq;
            exit_msg.Ack = exit_msg.Seq;
            exit_msg.Set_ACK();
            exit_msg.Seq = ++Seq;
            Send(exit_msg);
        }
    }
    closesocket(ClientSocket);
    WSACleanup();
    cout << "[Client] "
         << "Client is closed!" << endl;
    system("pause");
}