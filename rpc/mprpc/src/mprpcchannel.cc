#include "mprpcchannel.h"
#include <string>
#include "rpcheader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "mprpcapplication.h"
#include "mprpccontroller.h"

/*
header_size + service_name method_name args_size + args
*/
// 所有通过stub代理对象调用的rpc方法，都会走到这里了，
// 统一通过rpcChannel来调用方法
// 统一做rpc方法调用的数据数据序列化和网络发送
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                              google::protobuf::RpcController *controller,
                              const google::protobuf::Message *request,
                              google::protobuf::Message *response,
                              google::protobuf::Closure *done)
{
    const google::protobuf::ServiceDescriptor *sd = method->service();
    std::string service_name = sd->name();    // service_name
    std::string method_name = method->name(); // method_name

    // 获取参数的序列化字符串长度 args_size
    uint32_t args_size = 0;
    std::string args_str;
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        controller->SetFailed("serialize request error!");
        return;
    }

    // 定义rpc的请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    // 组织待发送的rpc请求的字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char *)&header_size, 4)); // header_size
    send_rpc_str += rpc_header_str;                               // rpcheader
    send_rpc_str += args_str;                                     // args

    // 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "============================================" << std::endl;

    // 使用tcp编程，完成rpc方法的远程调用，使用的是短连接，因此每次都要重新连接上去，待改成长连接。
    while (m_clientFd == -1)
    { // 没有连接或者连接已经断开，那么就要重新连接呢,会一直不断地重试
        // 读取配置文件rpcserver的信息
        // std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
        // uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
        // rpc调用方想调用service_name的method_name服务，需要查询zk上该服务所在的host信息
        //  /UserServiceRpc/Login
        std::string method_path = "/" + service_name + "/" + method_name;
        // 127.0.0.1:8000
        // show m_serviceUMap ，可以注释掉
        for (auto it = m_serviceUMap.begin(); it != m_serviceUMap.end(); it++)
        {
            auto oneService = it->second;
            cout << "service name:" << it->first << endl;
            for (auto itt = oneService.begin(); itt != oneService.end(); itt++)
            {
                cout << "     method name:" << itt->first << endl;
                auto hr = itt->second;
            }
            cout << "------------------" << endl;
        }

        //
        HashRing *hrPtr = &m_serviceUMap["/" + service_name][method_path];
        cout << (hrPtr == nullptr) << endl;

        std::string host_data = hrPtr->distributionNode(to_string(random()));

        cout << "host_data:" << host_data << endl;

        if (host_data == "")
        {
            controller->SetFailed(method_path + " is not exist!");
            return;
        }
        int idx = host_data.find(":");
        if (idx == -1)
        {
            controller->SetFailed(method_path + " address is invalid!");
            return;
        }
        std::string ip = host_data.substr(0, idx);
        uint16_t port = atoi(host_data.substr(idx + 1, host_data.size() - idx).c_str());
        string rt = newConnect(ip.c_str(), port);
        if (!rt.empty())
        {
            controller->SetFailed(rt);
            return;
        }
    }

    // 发送rpc请求
    if (-1 == send(m_clientFd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        close(m_clientFd); m_clientFd = -1;
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }
    /*
    从时间节点来说，这里将请求发送过去之后rpc服务的提供者就会开始处理，返回的时候就代表着已经返回响应了
    */

    // 接收rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(m_clientFd, recv_buf, 1024, 0)))
    {
        close(m_clientFd); m_clientFd = -1;
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 反序列化rpc调用的响应数据
    // std::string response_str(recv_buf, 0, recv_size); // bug出现问题，recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败
    // if (!response->ParseFromString(response_str))
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        char errtxt[1050] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }

}

MprpcChannel::MprpcChannel(string ip, short port)
{
    string errMsg;
    bool connected  = newConnect(ip.c_str(), port,&errMsg);
    if(!connected){
        cout << "Failed to create a new connection to the server: " << errMsg << endl;
        exit(-1);
    }
    
}

bool MprpcChannel::newConnect(const char *ip, uint16_t port,string* errMsg)
{
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno:%d", errno);
        m_clientFd = -1;
        *errMsg = errtxt;
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    // 连接rpc服务节点
    if (-1 == connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! errno:%d", errno);
        m_clientFd = -1;
        *errMsg = errtxt;
        return false;
    }
    m_clientFd = clientfd;
    return true;
}
