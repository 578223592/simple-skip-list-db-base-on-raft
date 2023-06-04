#include "./include/kvServer.h"   //todo  ： 这里为什么只能用相对路径

void KvServer::DprintfKVDB() {


    if (!Debug) {
        return;
    }
    m_mtx.lock();
    Defer ec1([this]() -> void {
        for (auto item: m_kvDB) {
//           DPrintf("[DBInfo ----]Key : %v, Value : %v", key, value)

        }
    });

}

void KvServer::ExecuteAppendOpOnKVDB(Op op) {

    //if op.IfDuplicate {   //get请求是可重复执行的，因此可以不用判复
    //	return
    //}
    m_mtx.lock();
    if (m_kvDB.find(op.Key) != m_kvDB.end()) {
        m_kvDB[op.Key] = m_kvDB[op.Key] + op.Value;
    } else {
        m_kvDB.insert(std::make_pair(op.Key, op.Value));
    }
    m_lastRequestId[op.ClientId] = op.RequestId;
    m_mtx.unlock();


//    DPrintf("[KVServerExeAPPEND-----]ClientId :%d ,RequestID :%d ,Key : %v, value : %v", op.ClientId, op.RequestId, op.Key, op.Value)
    DprintfKVDB();


}

void KvServer::ExecuteGetOpOnKVDB(Op op, string *value, bool *exist) {
    m_mtx.lock();
    *value = "";
    *exist = false;
    if (m_kvDB.find(op.Key) != m_kvDB.end()) {
        *exist = true;
        *value = m_kvDB[op.Key];
    }
    m_lastRequestId[op.ClientId] = op.RequestId;
    m_mtx.unlock();


    if (*exist) {
//                DPrintf("[KVServerExeGET----]ClientId :%d ,RequestID :%d ,Key : %v, value :%v", op.ClientId, op.RequestId, op.Key, value)
    } else {
//        DPrintf("[KVServerExeGET----]ClientId :%d ,RequestID :%d ,Key : %v, But No KEY!!!!", op.ClientId, op.RequestId, op.Key)
    }
    DprintfKVDB();

}

void KvServer::ExecutePutOpOnKVDB(Op op) {
    m_mtx.lock();
    m_kvDB[op.Key] = op.Value;
    m_lastRequestId[op.ClientId] = op.RequestId;
    m_mtx.unlock();



//    DPrintf("[KVServerExePUT----]ClientId :%d ,RequestID :%d ,Key : %v, value : %v", op.ClientId, op.RequestId, op.Key, op.Value)
    DprintfKVDB();

}

// 处理来自clerk的Get RPC
void KvServer::Get(const GetArgs *args, GetReply *reply) {
    Op op;
    op.Operation = "Get";
    op.Key = args->key();
    op.Value = "";
    op.ClientId = args->clientid();
    op.RequestId = args->requestid();


    int raftIndex = -1;
    int _ = -1;
    bool isLeader = false;
    m_raftNode->Start(op, &raftIndex, &_, &isLeader);//raftIndex：raft预计的logIndex ，虽然是预计，但是正确情况下是准确的，op的具体内容对raft来说 是隔离的

    if (!isLeader) {
        reply->set_err(ErrWrongLeader);
        return;
    }


    // create waitForCh
    m_mtx.lock();

    if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
        waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
    }
    auto chForRaftIndex = waitApplyCh[raftIndex];

    m_mtx.unlock(); //直接解锁，等待任务执行完成，不能一直拿锁等待


    // timeout
    Op raftCommitOp;

    if (!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {

//        DPrintf("[GET TIMEOUT!!!]From Client %d (Request %d) To Server %d, key %v, raftIndex %d", args.ClientId, args.RequestId, kv.me, op.Key, raftIndex)
        // todo 2023年06月01日
        int _ = -1;
        bool isLeader = false;
        m_raftNode->GetState(&_, &isLeader);

        if (ifRequestDuplicate(op.ClientId, op.RequestId) && isLeader) {
            //如果超时，代表raft集群不保证已经commitIndex该日志，但是如果是已经提交过的get请求，是可以再执行的。
            // 不会违反线性一致性
            string value;
            bool exist = false;
            ExecuteGetOpOnKVDB(op, &value, &exist);
            if (exist) {
                reply->set_err(OK);
                reply->set_value(value);
            } else {
                reply->set_err(ErrNoKey);
                reply->set_value("");

            }
        } else {
            reply->set_err(ErrWrongLeader);  //返回这个，其实就是让clerk换一个节点重试
        }
    } else {
        //raft已经提交了该command（op），可以正式开始执行了
//        DPrintf("[WaitChanGetRaftApplyMessage<--]Server %d , get Command <-- Index:%d , ClientId %d, RequestId %d, Opreation %v, Key :%v, Value :%v", kv.me, raftIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value)
        //todo 这里还要再次检验的原因：感觉不用检验，因为leader只要正确的提交了，那么这些肯定是符合的
        if (raftCommitOp.ClientId == op.ClientId && raftCommitOp.RequestId == op.RequestId) {
            string value;
            bool exist = false;
            ExecuteGetOpOnKVDB(op, &value, &exist);
            if (exist) {
                reply->set_err(OK);
                reply->set_value(value);
            } else {
                reply->set_err(ErrNoKey);
                reply->set_value("");
            }
        } else {
            reply->set_err(ErrWrongLeader);
//            DPrintf("[GET ] 不满足：raftCommitOp.ClientId{%v} == op.ClientId{%v} && raftCommitOp.RequestId{%v} == op.RequestId{%v}", raftCommitOp.ClientId, op.ClientId, raftCommitOp.RequestId, op.RequestId)
        }
    }
    m_mtx.lock();
    auto tmp = waitApplyCh[raftIndex];
    waitApplyCh.erase(raftIndex);
    delete tmp;
    m_mtx.unlock();

}

void KvServer::GetCommandFromRaft(ApplyMsg message) {

    Op op;
    op.parseFromString(message.Command);




//    DPrintf("[RaftApplyCommand]Server %d , Got Command --> Index:%d , ClientId %d, RequestId %d, Opreation %v, Key :%v, Value :%v",kv.me, message.CommandIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value)
    if (message.CommandIndex <= m_lastSnapShotRaftLogIndex) {
        return;
    }

    // State Machine (KVServer solute the duplicate problem)
    // duplicate command will not be exed
    if (!ifRequestDuplicate(op.ClientId, op.RequestId)) {
        // execute command
        if (op.Operation == "Put") {
            ExecutePutOpOnKVDB(op);

        }
        if (op.Operation == "Append") {
            ExecuteAppendOpOnKVDB(op);
        }
        //  kv.lastRequestId[op.ClientId] = op.RequestId  在Executexxx函数里面更新的
    }
    //到这里kvDB已经制作了快照
    if (m_maxRaftState != -1) {
        IfNeedToSendSnapShotCommand(message.CommandIndex, 9);
        //如果raft的log太大（大于指定的比例）就把制作快照
    }

    // Send message to the chan of op.ClientId
    SendMessageToWaitChan(op, message.CommandIndex);

}

bool KvServer::ifRequestDuplicate(string ClientId, int RequestId) {
    m_mtx.lock();
    Defer ec1([this]() -> void {
        m_mtx.unlock();
    });
    if (m_lastRequestId.find(ClientId) == m_lastRequestId.end()) {
        return false;
        // todo :不存在这个client就创建
    }
    return RequestId <= m_lastRequestId[ClientId];
}

void KvServer::PutAppend(const PutAppendArgs *args, PutAppendReply *reply) {
    Op op;
    op.Operation = args->op();
    op.Key = args->key();
    op.Value = args->value();
    op.ClientId = args->clientid();
    op.RequestId = args->requestid();
    int raftIndex = -1;
    int _ = -1;
    bool isleader = false;

    m_raftNode->Start(op, &raftIndex, &_, &isleader);


    if (!isleader) {
        reply->set_err(ErrWrongLeader);


        return;
    }
//    DPrintf("[PUTAPPEND StartToRaft]From Client %d (Request %d) To Server %d, key %v, raftIndex %d",args.ClientId,args.RequestId, kv.me, op.Key, raftIndex)
    m_mtx.lock();
    if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
        waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
    }
    auto chForRaftIndex = waitApplyCh[raftIndex];


    m_mtx.unlock(); //直接解锁，等待任务执行完成，不能一直拿锁等待


    // timeout
    Op raftCommitOp;

    if (!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {
//        DPrintf("[TIMEOUT PUTAPPEND !!!!]Server %d , get Command <-- Index:%d , ClientId %d, RequestId %d, Opreation %v, Key :%v, Value :%v",kv.me, raftIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value)

        if (ifRequestDuplicate(op.ClientId, op.RequestId)) {
            reply->set_err(OK);// 超时了,但因为是重复的请求，返回ok，实际上就算没有超时，在真正执行的时候也要判断是否重复
        } else {

            reply->set_err(ErrWrongLeader);   ///这里返回这个的目的让clerk重新尝试
        }
    } else {
//        DPrintf("[WaitChanGetRaftApplyMessage<--]Server %d , get Command <-- Index:%d , ClientId %d, RequestId %d, Opreation %v, Key :%v, Value :%v",kv.me, raftIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value)
        if (raftCommitOp.ClientId == op.ClientId &&
            raftCommitOp.RequestId == op.RequestId) {   //可能发生leader的变更导致日志被覆盖，因此必须检查
            reply->set_err(OK);
        } else {
            reply->set_err(ErrWrongLeader);

        }
    }

    m_mtx.lock();

    auto tmp = waitApplyCh[raftIndex];
    waitApplyCh.erase(raftIndex);
    delete tmp;
    m_mtx.unlock();

}

void KvServer::ReadRaftApplyCommandLoop() {
    while (true) {
        //如果只操作applyChan不用拿锁，因为applyChan自己带锁
        auto message = applyChan->Pop();  //阻塞弹出
        // listen to every command applied by its raft ,delivery to relative RPC Handler

        if (message.CommandValid) {
            GetCommandFromRaft(message);
        }
        if (message.SnapshotValid) {
            GetSnapShotFromRaft(message);
        }
    }
}

//raft会与persist层交互，kvserver层也会，因为kvserver层开始的时候需要恢复kvdb的状态
// 关于快照raft层与persist的交互：保存kvserver传来的snapshot；生成leaderInstallSnapshot RPC的时候也需要读取snapshot；
// 因此snapshot的具体格式是由kvserver层来定的，raft只负责传递这个东西
// snapShot里面包含kvserver需要维护的persist_lastRequestId 以及kvDB真正保存的数据persist_kvdb
void KvServer::ReadSnapShotToInstall(string snapshot) {
    if (snapshot.empty()) { // bootstrap without any state?
        return;
    }
    parseFromString(snapshot);

//    r := bytes.NewBuffer(snapshot)
//    d := labgob.NewDecoder(r)
//
//    var persist_kvdb map[string]string  //理应快照
//    var persist_lastRequestId map[int64]int //快照这个为了维护线性一致性
//
//    if d.Decode(&persist_kvdb) != nil || d.Decode(&persist_lastRequestId) != nil {
//                DPrintf("KVSERVER %d read persister got a problem!!!!!!!!!!",kv.me)
//        } else {
//        kv.kvDB = persist_kvdb
//        kv.lastRequestId = persist_lastRequestId
//    }
}

bool KvServer::SendMessageToWaitChan(const Op &op, int raftIndex) {
    m_mtx.lock();
    Defer ec1([this]() -> void {
        m_mtx.unlock();
    });
    if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
        return false;
    }
//    DPrintf("[RaftApplyMessageSendToWaitChan-->]Server %d , Send Command --> Index:%d , ClientId %d, RequestId %d, Opreation %v, Key :%v, Value :%v",kv.me, raftIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value)
    waitApplyCh[raftIndex]->Push(op);

    return true;
}

void KvServer::IfNeedToSendSnapShotCommand(int raftIndex, int proportion) {
    if (m_raftNode->GetRaftStateSize() > m_maxRaftState / 10.0) {
        // Send SnapShot Command
        auto snapshot = MakeSnapShot();
        m_raftNode->Snapshot(raftIndex, snapshot);

    }
}

void KvServer::GetSnapShotFromRaft(ApplyMsg message) {
    m_mtx.lock();
    Defer ec1([this]() -> void {
        m_mtx.unlock();
    });
    if (m_raftNode->CondInstallSnapshot(message.SnapshotTerm, message.SnapshotIndex, message.Snapshot)) {
        ReadSnapShotToInstall(message.Snapshot);
        m_lastSnapShotRaftLogIndex = message.SnapshotIndex;
    }


}

string KvServer::MakeSnapShot() {
    m_mtx.lock();
    Defer ec1([this]() -> void {
        m_mtx.unlock();
    });
    string snapshotData = getSnapshotData();
    return snapshotData;
}

void KvServer::PutAppend(google::protobuf::RpcController *controller, const ::mprrpc::PutAppendArgs *request,
                         ::mprrpc::PutAppendReply *response, ::google::protobuf::Closure *done) {
    PutAppend(request,response);
    done->Run();
}

void KvServer::Get(google::protobuf::RpcController *controller, const ::mprrpc::GetArgs *request,
                   ::mprrpc::GetReply *response, ::google::protobuf::Closure *done) {
    Get(request,response);
    done->Run();
}

KvServer::KvServer(int me, int maxraftstate,std::string nodeInforFileName) {
    std::shared_ptr<Persister> persister = make_shared<Persister>();

    m_me  = me;
    m_maxRaftState = maxraftstate;

    applyChan = make_shared<LockQueue<ApplyMsg>>() ;
    Raft * raftNodeTmpPrt = new Raft();

    ////////////////开启rpc接受功能
    std::thread t([raftNodeTmpPrt]()->void{
        // provider是一个rpc网络服务对象。把UserService对象发布到rpc节点上
        RpcProvider provider;
        provider.NotifyService(raftNodeTmpPrt);

        // 启动一个rpc服务发布节点   Run以后，进程进入阻塞状态，等待远程的rpc调用请求
        provider.Run();
    });
    t.detach();

    ////开启rpc远程调用能力，需要注意必须要保证所有节点都开启rpc接受功能之后才能开启rpc远程调用能力
    ////这里使用睡眠一分钟来保证
    std::cout<<"raftServer node:"<<m_me<<" start to sleep to wait all ohter raftnode start!!!!"<<std::endl;
    sleep(60);
    std::cout<<"raftServer node:"<<m_me<<" wake up!!!!"<<std::endl;
    //获取所有raft节点ip、port ，并进行连接  ,要排除自己
    MprpcConfig config;
    config.LoadConfigFile(nodeInforFileName.c_str());
    std::vector<std::pair<std::string,short>> ipPortVt;
    for (int i = 0; i < INT_MAX - 1 ; ++i) {
        std::string node = "node" + std::to_string(i);

        std::string nodeIp = config.Load(node+"ip");
        std::string nodePortStr = config.Load(node+"port");
        if(nodeIp.empty()){
            break;
        }
        ipPortVt.emplace_back(nodeIp, atoi(nodePortStr.c_str()));   //沒有atos方法，可以考慮自己实现
    }
    std::vector<shared_ptr< RaftRpc >> servers;
    //进行连接
    for(int i = 0;i<ipPortVt.size();++i){
        if(i == m_me){
            servers.push_back(nullptr);
            continue;
        }
        std::string ip = ipPortVt[i].first; short port = ipPortVt[i].second;
        auto* rpc = new RaftRpc(ip,port);
        servers.push_back(std::shared_ptr<RaftRpc>(rpc));
    }

    raftNodeTmpPrt->init(servers,m_me,persister,applyChan);//kv的server直接与raft通信，但kv不直接与raft通信，所以需要把ApplyMsg的chan传递下去用于通信，两者的persist也是共用的

    //////////////////////////////////

    // You may need initialization code here.
    m_kvDB;  //kvdb初始化
    waitApplyCh;
    m_lastRequestId;
    auto snapshot = persister->ReadSnapshot();
    if(!snapshot.empty()){
        ReadSnapShotToInstall(snapshot);
    }
    std::thread t2(&KvServer::ReadRaftApplyCommandLoop, this); //马上向其他节点宣告自己就是leader
    t2.detach();

}



