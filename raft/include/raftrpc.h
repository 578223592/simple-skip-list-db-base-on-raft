#pragma once
#include <iostream>
#include "mprpcapplication.h"
#include "../../rpc/example/include/raftRPC.pb.h"
#include "rpcprovider.h"
using namespace mprrpc;

/// @brief 维护当前节点对其他某一个结点的所有rpc发送通信的功能
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

    RaftRpc(std::string ip, short port);
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
    //发送rpc设置
    stub = new mprrpc::raftRpc_Stub(new MprpcChannel(ip, port));
}

RaftRpc::~RaftRpc()
{
    delete stub;
}
