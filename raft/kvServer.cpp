#include "kvServer.h"


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
    if(m_kvDB.find(op.Key) != m_kvDB.end()){
        m_kvDB[op.Key]  = m_kvDB[op.Key] + op.Value;
    } else{
        m_kvDB.insert(std::make_pair(op.Key,op.Value));
    }
    m_lastRequestId[op.ClientId] = op.RequestId;
    m_mtx.unlock();


//    DPrintf("[KVServerExeAPPEND-----]ClientId :%d ,RequestID :%d ,Key : %v, value : %v", op.ClientId, op.RequestId, op.Key, op.Value)
    DprintfKVDB();


}

void KvServer::ExecuteGetOpOnKVDB(Op op,string* value,bool* exist) {
    m_mtx.lock();
    *value = "";
    *exist = false;
    if(m_kvDB.find(op.Key) != m_kvDB.end()){
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


    int raftIndex = -1;int _ = -1;bool isLeader = false;
    m_raftNode->Start(op,&raftIndex,&_,&isLeader);//raftIndex：raft预计的logIndex ，虽然是预计，但是正确情况下是准确的，op的具体内容对raft来说 是隔离的

    if(!isLeader){
        reply->set_err(ErrWrongLeader);
        return ;
    }


    // create waitForCh
    m_mtx.lock();

    if(waitApplyCh.find(raftIndex) == waitApplyCh.end()){
        waitApplyCh.insert(std::make_pair(raftIndex,new LockQueue<Op>()));
    }
    auto  chForRaftIndex = waitApplyCh[raftIndex];

    m_mtx.unlock(); //直接解锁，等待任务执行完成，不能一直拿锁等待


    // timeout
    Op raftCommitOp;

    if(!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT,&raftCommitOp)){

//        DPrintf("[GET TIMEOUT!!!]From Client %d (Request %d) To Server %d, key %v, raftIndex %d", args.ClientId, args.RequestId, kv.me, op.Key, raftIndex)
        // todo 2023年06月01日
        int  _ = -1; bool isLeader = false;
        m_raftNode->GetState(&_,&isLeader);

        if (ifRequestDuplicate(op.ClientId, op.RequestId) && isLeader) {
                    //如果超时，代表raft集群不保证已经commitIndex该日志，但是如果是已经提交过的get请求，是可以再执行的。
                    // 不会违反线性一致性
            string value;bool exist = false;
            ExecuteGetOpOnKVDB(op,&value,&exist);
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
    }else{
        //raft已经提交了该command（op），可以正式开始执行了
//        DPrintf("[WaitChanGetRaftApplyMessage<--]Server %d , get Command <-- Index:%d , ClientId %d, RequestId %d, Opreation %v, Key :%v, Value :%v", kv.me, raftIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value)
        //todo 这里还要再次检验的原因：感觉不用检验，因为leader只要正确的提交了，那么这些肯定是符合的
        if(raftCommitOp.ClientId == op.ClientId &&raftCommitOp.RequestId == op.RequestId){
            string value;bool exist = false;
            ExecuteGetOpOnKVDB(op,&value,&exist);
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
    auto tmp =  waitApplyCh[raftIndex];
    waitApplyCh.erase(raftIndex);
    delete tmp;
    m_mtx.unlock();

}

void KvServer::GetCommandFromRaft(ApplyMsg message) {

    Op op;
    op.parseFromString(message.Command);




//    DPrintf("[RaftApplyCommand]Server %d , Got Command --> Index:%d , ClientId %d, RequestId %d, Opreation %v, Key :%v, Value :%v",kv.me, message.CommandIndex, op.ClientId, op.RequestId, op.Operation, op.Key, op.Value)
if(message.CommandIndex <= m_lastSnapShotRaftLogIndex){
    return ;
}

    // State Machine (KVServer solute the duplicate problem)
    // duplicate command will not be exed
    if(!ifRequestDuplicate(op.ClientId,op.RequestId)){
        // execute command
        if (op.Operation == "Put") {
            execute


            kv.ExecutePutOpOnKVDB(op)
        }
        if (op.Operation == "Append" ){
            kv.ExecuteAppendOpOnKVDB(op)
        }
        //  kv.lastRequestId[op.ClientId] = op.RequestId  在Executexxx函数里面更新的
    }
    //到这里kvDB已经制作了快照
    if kv.maxraftstate != -1{
        kv.IfNeedToSendSnapShotCommand(message.CommandIndex,9)
        //如果raft的log太大就把制作快照
    }

    // Send message to the chan of op.ClientId
    kv.SendMessageToWaitChan(op,message.CommandIndex)


}
