# <center>**计算机网络实验报告**</center>

## <center>**Lab1 SOCKET编程**</center>

## <center> **网络空间安全学院 信息安全专业**</center>

## <center> **2112492 刘修铭 1063**</center>

https://github.com/lxmliu2002/Computer_Networking

# 一、协议设计

## （1）原理

### 套接字

进程通过套接字发送消息和接受消息。主要包括以下两种类型：

* 数据报套接字：使用UDP协议，支持主机之间面向非连接、不可靠的数据传输
* 流式套接字：使用TCP协议，支持主机之间面向连接的、顺序的、可靠的、全双工字节流传输

### 协议

计算机网络中各实体之间数据交换必须遵守事先约定好的规则，这些规则称为协议。以下为网络协议的组成要素：

* 语法：数据与控制信息的结构或格式（协议数据单元PDU）
* 语义：需要发出何种控制信息，完成何种动作以及做出何种响应
* 时序：事件实现顺序的详细说明

## （2）实现

按照实验要求，基于实验目标，采取了以下的协议设计方案：

* 使用TCP传输协议，选用流式套接字，采用多线程方式
* 分别设计两个程序，一个作为服务器端（server），另一个作为客户端（client）。使用时需首先启动`server.exe`，再启动若干个`client.exe`，若干个`client`借助`server`完成通信
* 为了保证通讯质量，本次实验在`server`设置了最大连接数`MaxClient`、最大缓冲区`BufSize`。当最大连接数上限时，无法再连接；当发送的信息达到消息缓冲区的限制时，超出部分无法发送
* 服务器和客户端之间通过 `send` 和 `recv` 函数来发送和接收消息数据，这些数据的格式和大小都受到语法规则的约束
* 编写了一系列错误处理函数，输出错误信息，方便调试程序

### 服务器端

- 服务器端监听指定端口（在代码中为`8000`），等待客户端的连接请求
- 服务器端可以同时接受多个客户端的连接请求，最多支持`MaxClient - 1`个客户端同时连接，当达到最大连接数时，不再接受新的连接请求
- 每个客户端连接后，服务器为其创建一个独立的线程（`ThreadFunction`函数），并使用一个新的`socket`来处理该客户端的消息接收和转发
- 服务器的每个线程接收它所负责客户端的消息，并将消息打上时间标签再发送给其他客户端并打印到日志信息
- 服务器维护一个`clientSockets`数组，用于存储每个客户端的套接字，以便进行消息的收发

### 客户端

- 客户端创建一个套接字，然后连接到服务器的IP地址和端口号（在代码中为`127.0.0.1`和`8000`）
- 客户端也创建一个独立的线程（`recvThread`函数）来接收服务器发送的消息，并将其显示在控制台上
- 客户端可以通过在控制台中输入文本消息，然后将其发送给服务器

### **消息格式**：

- 消息格式为`"[#ClientID]: Message --Timestamp"`，其中`ClientID`是客户端的唯一标识，`Message`是实际消息内容，`Timestamp`是消息的时间戳
- 客户端发送消息时，服务器接收到消息后会在服务器端控制台显示，并将消息转发给其他所有客户端，以便实现聊天功能
- 服务器端也会接收来自其他客户端的消息，并将其显示在服务器端控制台上

### **退出机制**：

- 客户端可以输入`exit`来退出聊天程序，此时客户端会关闭连接，并在服务器端显示客户端退出的消息
- 服务器端会检测客户端的连接状态，如果客户端主动关闭连接，服务器会在控制台上显示客户端退出的消息，并关闭相应的套接字，释放资源



# 二、功能实现与代码分析

## （1）服务器端

该部分实现了一个多线程的聊天服务器，允许多个客户端连接并在客户端之间实现消息广播。通过创建线程处理每个客户端的通信，实现了同时处理多个客户端的连接请求和消息传递。

### 多线程通信

本次实验中，通过编写线程函数，借助宏定义，实现了有连接上限的多线程通信。

代码开头，实现了对连接数、客户端`socket`数组等的定义

```c++
#define PORT 8000  //端口号
#define BufSize 1024  //缓冲区大小
#define MaxClient 5 //最大连接数 = MaxClient - 1

SOCKET clientSockets[MaxClient];//客户端socket数组
SOCKET serverSocket;//服务器端socket
SOCKADDR_IN clientAddrs[MaxClient];//客户端地址数组
SOCKADDR_IN serverAddr;//定义服务器地址

int current_connect_count = 0; //当前连接的客户数
int condition[MaxClient] = {};//每一个连接的情况
```

编写了一个线程函数。每个客户端连接都会创建一个线程，函数负责处理该客户端的通信。

* 该函数首先通过传递给线程的参数 `lpParameter` 获取当前连接的`cilentSocket`索引
* 使用 `recv` 函数接收客户端发送的消息，并根据协议的要求进行处理
* 如果接收成功，将消息格式化为特定格式，并通过 `send` 函数发送给其他连接的客户端，实现消息广播
* 如果客户端主动关闭连接，会通过 `recv` 返回值判断，并在服务器端记录客户端退出的时间

```c++
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
```

### main函数

- 在 `main` 函数中，首先进行`WinSock2`库的初始化，检查初始化是否成功
- 创建服务器套接字 `serverSocket`，并绑定服务器地址和端口
- 设置监听，以便接受客户端连接请求
- 进入循环，不断接受客户端的连接请求，如果连接数未达到最大连接数 `MaxClient`，则创建新的线程来处理客户端通信
- 在 `main` 函数中，也记录了客户端的连接时间和当前连接数，以及处理连接数达到最大值时的情况

```c++
int main()
{
    //system("chcp 936");//防止乱码
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
    //serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
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
```

## （2）客户端

该部分实现了一个简单的客户端程序，用于连接到服务器并进行基本的消息通信。它使用多线程来同时接收和发送消息，允许用户在控制台上输入消息，并将消息发送到服务器。

代码开头部分进行了与服务器端相近的宏定义、全局变量等的声明，此处不再赘述。

### 线程函数

编写了一个接收信息的线程函数`recvThread`，用于接收从服务器发送过来的消息并显示在控制台上

* 使用 `recv` 函数来接收消息，然后将消息显示在控制台上
* 如果接收到的消息小于等于 0，表示连接已经断开，线程将退出

```c++
DWORD WINAPI recvThread() //接收消息线程
{
	while (true)
	{
		char buffer[BufSize] = {};//接收数据缓冲区
		if (recv(clientSocket, buffer, sizeof(buffer), 0) > 0)
		{
			cout << buffer << endl;
		}
		else if (recv(clientSocket, buffer, sizeof(buffer), 0) < 0)
		{
			cout << "The Connection is lost!" << endl;
			break;
		}
	}
	Sleep(100);//延时100ms
	return 0;
}
```

### main函数

- 在 `main` 函数中，首先进行`WinSock2`库的初始化，检查初始化是否成功
- 创建客户端套接字 `clientSocket`，并连接到服务器
- 如果连接失败，程序会输出错误信息并退出
- 如果连接成功，程序会输出连接成功的信息
- `main` 函数进入一个循环，等待用户输入消息
  - 用户可以通过键盘输入消息，然后使用 `send` 函数将消息发送给服务器
  - 如果用户输入 "exit"，则退出循环，关闭套接字，清理WinSock2库并退出程序

```c++
int main()
{
	//system("chcp 936");//防止乱码
	//初始化DLL
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		perror("Error in Initializing Socket DLL!\n");
		cout << endl;
		exit(EXIT_FAILURE);
	}
	cout << "Initializing Socket DLL is successful!\n" << endl;

	//创建客户端套接字
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET)
	{
		cout << "Error in Creating Socket!\n" << endl;
		exit(EXIT_FAILURE);
		return -1;
	}
	cout << "Creating Socket is successful!\n" << endl;

	//绑定服务器地址
	servAddr.sin_family = AF_INET;//地址类型
	servAddr.sin_port = htons(PORT);//端口号
	if (inet_pton(AF_INET, "127.0.0.1", &(servAddr.sin_addr)) != 1) {
		cout << "Error in Inet_pton" << endl;
		exit(EXIT_FAILURE);
	}
	//servAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");


	//向服务器发起请求
	if (connect(clientSocket, (SOCKADDR*)&servAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "Error in Connection: " << WSAGetLastError() << endl;
		exit(EXIT_FAILURE);
	}
	else
	{
		cout << "Connect is successful! \n" << endl;
	}

	//创建消息线程
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)recvThread, NULL, 0, 0);

	char buf[BufSize] = {};
	cout << "Enter a message to send or 'exit' to end the chatting!" << endl;
	
	//发送消息
	while (true)
	{
		cin.getline(buf, sizeof(buf));
		if (strcmp(buf, "exit") == 0) //输入exit退出
		{
			break;
		}
		send(clientSocket, buf, sizeof(buf), 0);//发送消息
	}

	closesocket(clientSocket);
	WSACleanup();


	return 0;
}
```

## （3）遇到的问题及解决方案

在设计退出程序时，由于先编写的server程序，最开始选择在server端进行信息检测。将收到的消息进行分析，如果是`exit`，就关闭当前的`cilentSocket[num]`,记录推出时间并发送关闭消息。基于此编写了如下代码：

```c++
for (int i = 0; i < 5; i++)
{
	exitBuf[i] = RecvBuf[i];
}
if (strcmp(exitBuf, "exit"))
{
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
    send(clientSockets[num], "Your server has been closed!", sizeof(SendBuf), 0);
    return 0;
}
```

但是运行时发现，一旦输入中文，客户端会自动断开连接并退出。修改函数参数类型、控制台编码等均未能改变这一情况。于是我选择将退出机制放在了客户端实现。

```c++
if (strcmp(buf, "exit") == 0) //输入exit退出
{
	break;
}
```



# 三、运行结果展示

根据实验要求，需实现多人聊天，故此处以三人聊天为示例。

* 首先启动服务器，运行server

  * 控制台已经启动，并输出操作日志

    * Socket DLL初始化成功
    * Socket创建成功
    * 端口号已经绑定
    * 开始监听

    <img src="./pic/%E5%BE%AE%E4%BF%A1%E6%88%AA%E5%9B%BE_20230923130958.png" style="zoom:50%;" />

  * 如果在服务器端未启动的情况下运行客户端，会出现连接失败的错误提示

    <img src="./pic/%E5%BE%AE%E4%BF%A1%E6%88%AA%E5%9B%BE_20230923131350.png" style="zoom:50%;" />

* 接着启动三个客户端程序，模拟三人聊天

  <img src="./pic/%E5%BE%AE%E4%BF%A1%E6%88%AA%E5%9B%BE_20230923132134.png" style="zoom: 33%;" />

  * 客户端分别输出操作日志
    * Socket DLL初始化成功
    * Socket创建成功
    * 连接成功
  * 服务器端显示当前连接数以及连接情况

* 在其中一个聊天框中输入`你好，我是1号`并按下回车，可以看到消息发送成功，服务器端输出消息发送内容及时间，所有客户端中同步显示

  <img src="./pic/%E5%BE%AE%E4%BF%A1%E6%88%AA%E5%9B%BE_20230923144814.png" style="zoom: 33%;" />

* 在另一个聊天框中输入`hello`并按下回车，可以看到与上面类似的情况

  <img src="./pic/%E5%BE%AE%E4%BF%A1%E6%88%AA%E5%9B%BE_20230923145241.png" style="zoom: 33%;" />

  * 借助`wire shark`进行数据包抓取，可以看到如下结果

    <img src="./pic/%E5%BE%AE%E4%BF%A1%E5%9B%BE%E7%89%87_20230923145418.png" style="zoom: 33%;" />

* 在第三个窗口中输入`exit`退出聊天，可以看到服务端提示该客户端已经退出，并与之断开连接

  <img src="./pic/%E5%BE%AE%E4%BF%A1%E6%88%AA%E5%9B%BE_20230923145819.png" style="zoom: 33%;" />

* 直接关闭第二个聊天窗口，可以看到服务器端也提示该客户端已经退出，并与之断开连接

  <img src="./pic/%E5%BE%AE%E4%BF%A1%E6%88%AA%E5%9B%BE_20230923150153.png" style="zoom:33%;" />

* 打开`MaxClient`个客户端，可以看到服务器端显示`Fulling...`，表明已经达到最大连接数

  <img src="./pic/%E5%BE%AE%E4%BF%A1%E6%88%AA%E5%9B%BE_20230923150355.png" style="zoom:33%;" />
