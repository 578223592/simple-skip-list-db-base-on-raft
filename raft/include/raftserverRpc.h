#ifndef RAFTSERVERRPC_H
#define RAFTSERVERRPC_H


#include <iostream>
#include "mprpcchannel.h"
#include "kvServerRPC.pb.h"
#include "rpcprovider.h"
#include "mprpccontroller.h"

/// @brief 维护当前节点对其他某一个结点的所有rpc通信，包括接收其他节点的rpc和发送
// 对于一个节点来说，对于任意其他的节点都要维护一个rpc连接，
class kvServerRpc
{
private:
    mprrpc::kvServerRpc_Stub *stub;

public:
    //主动调用其他节点的三个方法,可以按照mit6824来调用，但是别的节点调用自己的好像就不行了，要继承protoc提供的service类才行

    //响应其他节点的方法
    bool Get(mprrpc::GetArgs* GetArgs,mprrpc::GetReply* reply);
    bool PutAppend(mprrpc::PutAppendArgs* args,mprrpc::PutAppendReply* reply);

    kvServerRpc(std::string ip, short port);
    ~kvServerRpc();
};




#endif //RAFTSERVERRPC_H

