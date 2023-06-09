#include "raftrpc.h"



 bool RaftRpc::AppendEntries(mprrpc::AppendEntriesArgs *args, mprrpc::AppendEntriesReply *response)
{

    MprpcController controller;
    stub->AppendEntries(&controller, args, response, nullptr);
    return !controller.Failed();
}

 bool RaftRpc::InstallSnapshot(mprrpc::InstallSnapshotRequest *args, mprrpc::InstallSnapshotResponse *response)
{
    MprpcController controller;
    stub->InstallSnapshot(&controller, args, response, nullptr);
    return !controller.Failed();
}

 bool RaftRpc::RequestVote(mprrpc::RequestVoteArgs *args, mprrpc::RequestVoteReply *response)
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
    stub = new mprrpc::raftRpc_Stub(new MprpcChannel(ip, port,true));
}

RaftRpc::~RaftRpc()
{
    delete stub;
}
