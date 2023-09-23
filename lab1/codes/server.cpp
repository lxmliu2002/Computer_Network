#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <WinSock2.h>
#include <ws2tcpip.h>
//#include <ctime>  // 添加时间库

#pragma comment(lib,"ws2_32.lib")   //socket库

using namespace std;

#define PORT 8000  //端口号
#define BufSize 1024  //缓冲区大小
#define MaxClient 5 //最大连接数 = MaxClient - 1
#define _CRT_SECURE_NO_WARNINGS //禁止使用不安全的函数报错
#define _WINSOCK_DEPRECATED_NO_WARNINGS //禁止使用旧版本的函数报错

SOCKET clientSockets[MaxClient];//客户端socket数组
SOCKET serverSocket;//服务器端socket
SOCKADDR_IN clientAddrs[MaxClient];//客户端地址数组
SOCKADDR_IN serverAddr;//定义服务器地址

int current_connect_count = 0; //当前连接的客户数
int condition[MaxClient] = {};//每一个连接的情况

int check()//查询空闲的连接口的索引
{
    for (int i = 0; i < MaxClient; i++)
    {
        if (condition[i] == 0)//连接空闲
        {
            return i;
        }
    }
    exit(EXIT_FAILURE);
}

DWORD WINAPI ThreadFunction(LPVOID lpParameter)//线程函数
{
    int receByt = 0; //接收到的字节数
    char RecvBuf[BufSize]; //接收缓冲区
    char SendBuf[BufSize]; //发送缓冲区
    //char exitBuf[5];
    //SOCKET sock = *((SOCKET*)lpParameter);
    
    //循环接收信息
    while (true)
    {
        int num = (int)lpParameter; //当前连接的索引
        Sleep(100); //延时100ms
        //receByt = recv(sock, RecvBuf, sizeof(RecvBuf), 0);
        receByt = recv(clientSockets[num], RecvBuf, sizeof(RecvBuf), 0); //接收信息
        if (receByt > 0) //接收成功
        {
            //for (int i = 0; i < 5; i++)
            //{
            //    exitBuf[i] = RecvBuf[i];
            //}
            //if (strcmp(exitBuf, "exit"))
            //{
            //    auto currentTime = chrono::system_clock::now();
            //    time_t timestamp = chrono::system_clock::to_time_t(currentTime);
            //    tm localTime;
            //    localtime_s(&localTime, &timestamp);
            //    char timeStr[50];
            //    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d--%H:%M:%S", &localTime); // 格式化时间
            //    cout << "Client " << clientSockets[num] << " exit! Time: " << timeStr << endl;
            //    closesocket(clientSockets[num]);
            //    current_connect_count--;
            //    condition[num] = 0;
            //    send(clientSockets[num], "Your server has been closed!", sizeof(SendBuf), 0);
            //    return 0;
            //}
            //创建时间戳，记录当前通讯时间
            auto currentTime = chrono::system_clock::now();
            time_t timestamp = chrono::system_clock::to_time_t(currentTime);
            tm localTime;
            localtime_s(&localTime, &timestamp);
            char timeStr[50];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d--%H:%M:%S", &localTime); // 格式化时间
            cout << "Client " << clientSockets[num] << " : " << RecvBuf << " --" << timeStr << endl;
            sprintf_s(SendBuf, sizeof(SendBuf), "%d: %s --%s ", clientSockets[num], RecvBuf, timeStr); // 格式化发送信息
            for (int i = 0; i < MaxClient; i++)//将消息同步到所有聊天窗口
            {
                if (condition[i] == 1)
                {
                    send(clientSockets[i], SendBuf, sizeof(SendBuf), 0);//发送信息
                }
            }
        }
        else //接收失败
        {
            if (WSAGetLastError() == 10054)//客户端主动关闭连接
            {
                //创建时间戳，记录当前通讯时间
                auto currentTime = chrono::system_clock::now();
                time_t timestamp = chrono::system_clock::to_time_t(currentTime);
                tm localTime;
                localtime_s(&localTime, &timestamp);
                char timeStr[50];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d--%H:%M:%S", &localTime); // 格式化时间
                cout << "Client " << clientSockets[num] << " exit! Time: " << timeStr << endl;
                closesocket(clientSockets[num]);
                current_connect_count--;
                condition[num] = 0;
                cout << "current_connect_count: " << current_connect_count << endl;
                return 0;
            }
            else
            {
                cout << "Failed to receive, Error:" << WSAGetLastError() << endl;
                break;
            }
        }
    }
}


int main()
{
//    system("chcp 936");//防止乱码
    //初始化DLL
    WSAData wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData); //MAKEWORD（主版本号，副版本号）
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)//错误处理 如果初始化成功，wVersion的低位为2，高位为2，存储为0x0202
    {
        perror("Error in Initializing Socket DLL!\n");
        exit(EXIT_FAILURE);
    }
    cout << "Initializeing Socket DLL is successful!\n" << endl;

    //创建服务器端套接字
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//IPv4地址族，流式套接字，TCP协议
    if (serverSocket == INVALID_SOCKET)//错误处理
    {
        perror("Error in Creating Socket!\n");
        exit(EXIT_FAILURE);
    }
    cout << "Creating Socket is successful!\n" << endl;

    //绑定服务器地址
    serverAddr.sin_family = AF_INET;//地址类型
    serverAddr.sin_port = htons(PORT);//端口号
	if (inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)) != 1) {
		cout << "Error in Inet_pton" << endl;
		exit(EXIT_FAILURE);
	}
//	serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    if (bind(serverSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)//将服务器套接字与服务器地址和端口绑定
    {
        perror("Binding is failed!\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        cout << "Binding to port " << PORT << " is successful!\n" << endl;
    }

    //设置监听
    if (listen(serverSocket, MaxClient) != 0)
    {
        perror("Listening is failed! \n");
        exit(EXIT_FAILURE);
    }
    else
    {
        cout << "Listening is successful! \n" << endl;
    }

    cout << "The Server is ready! Waiting for Client request...\n\n" << endl;

    cout << "current_connect_count: " << current_connect_count << endl;

    //循环接收客户端请求
    while (true)
    {
        if (current_connect_count < MaxClient)
        {
            int num = check();
            int addrlen = sizeof(SOCKADDR);
            clientSockets[num] = accept(serverSocket, (sockaddr*)&clientAddrs[num], &addrlen);//接收客户端请求
            if (clientSockets[num] == SOCKET_ERROR)//错误处理
            {
                perror("The Client is failed! \n");
                closesocket(serverSocket);
                WSACleanup();
                exit(EXIT_FAILURE);
            }
            condition[num] = 1;//连接位 置1表示占用
            current_connect_count++; //当前连接数加1
            //创建时间戳，记录当前通讯时间
            auto currentTime = chrono::system_clock::now();
            time_t timestamp = chrono::system_clock::to_time_t(currentTime);
            tm localTime;
            localtime_s(&localTime, &timestamp);
            char timeStr[50];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d--%H:%M:%S", &localTime); // 格式化时间

            cout << "The Client " << clientSockets[num] << " is connected. Time is " << timeStr << endl;
            cout << "current_connect_count: " << current_connect_count << endl;
            HANDLE Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadFunction, (LPVOID)num, 0, NULL);//创建线程
            //HANDLE Thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadFunction, (LPVOID) & (clientSockets[num]), 0, NULL);
            if (Thread == NULL)//线程创建失败
            {
                perror("The Thread is failed!\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                CloseHandle(Thread);
            }
        }
        else
        {
            cout << "Fulling..." << endl;
        }
    }

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}

