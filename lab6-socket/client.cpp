#include <iostream>
#include <WinSock2.h>                   // socket 头文件
#include <ws2tcpip.h>
#include <thread>
#include <string>

#pragma comment(lib,"Ws2_32.lib")      // 链接Winsock库，确保开发环境支持Winsock 2.2（通常用这个命令就会链接相应的库）
#pragma warning(disable:4996)           // 报错函数——禁用4996号警告（如不安全的函数调用警告）

#define BUFFER_SIZE 1024                // 定义缓冲区大小

using namespace std;

SOCKET client_socket = INVALID_SOCKET;  // 客户端Socket
bool is_connected = false;              // 连接状态标志
thread recv_thread;                     // 接收数据的子线程

// 接收数据的子线程函数
void receive_data()
{
    char buffer[BUFFER_SIZE];
    while(is_connected)
    {
        memset(buffer,0,BUFFER_SIZE);       // 清空缓冲区
        int bytes_received = recv(client_socket,buffer,BUFFER_SIZE-1,0);   // 接收数据
        if(bytes_received <= 0)
        {
            cout << "Disconnected from server." << endl;
            is_connected = false;
            break;
        }
        cout << "Server: " << buffer << endl;       // 输出服务器发送的数据
    }
}

// 连接到服务器
void connect_to_server()
{
    if (is_connected)
    {
        cout << "Already connected to a server." << endl;
        return;
    }

    string server_ip;
    int server_port;
    cout << "Enter server   IP: ";
    cin >> server_ip;       // 输入服务器IP地址
    cout << "Enter server port: ";
    cin >> server_port;     // 输入服务器端口

    client_socket = socket(AF_INET, SOCK_STREAM, 0);    // 创建Socket
    if (client_socket == INVALID_SOCKET) {
        cerr << "Socket creation failed." << endl;
        return;
    }

    sockaddr_in server_address{};
    memset(&server_address,0,sizeof(server_address));          // 初始化服务器地址结构
    server_address.sin_family = AF_INET;                              // IPv4
    server_address.sin_port = htons(server_port);                     // 设置端口号

    // 将IP地址转换为网络字节序
    if (inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr) <= 0)
    {
        cerr << "Invalid IP address." << endl;
        closesocket(client_socket);
        return;
    }

    // 尝试连接到服务器
    if (connect(client_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR)
    {
        cerr << "Connection failed." << endl;
        closesocket(client_socket);
        return;
    }

    cout << "Connected to server " << server_ip << ":" << server_port << endl;
    is_connected = true;

    // 启动接收数据的子线程
    recv_thread = thread(receive_data);
}

// 断开与服务器的连接
void disconnect_from_server()
{
    if (!is_connected)
    {
        cout << "Not connected to any server." << endl;
        return;
    }

    is_connected = false; // 更新连接状态
    closesocket(client_socket); // 关闭Socket
    if (recv_thread.joinable())
    {
        recv_thread.join(); // 等待子线程结束
    }
}

// 发送请求到服务器
void send_request(const string& request)
{
    if (!is_connected)
    {
        cout << "Not connected to any server." << endl;
        return;
    }

    send(client_socket,request.c_str(),request.length(),0); // 发送数据
}

// 发送消息给指定的客户端
void send_message()
{
    if (!is_connected)
    {
        cout << "Not connected to any server." << endl;
        return;
    }

    int client_id;
    string message;
    cout << "Enter client ID: ";
    cin >> client_id;               // 输入目标客户端ID
    cin.ignore();                   // 清空输入缓冲区
    cout << "Enter message  : ";
    getline(cin,message);     // 输入消息内容

    // 构造完整请求
    string full_message = "SEND_MSG " + to_string(client_id) + " " + message;
    send_request(full_message);
}

// 显示功能菜单
void display_menu()
{
    cout << "--------------------------------------------" << endl;
    cout << "|                   Menu                   |" << endl;
    cout << "|------------------------------------------|" << endl;
    cout << "|   1   |      Connect to server           |" << endl;
    cout << "|   2   |      Disconnect from server      |" << endl;
    cout << "|   3   |      Get server time             |" << endl;
    cout << "|   4   |      Get server name             |" << endl;
    cout << "|   5   |      Get active connections      |" << endl;
    cout << "|   6   |      Send message to client      |" << endl;
    cout << "|   7   |      Exit                        |" << endl;
    cout << "--------------------------------------------" << endl;
    cout << "Please select an option: ";
}

// 显示部分功能菜单
void display_menu_lack()
{
    cout << "--------------------------------------------" << endl;
    cout << "|                   Menu                   |" << endl;
    cout << "|------------------------------------------|" << endl;
    cout << "|   1   |      Connect to server           |" << endl;
    cout << "|   7   |      Exit                        |" << endl;
    cout << "--------------------------------------------" << endl;
    cout << "Please select an option: ";
}

int main() {
    // 初始化Winsock库
    WORD web_Version = MAKEWORD(2, 2);
    WSADATA wsa_data;
    if (WSAStartup(web_Version, &wsa_data) != 0) {
        cerr << "WSAStartup failed." << endl;
        return 1;
    }

    while (true) {
        Sleep(500); // 括号里面填写暂停时间500ms   让每一次执行命令显示分离
        if(is_connected) {
            display_menu();         // 已连接，显示菜单
        }else {
            display_menu_lack();    // 未连接，显示部分菜单
        }

        int option;
        cin >> option;          // 获取用户选择

        switch (option) {
            case 1: // 连接到服务器
                connect_to_server();
                break;
            case 2: // 断开连接
                disconnect_from_server();
                break;
            case 3: // 获取服务器时间
                send_request("GET_TIME");
                break;
            case 4: // 获取服务器名称
                send_request("GET_NAME");
                break;
            case 5: // 获取活动连接列表
                send_request("GET_CLIENTS");
                break;
            case 6: // 发送消息到客户端
                send_message();
                break;
            case 7: // 退出
                if (is_connected)
                {
                    disconnect_from_server(); // 退出前断开连接
                }
                cout << "Exiting program." << endl;
                WSACleanup(); // 清理Winsock资源
                return 0;
            default:
                cout << "Invalid option. Please try again." << endl;
                break;
        }
    }

    // 反初始化，清理Winsock资源
    WSACleanup();
    return 0;
}
