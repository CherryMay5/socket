#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <mutex>    // 互斥锁
#include <map>
#include <string>

#pragma comment(lib,"Ws2_32.lib")  // 链接Winsock库
#pragma warning(disable:4996)      // 禁用4996号警告（如不安全的函数调用警告）

#define BUFFER_SIZE 1024 // 定义缓冲区大小
#define SERVER_PORT 5865 // 服务器监听端口

using namespace std;

// 用于存储客户端信息的结构体
struct ClientInfo {
    SOCKET socket;   // 客户端的Socket句柄
    string ip;       // 客户端的IP地址
    int port;        // 客户端的端口号
};

vector<ClientInfo>clients;  // 存储所有已连接客户端的信息
mutex client_mutex;         // 互斥锁，用于保护客户端列表的访问

// 获取当前服务器时间并返回为字符串
string get_current_time()
{
    time_t now = time(nullptr); // 获取当前时间
    char buf[BUFFER_SIZE];
    strftime(buf, sizeof(buf),"%Y-%m-%d %H:%M:%S",localtime(&now)); // 格式化时间
    return string(buf);
}

// 处理客户端连接的线程函数
void handle_client(ClientInfo client)
{
    char buffer[BUFFER_SIZE]; // 缓冲区，用于存储接收到的数据
    cout << "New connection from " << client.ip << ":" << client.port << endl;

    // 发送欢迎消息给客户端
    string welcome_msg = "Hello from server!\n";
    send(client.socket, welcome_msg.c_str(), welcome_msg.size(), 0);

    while(true)
    {
        // 清空缓冲区
        memset(buffer, 0, BUFFER_SIZE);
        // 接收客户端数据
        int bytes_received = recv(client.socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0)    // 接收数据失败或客户端断开连接
        {
            cout << "Client disconnected: " << client.ip << ":" << client.port << endl;

            // 从客户端列表中移除
            client_mutex.lock();
            auto it = remove_if(clients.begin(),clients.end(),
                                            [&](const ClientInfo& c) {
                                                    return c.socket == client.socket;
                                                });
            clients.erase(it, clients.end());
            client_mutex.unlock();

            closesocket(client.socket); // 关闭客户端的Socket
            break;
        }

        string request(buffer); // 将接收到的数据转换为字符串
        cout << "Received from " << client.ip << ": " << request << endl;

        // 根据客户端请求进行处理
        if (request == "GET_TIME")  // 客户端请求服务器时间
        {
            string time_msg = "Server Time: " + get_current_time() + "\n";
            send(client.socket, time_msg.c_str(), time_msg.size(), 0);
        } else if (request == "GET_NAME")   // 客户端请求服务器名称
        {
            char hostname[BUFFER_SIZE];
            gethostname(hostname, sizeof(hostname)); // 获取主机名
            string name_msg = "Server Name: " + string(hostname) + "\n";
            send(client.socket, name_msg.c_str(), name_msg.size(), 0);
        } else if (request == "GET_CLIENTS")    // 客户端请求当前活动的连接列表
        {
            string clients_msg = "Active Clients:\n";

            client_mutex.lock();
            for (size_t i = 0; i < clients.size(); ++i) {
                clients_msg += to_string(i + 1) + ". " + clients[i].ip + ":" + to_string(clients[i].port) + "\n";
            }
            client_mutex.unlock();

            send(client.socket, clients_msg.c_str(), clients_msg.size(), 0);
        } else if (request.rfind("SEND_MSG", 0) == 0)   // 客户端发送消息给指定的其他客户端
        {
            // 客户端发送消息格式：SEND_MSG <client_id> <message>
            size_t space1 = request.find(' ');
            size_t space2 = request.find(' ', space1 + 1);

            if (space1 != string::npos && space2 != string::npos)
            {
                int client_id = stoi(request.substr(space1 + 1, space2 - space1 - 1)) - 1; // 获取目标客户端ID
                string message = request.substr(space2 + 1); // 获取消息内容

                client_mutex.lock();
                if (client_id >= 0 && client_id < clients.size())
                {
                    string full_message = "Message from " + client.ip + ": " + message + "\n";
                    send(clients[client_id].socket, full_message.c_str(), full_message.size(), 0);
                } else
                {
                    string error_msg = "Invalid client ID\n";
                    send(client.socket, error_msg.c_str(), error_msg.size(), 0);
                }
                client_mutex.unlock();
            } else
            {
                string error_msg = "Invalid SEND_MSG format\n";
                send(client.socket, error_msg.c_str(), error_msg.size(), 0);
            }
        } else  // 未知命令
        {
            string error_msg = "Unknown command\n";
            send(client.socket, error_msg.c_str(), error_msg.size(), 0);
        }
    }
}

int main()
{
    // 初始化Winsock库
    WORD wVersion = MAKEWORD(2, 2);
    WSADATA wsa_data;
    if (WSAStartup(wVersion, &wsa_data) != 0)
    {
        cerr << "WSAStartup failed." << endl;
        return 1;
    }

    // 创建服务器Socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed." << endl;
        WSACleanup();
        return 1;
    }

    // 绑定Socket到指定端口
    sockaddr_in server_address{};
    memset(&server_address,0,sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY; // 监听所有可用的网络接口

    if (bind(server_socket,(sockaddr*)&server_address,sizeof(server_address)) == SOCKET_ERROR)
    {
        cerr << "Bind failed." << endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // 开始监听连接
    if (listen(server_socket,SOMAXCONN) == SOCKET_ERROR)
    {
        cerr << "Listen failed." << endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    cout << "Server listening on port " << SERVER_PORT << "..." << endl;

    while (true)
    {
        sockaddr_in client_address{};
        int client_address_size = sizeof(client_address);

        // 接受客户端连接
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_size);
        if (client_socket == INVALID_SOCKET)
        {
            cerr << "Accept failed." << endl;
            continue;
        }

        // 获取客户端信息
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_address.sin_port);

        ClientInfo client_info = { client_socket, string(client_ip), client_port };

        // 将新客户端加入列表
        client_mutex.lock();
        clients.push_back(client_info);
        client_mutex.unlock();

        // 启动处理客户端的线程
        thread client_thread(handle_client, client_info);
        client_thread.detach();
    }

    // 关闭服务器Socket
    closesocket(server_socket);
    WSACleanup();
    return 0;
}
