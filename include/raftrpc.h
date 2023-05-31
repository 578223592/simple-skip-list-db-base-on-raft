#pragma once
#include <iostream>
#include "mprpcapplication.h"
#include "raftRPC.pb.h"
#include "rpcprovider.h"
using namespace mprrpc;

/// @brief 维护当前节点对其他某一个结点的所有rpc通信，包括接收其他节点的rpc和发送
// 对于一个raft节点来说，对于任意其他的节点都要维护一个rpc连接，即MprpcChannel
class RaftRpc
{
private:
    mprrpc::raftRpc_Stub  *stub;  
public:
    //主动调用其他节点的三个方法,可以按照mit6824来调用，但是别的节点调用自己的好像就不行了，要继承protoc提供的service类才行
    bool AppendEntries(AppendEntriesArgs* args,AppendEntriesReply* response);
    bool InstallSnapshot(InstallSnapshotRequest *args, InstallSnapshotResponse *response);
    bool RequestVote(RequestVoteArgs *args, RequestVoteReply * response);
    //响应其他节点的方法

    RaftRpc(string ip, short port);
    ~RaftRpc();
};

inline bool RaftRpc::AppendEntries(AppendEntriesArgs *args, AppendEntriesReply *response)
{
    MprpcController controller;
    stub->AppendEntries(&controller, args, response, nullptr);
    return !controller.Failed();
}

inline bool RaftRpc::InstallSnapshot(InstallSnapshotRequest *args, InstallSnapshotResponse *response)
{
    MprpcController controller;
    stub->InstallSnapshot(&controller, args, response, nullptr);
    return !controller.Failed();
}

inline bool RaftRpc::RequestVote(RequestVoteArgs *args, RequestVoteReply *response)
{
    MprpcController controller;
    stub->RequestVote(&controller, args, response, nullptr);
    return !controller.Failed();
}
//先开启服务器，再尝试连接其他的节点，中间给一个间隔时间，等待其他的rpc服务器节点启动
RaftRpc::RaftRpc(std::string ip,short port)
{
    //*********************************************  */
    // 接收rpc设置
    // todo:2023-5-26，改到这里了，channel里面应该维护的是长连接
    // 然后传入ip和port就指定使用这个ip和port而不是每次都去zk里面拉取

    // 调用框架的初始化操作
    MprpcApplication::Init(argc, argv); // 后面provider需要来查配置

    // provider是一个rpc网络服务对象。把UserService对象发布到rpc节点上
    RpcProvider provider;
    provider.NotifyService(new FriendService()); //todo，这里待修改

    // 启动一个rpc服务发布节点   Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();
    //*********************************************  */
    //发送rpc设置
    stub = new mprrpc::raftRpc_Stub(new MprpcChannel(ip, port));
}

RaftRpc::~RaftRpc()
{
    delete stub;
}
