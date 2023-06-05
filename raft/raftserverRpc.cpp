#include "raftserverRpc.h"


//kvserver不同于raft节点之间，kvserver的rpc是用于clerk向kvserver调用，不会被调用，因此只用写caller功能，不用写callee功能
//先开启服务器，再尝试连接其他的节点，中间给一个间隔时间，等待其他的rpc服务器节点启动
kvServerRpc::kvServerRpc(std::string ip,short port)
{
    //*********************************************  */
    // 接收rpc设置
    //*********************************************  */
    //发送rpc设置
    stub = new mprrpc::kvServerRpc_Stub(new MprpcChannel(ip, port));
}

kvServerRpc::~kvServerRpc() {
    delete stub;
}

bool kvServerRpc::Get(mprrpc::GetArgs *GetArgs, mprrpc::GetReply *reply) {
    MprpcController controller;
    stub->Get(&controller, GetArgs, reply, nullptr);
    return !controller.Failed();
}

bool kvServerRpc::PutAppend(mprrpc::PutAppendArgs *args, mprrpc::PutAppendReply *reply) {
    MprpcController controller;
    stub->PutAppend(&controller, args, reply, nullptr);
    return !controller.Failed();
}
