//
// Created by swx on 23-6-1.
//

#ifndef SKIP_LIST_ON_RAFT_KVSERVER_H
#define SKIP_LIST_ON_RAFT_KVSERVER_H

#include <mutex>
#include "raft.h"
#include <unordered_map>
#include "kvServerRPC.pb.h"

using string = std::string;



class KvServer : mprrpc::kvServerRpc {
private:
    std::mutex m_mtx;
    int m_me;
    Raft * m_raftNode;
    LockQueue<ApplyMsg> *applyChan;//kvServer和raft节点的通信管道
    int m_maxRaftState; // snapshot if log grows this big

    // Your definitions here.
    std::unordered_map<std::string, std::string> m_kvDB;

    std::unordered_map<int, LockQueue<Op>*> waitApplyCh;  // index(raft) -> chan  //？？？字段含义   waitApplyCh是一个map，键是int，值是Op类型的管道

    std::unordered_map<string, int> m_lastRequestId;   // clientid -> requestID  //一个kV服务器可能连接多个client

    // last SnapShot point , raftIndex
    int m_lastSnapShotRaftLogIndex;


public:
    void DprintfKVDB();

    void ExecuteAppendOpOnKVDB(Op op);

    void ExecuteGetOpOnKVDB(Op op,string* value,bool* exist);

    void ExecutePutOpOnKVDB(Op op);

    void Get(const GetArgs *args, GetReply *reply);   //将 GetArgs 改为rpc调用的，因为是远程客户端，即服务器宕机对客户端来说是无感的

    void GetCommandFromRaft(ApplyMsg message);

    bool ifRequestDuplicate(string ClientId, int RequestId);

    void PutAppend(const PutAppendArgs *args, PutAppendReply *reply);


    void ReadRaftApplyCommandLoop();

    void ReadSnapShotToInstall(string snapshot);

    bool SendMessageToWaitChan( Op op,int raftIndex );

public:
    virtual void PutAppend(google::protobuf::RpcController *controller,
                           const ::mprrpc::PutAppendArgs *request,
                           ::mprrpc::PutAppendReply *response,
                           ::google::protobuf::Closure *done);

    virtual void Get(google::protobuf::RpcController *controller,
                     const ::mprrpc::GetArgs *request,
                     ::mprrpc::GetReply *response,
                     ::google::protobuf::Closure *done);
};




#endif //SKIP_LIST_ON_RAFT_KVSERVER_H
