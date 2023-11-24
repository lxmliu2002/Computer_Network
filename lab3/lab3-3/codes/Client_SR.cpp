#include "Defs_SR.h"

#define Router_Port 65432
#define Client_Port 54321

WSADATA wsaData;

int Seq = 0;
atomic_int Base_Seq(1);
atomic_int Next_Seq(1);
atomic_int Header_Seq(0);
atomic_int Count(0);
int Msg_Num = 0;
atomic_bool Re_Send(false);
atomic_bool Finish(false);
mutex mtx;
atomic_int *Send_Time;
atomic_bool *Re_Send_Single;

int Send(Message &msg);
void Client_Initial();
void Connect();
void Receive_Ack(bool *Rec_Ack);
void Send_Message(string file_path);
void Disconnect();
void Wait_Exit();
void Print_Window(int base_seq, int next_seq);

// ofstream time_txt("time.txt");

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        ClientIP = argv[1];
        RouterIP = argv[2];
    }

    Client_Initial();
    cout << "Client is ready! Trying to connection" << endl;
    Connect();

    while (true)
    {
        int select = 0;
        cout << "Please select the function you want to use:" << endl;
        cout << "1. Send File" << endl;
        cout << "2. Exit" << endl;
        cout << "> ";
        cin >> select;
        string file_path;
        switch (select)
        {
        case 1:
            cout << "Please input the file path:" << endl;
            cout << "> ";
            cin >> file_path;
            Send_Message(file_path);
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
    msg.SrcPort = Client_Port;
    msg.DstPort = Router_Port;
    msg.Set_Check();
    return sendto(ClientSocket, (char *)&msg, sizeof(msg), 0, (SOCKADDR *)&RouterAddr, RouterAddrLen);
}
void Client_Initial()
{
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        perror("[Client] Error in Initializing Socket DLL!\n");
        exit(EXIT_FAILURE);
    }
    cout << "Initializing Socket DLL is successful!\n";

    ClientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    unsigned long on = 1;
    ioctlsocket(ClientSocket, FIONBIO, &on);
    if (ClientSocket == INVALID_SOCKET)
    {
        cout << "Error in Creating Socket!\n";
        exit(EXIT_FAILURE);
    }
    cout << "Creating Socket is successful!\n";

    ClientAddr.sin_family = AF_INET;
    ClientAddr.sin_port = htons(Client_Port);
    ClientAddr.sin_addr.S_un.S_addr = inet_addr(ClientIP.c_str());

    if (bind(ClientSocket, (SOCKADDR *)&ClientAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        cout << "Error in Binding Socket!\n";
        exit(EXIT_FAILURE);
    }
    cout << "Binding Socket to port " << Client_Port << " is successful!\n\n";

    RouterAddr.sin_family = AF_INET;
    RouterAddr.sin_port = htons(Router_Port);
    RouterAddr.sin_addr.S_un.S_addr = inet_addr(RouterIP.c_str());
}
void Connect()
{
    Message con_msg[3];
    // * First-Way Handshake
    con_msg[0].Seq = ++Seq;
    con_msg[0].Set_SYN();

    int re = Send(con_msg[0]);
    float msg1_Send_Time = clock();
    if (re > 0)
    {
        con_msg[0].Print_Message();
        // * Second-Way Handshake
        while (true)
        {
            if (recvfrom(ClientSocket, (char *)&con_msg[1], sizeof(con_msg[1]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
            {
                con_msg[1].Print_Message();
                if (!(con_msg[1].Is_ACK() && con_msg[1].Is_SYN() && con_msg[1].CheckValid() && con_msg[1].Ack == con_msg[0].Seq))
                {
                    cout << "Error Message!" << endl;
                    exit(EXIT_FAILURE);
                }
                Seq = con_msg[1].Seq;
                break;
            }
            if ((clock() - msg1_Send_Time) > Wait_Time)
            {
                int re = Send(con_msg[0]);
                msg1_Send_Time = clock();
                if (re)
                {
                    SetConsoleTextAttribute(hConsole, 12);
                    cout << "!Time Out! -- Send Message to Router! -- First-Way Handshake" << endl;
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
    if (Send(con_msg[2]))
    {
        con_msg[2].Print_Message();
    }
    cout << "Third-Way Handshake is successful!\n\n";
}
void Receive_Ack(bool *Rec_Ack)
{
    int Err_Ack_Num = 0;
    while (true)
    {
        Message ack_msg;
        if (recvfrom(ClientSocket, (char *)&ack_msg, sizeof(ack_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen))
        {
            if (ack_msg.Is_ACK() && ack_msg.CheckValid())
            {
                lock_guard<mutex> lock(mtx);
                ack_msg.Print_Message();
                cout << "Receive Ack = " << ack_msg.Ack << endl;
                Rec_Ack[ack_msg.Ack - Header_Seq - 1] = true;
                int tmp = Base_Seq - 1;
                for (int j = tmp; j < tmp + Windows_Size && Rec_Ack[j]; j++)
                {
                    Base_Seq++;
                }
                Print_Window(Base_Seq + Header_Seq, Next_Seq + Header_Seq);
                int Is_Finish = 0;
                for (int j = 0; j < Msg_Num + 1; j++)
                {
                    if (Rec_Ack[j])
                    {
                        Is_Finish++;
                    }
                    else
                    {
                        break;
                    }
                }
                if (Is_Finish == Msg_Num + 1)
                {
                    Finish = true;
                    return;
                }
                if (Err_Ack_Num != ack_msg.Ack)
                {
                    Err_Ack_Num = ack_msg.Ack;
                    Count = 0;
                }
                else
                {
                    Count++;
                    if (Count == 3)
                    {
                        Re_Send = true;
                        Count = 0;
                    }
                }
            }
        }
        for (int i = 0; i < Next_Seq - Base_Seq; i++)
        {
            if (clock() - Send_Time[Base_Seq + i - 1] > Wait_Time)
            {
                Re_Send_Single[Base_Seq + i - 1] = true;
            }
        }
    }
    return;
}

void Send_Message(string file_path)
{
    size_t found = file_path.find_last_of("/\\");
    string file_name = file_path.substr(found + 1);

    ifstream file(file_path, ios::binary);
    if (!file.is_open())
    {
        cout << "Error in Opening File!" << endl;
        exit(EXIT_FAILURE);
    }
    file.seekg(0, ios::end);
    file_length = file.tellg();
    file.seekg(0, ios::beg);
    if (file_length > pow(2, 32))
    {
        cout << "File is too large!" << endl;
        exit(EXIT_FAILURE);
    }

    int complete_num = file_length / MSS;
    int last_length = file_length % MSS;
    Header_Seq = Seq;
    cout << "Header_Seq = " << Header_Seq << endl;
    if (last_length != 0)
    {
        Msg_Num = complete_num + 1;
    }
    else
    {
        Msg_Num = complete_num;
    }
    bool *Rec_Ack = new bool[Msg_Num + 1];
    memset(Rec_Ack, 0, Msg_Num + 1);
    Send_Time = new atomic_int[Msg_Num + 1];
    memset(Send_Time, 0, Msg_Num + 1);
    Re_Send_Single = new atomic_bool[Msg_Num + 1];
    memset(Re_Send_Single, 0, Msg_Num + 1);
    thread Receive_Ack_Thread(Receive_Ack, Rec_Ack);
    Message *data_msg = new Message[Msg_Num + 1];
    float complete_time = clock();
    while (!Finish)
    {
        for (int i = 0; (i < Next_Seq - Base_Seq) && Re_Send_Single[Base_Seq + i - 1]; i++)
        {
            lock_guard<mutex> lock(mtx);
            if (Send(data_msg[Base_Seq + i - 1]))
            {
                Send_Time[Base_Seq + i - 1] = clock();
                SetConsoleTextAttribute(hConsole, 12);
                cout << "!Re_Send_Single! -- Send Message to Router!" << endl;
                SetConsoleTextAttribute(hConsole, 7);
                data_msg[Base_Seq + i - 1].Print_Message();
            }
            else
            {
                cout << "Error in Sending Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Re_Send_Single[Base_Seq + i - 1] = false;
        }
        if (Re_Send)
        {
            for (int i = 0; (i < Next_Seq - Base_Seq) && (!Rec_Ack[Base_Seq + i - 1]); i++)
            {
                lock_guard<mutex> lock(mtx);
                if (Send(data_msg[Base_Seq + i - 1]))
                {
                    Send_Time[Base_Seq + i - 1] = clock();
                    SetConsoleTextAttribute(hConsole, 12);
                    cout << "!Re_Send! -- Send Message to Router!" << endl;
                    SetConsoleTextAttribute(hConsole, 7);
                    data_msg[Base_Seq + i - 1].Print_Message();
                }
                else
                {
                    cout << "Error in Sending Message!" << endl;
                    exit(EXIT_FAILURE);
                }
            }
            Re_Send = false;
        }
        if (Next_Seq < Base_Seq + Windows_Size && Next_Seq - 1 <= Msg_Num)
        {
            if (Next_Seq == 1)
            {
                lock_guard<mutex> lock(mtx);
                strcpy(data_msg[Next_Seq - 1].Data, file_name.c_str());
                data_msg[Next_Seq - 1].Data[strlen(data_msg[Next_Seq - 1].Data)] = '\0';
                data_msg[Next_Seq - 1].Length = file_length;
                data_msg[Next_Seq - 1].Seq = ++Seq;
                data_msg[Next_Seq - 1].Set_CFH();
            }
            else if (Next_Seq - 1 == Msg_Num && last_length)
            {
                lock_guard<mutex> lock(mtx);
                file.read(data_msg[Next_Seq - 1].Data, last_length);
                data_msg[Next_Seq - 1].Length = last_length;
                data_msg[Next_Seq - 1].Seq = ++Seq;
            }
            else
            {
                lock_guard<mutex> lock(mtx);
                file.read(data_msg[Next_Seq - 1].Data, MSS);
                data_msg[Next_Seq - 1].Length = MSS;
                data_msg[Next_Seq - 1].Seq = ++Seq;
            }
            if (Next_Seq % 17 == 0)
            {
                Next_Seq++;
                continue;
            }
            if (Next_Seq % 19 == 0)
            {
                Sleep(10);
                Next_Seq++;
                continue;
            }
            if (Send(data_msg[Next_Seq - 1]))
            {
                Send_Time[Next_Seq - 1] = clock();
                lock_guard<mutex> lock(mtx);
                data_msg[Next_Seq - 1].Print_Message();
                Next_Seq++;
            }
            else
            {
                lock_guard<mutex> lock(mtx);
                cout << "Error in Sending Message!" << endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    Receive_Ack_Thread.join();

    float send_time = clock() - complete_time;
    file.close();
    SetConsoleTextAttribute(hConsole, 10);
    cout << "Finish Sending File!" << endl;
    cout << "Send Time: " << send_time << " ms" << endl;
    cout << "Send Speed: " << file_length / send_time << " Byte/ms\n\n";
    SetConsoleTextAttribute(hConsole, 7);
    lock_guard<mutex> lock(mtx);
    Next_Seq = 1;
    Base_Seq = 1;
    Finish = false;
    Re_Send = false;
    Header_Seq = 0;
    Msg_Num = 0;
    return;
}
void Disconnect() // * Client端主动断开连接
{
    Message discon_msg[4];

    // * First-Way Wavehand
    discon_msg[0].Seq = ++Seq;
    discon_msg[0].Set_FIN();
    int re = Send(discon_msg[0]);
    float dismsg0_Send_Time = clock();
    if (re)
    {
        discon_msg[0].Print_Message();
    }
    // * Second-Way Wavehand
    while (true)
    {
        if (recvfrom(ClientSocket, (char *)&discon_msg[1], sizeof(discon_msg[1]), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen) > 0)
        {
            discon_msg[1].Print_Message();
            if (discon_msg[1].Seq < Seq + 1)
            {
                continue;
            }
            else if (!(discon_msg[1].Is_ACK() && discon_msg[1].CheckValid() && discon_msg[1].Seq == Seq + 1 && discon_msg[1].Ack == discon_msg[0].Seq))
            {
                cout << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Seq = discon_msg[1].Seq;
            break;
        }
        if ((clock() - dismsg0_Send_Time) > Wait_Time)
        {
            SetConsoleTextAttribute(hConsole, 12);
            cout << "!Time Out! -- First-Way Wavehand" << endl;
            int re = Send(discon_msg[0]);
            dismsg0_Send_Time = clock();
            if (re > 0)
            {
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
            discon_msg[2].Print_Message();
            if (!(discon_msg[2].Is_ACK() && discon_msg[2].Is_FIN() && discon_msg[2].CheckValid() && discon_msg[2].Seq == Seq + 1 && discon_msg[2].Ack == discon_msg[1].Seq))
            {
                cout << "Error Message!" << endl;
                exit(EXIT_FAILURE);
            }
            Seq = discon_msg[2].Seq;
            break;
        }
    }
    // * Fourth-Way Wavehand
    discon_msg[3].Ack = discon_msg[2].Seq;
    discon_msg[3].Set_ACK();
    discon_msg[3].Seq = ++Seq;
    if (Send(discon_msg[3]))
    {
        discon_msg[3].Print_Message();
    }
    cout << "Fourth-Way Wavehand is successful!" << endl;

    Wait_Exit();
}
void Wait_Exit()
{
    Message exit_msg;
    float exit_msg_time = clock();
    while (clock() - exit_msg_time < 2 * Wait_Time)
    {
        if (recvfrom(ClientSocket, (char *)&exit_msg, sizeof(exit_msg), 0, (SOCKADDR *)&RouterAddr, &RouterAddrLen))
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
    cout << "Client is closed!" << endl;
}
void Print_Window(int base_seq, int next_seq)
{
    cout << "[ Window ] Base: " << base_seq << " Next: " << next_seq << " No_Verify_Num: " << next_seq - base_seq << " Residue: " << Windows_Size - next_seq + base_seq << endl;
}