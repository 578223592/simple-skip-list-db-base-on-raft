#pragma once

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <string>
#include <iostream>
#include <functional>
#include <algorithm>
#include <vector>
#include <map>
#include <algorithm> // 包含 std::generate_n() 和 std::generate() 函数的头文件
#include <random>    // 包含 std::uniform_int_distribution 类型的头文件
#include "zookeeperutil.h"
#include <unordered_map>
using namespace std;

// 真正负责发送和接受的前后处理工作
//  如消息的组织方式，向哪个节点发送等等
class MprpcChannel : public google::protobuf::RpcChannel
{

public:
    // 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送 那一步
    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done);
    MprpcChannel(string ip, short port);

private:
    int m_clientFd;

    /// @brief 连接ip和端口,并设置m_clientFd
    /// @param ip ip地址，本机字节序
    /// @param port 端口，本机字节序
    /// @return 成功返回空字符串，否则返回失败信息
    bool newConnect(const char *ip, uint16_t port, string *errMsg);
    // todo:记得初始化和每次节点变动可能涉及到重新连接
    /// @brief 创建目录
};