#ifndef  RAFTRPC_H
#define RAFTRPC_H


#include <iostream>
#include "raftRPC.pb.h"
#include "rpcprovider.h"
#include "mprpccontroller.h"
#include "mprpcchannel.h"
/// @brief 维护当前节点对其他某一个结点的所有rpc发送通信的功能
// 对于一个raft节点来说，对于任意其他的节点都要维护一个rpc连接，即MprpcChannel
class RaftRpc
{
private:
    mprrpc::raftRpc_Stub  *stub;  
public:
    //主动调用其他节点的三个方法,可以按照mit6824来调用，但是别的节点调用自己的好像就不行了，要继承protoc提供的service类才行
    bool AppendEntries(mprrpc::AppendEntriesArgs* args,mprrpc::AppendEntriesReply* response);
    bool InstallSnapshot(mprrpc::InstallSnapshotRequest *args, mprrpc::InstallSnapshotResponse *response);
    bool RequestVote(mprrpc::RequestVoteArgs *args, mprrpc::RequestVoteReply * response);
    //响应其他节点的方法

    RaftRpc(std::string ip, short port);
    ~RaftRpc();
};

#endif //RAFTRPC_H