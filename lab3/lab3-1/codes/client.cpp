#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <cstring>

#pragma comment(lib,"ws2_32.lib")   //socket库
using namespace std;

#define PORT 8000  //端口号
#define BufSize 1024  //缓冲区大小
SOCKET clientSocket; //定义客户端socket
SOCKADDR_IN servAddr; //定义服务器地址

#define _WINSOCK_DEPRECATED_NO_WARNINGS

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