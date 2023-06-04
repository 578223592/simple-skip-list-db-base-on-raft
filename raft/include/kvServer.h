//
// Created by swx on 23-6-1.
//

#ifndef SKIP_LIST_ON_RAFT_KVSERVER_H
#define SKIP_LIST_ON_RAFT_KVSERVER_H

#include <mutex>
#include "raft.h"
#include <unordered_map>
#include "../../rpc/example/include/kvServerRPC.pb.h"
#include <iostream>
#include <unordered_map>
#include "boost/serialization/serialization.hpp"
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include "boost/archive/binary_oarchive.hpp"
#include "boost/archive/binary_iarchive.hpp"
#include <boost/serialization/export.hpp>
#include "boost/foreach.hpp"
#include "boost/any.hpp"
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
using string = std::string;



class KvServer : mprrpc::kvServerRpc {

private:
    std::mutex m_mtx;
    int m_me;
    std::shared_ptr<Raft>  m_raftNode;
    std::shared_ptr<LockQueue<ApplyMsg>> applyChan;//kvServer和raft节点的通信管道
    int m_maxRaftState; // snapshot if log grows this big

    // Your definitions here.
    std::unordered_map<std::string, std::string> m_kvDB;

    std::unordered_map<int, LockQueue<Op>*> waitApplyCh;  // index(raft) -> chan  //？？？字段含义   waitApplyCh是一个map，键是int，值是Op类型的管道

    std::unordered_map<string, int> m_lastRequestId;   // clientid -> requestID  //一个kV服务器可能连接多个client

    // last SnapShot point , raftIndex
    int m_lastSnapShotRaftLogIndex;


public:
    KvServer() = delete;
    KvServer(int me,int maxraftstate,std::string nodeInforFileName);
    void StartKVServer();
    void DprintfKVDB();

    void ExecuteAppendOpOnKVDB(Op op);

    void ExecuteGetOpOnKVDB(Op op,string* value,bool* exist);

    void ExecutePutOpOnKVDB(Op op);

    void Get(const GetArgs *args, GetReply *reply);   //将 GetArgs 改为rpc调用的，因为是远程客户端，即服务器宕机对客户端来说是无感的

    void GetCommandFromRaft(ApplyMsg message);

    bool ifRequestDuplicate(string ClientId, int RequestId);
    // clerk 使用RPC远程调用
    void PutAppend(const PutAppendArgs *args, PutAppendReply *reply);

////一直等待raft传来的applyCh
    void ReadRaftApplyCommandLoop();

    void ReadSnapShotToInstall(string snapshot);

    bool SendMessageToWaitChan( const Op& op,int raftIndex );
    // 检查是否需要制作快照，需要的话就向raft之下制作快照
    void IfNeedToSendSnapShotCommand(int raftIndex ,int  proportion );
    // Handler the SnapShot from kv.rf.applyCh
    void GetSnapShotFromRaft(ApplyMsg message );


    string MakeSnapShot();

public:   //for rpc
    virtual void PutAppend(google::protobuf::RpcController *controller,
                           const ::mprrpc::PutAppendArgs *request,
                           ::mprrpc::PutAppendReply *response,
                           ::google::protobuf::Closure *done) override;

    virtual void Get(google::protobuf::RpcController *controller,
                     const ::mprrpc::GetArgs *request,
                     ::mprrpc::GetReply *response,
                     ::google::protobuf::Closure *done) override;



    /////////////////serialiazation start ///////////////////////////////
    //notice ： func serialize
private:

    friend class boost::serialization::access;

    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)   //这里面写需要序列话和反序列化的字段
    {
        ar & m_kvDB;
        ar & m_lastRequestId;
    }
    std::string getSnapshotData(){
        std::stringstream ss;
        boost::archive::text_oarchive oa(ss);
        oa<< *this;
        return ss.str();
    }

    void parseFromString(const string& str){
        std::stringstream ss(str);
        boost::archive::text_iarchive ia(ss);
        ia>>*this;
    }

    /////////////////serialiazation end ///////////////////////////////
};







#endif //SKIP_LIST_ON_RAFT_KVSERVER_H
