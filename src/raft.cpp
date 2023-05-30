#include "raft.h"
#include <util.h>
void Raft::AppendEntries1(AppendEntriesArgs *args, AppendEntriesReply *reply) {
    std::lock_guard<std::mutex> locker(m_mtx);
    reply->set_appstate(AppNormal);// 能接收到代表网络是正常的
    // Your code here (2A, 2B).
//	不同的人收到AppendEntries的反应是不同的，要注意无论什么时候收到rpc请求和响应都要检查term


    if (args->term() < m_currentTerm) {
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(-100); // 论文中：让领导人可以及时更新自己
        // DPrintf("[func-AppendEntries-rf{%v}] 拒绝了 因为Leader{%v}的term{%v}< rf{%v}.term{%v}\n", rf.me, args.LeaderId, args.Term, rf.me, rf.currentTerm) 
        return; // 注意从过期的领导人收到消息不要重设定时器
    }
    Defer ec1([this]() -> void { this->persist(); });//由于这个局部变量创建在锁之后，因此执行persist的时候应该也是拿到锁的.
    if (args->term() > m_currentTerm) {
        // 三变 ,防止遗漏，无论什么时候都是三变
        // DPrintf("[func-AppendEntries-rf{%v} ] 变成follower且更新term 因为Leader{%v}的term{%v}> rf{%v}.term{%v}\n", rf.me, args.LeaderId, args.Term, rf.me, rf.currentTerm)
        m_status = Follower;
        m_currentTerm = args->term();
        m_votedFor = -1; // 这里设置成-1有意义，如果突然宕机然后上线理论上是可以投票的
        // 这里可不返回，应该改成让改节点尝试接收日志
        // 如果是领导人和candidate突然转到Follower好像也不用其他操作
        // 如果本来就是Follower，那么其term变化，相当于“不言自明”的换了追随的对象，因为原来的leader的term更小，是不会再接收其消息了
    }
    myAssert(args->term() == m_currentTerm, format("assert {args.Term == rf.currentTerm} fail"));
    // 如果发生网络分区，那么candidate可能会收到同一个term的leader的消息，要转变为Follower，为了和上面，因此直接写
    m_status = Follower; // 这里是有必要的，因为如果candidate收到同一个term的leader的AE，需要变成follower
    // term相等
    m_lastResetElectionTime = now();
    //  DPrintf("[	AppendEntries-func-rf(%v)		] 重置了选举超时定时器\n", rf.me);

    // 不能无脑的从prevlogIndex开始阶段日志，因为rpc可能会延迟，导致发过来的log是很久之前的

    //	那么就比较日志，日志有3种情况
    if (args->prevlogindex() > getLastLogIndex()) {
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(getLastLogIndex() + 1);
        //  DPrintf("[func-AppendEntries-rf{%v}] 拒绝了节点{%v}，因为日志太新,args.PrevLogIndex{%v} > lastLogIndex{%v}，返回值：{%v}\n", rf.me, args.LeaderId, args.PrevLogIndex, rf.getLastLogIndex(), reply)
        return;
    } else if (args->prevlogindex() < m_lastSnapshotIncludeIndex) { // 如果prevlogIndex还没有更上快照
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(m_lastSnapshotIncludeIndex + 1); // todo 如果想直接弄到最新好像不对，因为是从后慢慢往前匹配的，这里不匹配说明后面的都不匹配
        //  DPrintf("[func-AppendEntries-rf{%v}] 拒绝了节点{%v}，因为log太老，返回值：{%v}\n", rf.me, args.LeaderId, reply) return
    }
    //	本机日志有那么长，冲突(same index,different term),截断日志
    // 注意：这里目前当args.PrevLogIndex == rf.lastSnapshotIncludeIndex与不等的时候要分开考虑，可以看看能不能优化这块
    if (matchLog(args->prevlogindex(), args->prevlogterm())) {
        //	todo：	整理logs ，不能直接截断，必须一个一个检查，因为发送来的log可能是之前的，直接截断可能导致“取回”已经在follower日志中的条目
        // 那意思是不是可能会有一段发来的AE中的logs中前半是匹配的，后半是不匹配的，这种应该：1.follower如何处理？ 2.如何给leader回复
        // 3. leader如何处理

        for (int i = 0; i < args->entries_size(); i++) {
            auto log = args->entries(i);
            if (log.logindex() > getLastLogIndex()) {
                m_logs.push_back(log);
            } else {
                // todo ： 这里可以改进为比较对应logIndex位置的term是否相等，term相等就代表匹配
                //  todo：这个地方放出来会出问题,按理说index相同，term相同，log也应该相同才对
                // rf.logs[entry.Index-firstIndex].Term ?= entry.Term

                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() == log.logterm() &&
                    m_logs[getSlicesIndexFromLogIndex(log.logindex())].command() != log.command()
                        ) { //相同位置的log ，其logTerm相等，但是命令却不相同，不符合raft的前向匹配，异常了！
                    myAssert(false,
                             format("[func-AppendEntries-rf{%d}] 两节点logIndex{%d}和term{%d}相同，但是其command{%d:%d}    {%d:%d}却不同！！\n",
                                    m_me, log.logindex(), log.logterm(), m_me,
                                    m_logs[getSlicesIndexFromLogIndex(log.logindex())].command(), args->leaderid(),
                                    log.command()));
                }
                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() != log.logterm()) {
                    m_logs[getSlicesIndexFromLogIndex(log.logindex())] = log;
                }
            }
        }


        // 错误写法like：  rf.shrinkLogsToIndex(args.PrevLogIndex)
        // rf.logs = append(rf.logs, args.Entries...)
        // 因为可能会收到过期的log！！！ 因此这里是大于等于
        myAssert(getLastLogIndex() >= args->prevlogindex() + args->entries_size(),
                 format("[func-AppendEntries1-rf{%d}]rf.getLastLogIndex(){%d} != args.PrevLogIndex{%d}+len(args.Entries){%d}",
                        m_me, getLastLogIndex(), args->prevlogindex(), args->entries_size()))
        // if len(args.Entries) > 0 {
        //	fmt.Printf("[func-AppendEntries  rf:{%v}] ] : args.term:%v, rf.term:%v  ,rf.logs的长度：%v\n", rf.me, args.Term, rf.currentTerm, len(rf.logs))
        // }
        if (args->leadercommit() > m_commitIndex) {
            m_commitIndex = std::min(args->leadercommit(), getLastLogIndex());// 这个地方不能无脑跟上
        }


        // 领导会一次发送完所有的日志
        myAssert(getLastLogIndex() >= m_commitIndex,
                 format("[func-AppendEntries1-rf{%d}]  rf.getLastLogIndex{%d} < rf.commitIndex{%d}", m_me,
                        getLastLogIndex(), m_commitIndex));
        reply->set_success(true);
        reply->set_term(m_currentTerm);

//        DPrintf("[func-AppendEntries-rf{%v}] 接收了来自节点{%v}的log，当前lastLogIndex{%v}，返回值：{%v}\n", rf.me,
//                args.LeaderId, rf.getLastLogIndex(), reply)

        return;
    } else {
        // 优化
        // PrevLogIndex 长度合适，但是不匹配，因此往前寻找 矛盾的term的第一个元素
        // 为什么该term的日志都是矛盾的呢？也不一定都是矛盾的，只是这么优化减少rpc而已
        // ？什么时候term会矛盾呢？很多情况，比如leader接收了日志之后马上就崩溃等等
        reply->set_updatenextindex(args->prevlogindex());

        for (int index = args->prevlogindex(); index >= m_lastSnapshotIncludeIndex; --index) {
            if (getLogTermFromLogIndex(index) != getLogTermFromLogIndex(args->prevlogindex())) {
                reply->set_updatenextindex(index + 1);
                break;
            }
        }
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        // 对UpdateNextIndex待优化  todo  找到符合的term的最后一个
//        DPrintf("[func-AppendEntries-rf{%v}] 拒绝了节点{%v}，因为prevLodIndex{%v}的args.term{%v}不匹配当前节点的logterm{%v}，返回值：{%v}\n",
//                rf.me, args.LeaderId, args.PrevLogIndex, args.PrevLogTerm,
//                rf.logs[rf.getSlicesIndexFromLogIndex(args.PrevLogIndex)].LogTerm, reply)
//        DPrintf("[func-AppendEntries-rf{%v}] 返回值: reply.UpdateNextIndex从{%v}优化到{%v}，优化了{%v}\n", rf.me,
//                args.PrevLogIndex, reply.UpdateNextIndex, args.PrevLogIndex - reply.UpdateNextIndex) // 很多都是优化了0
        return;
    }

    // fmt.Printf("[func-AppendEntries,rf{%v}]:len(rf.logs):%v, rf.commitIndex:%v\n", rf.me, len(rf.logs), rf.commitIndex)
}

void Raft::applierTicker() {
    m_mtx.lock();
    auto applyMsgs = getApplyLogs();
    m_mtx.unlock();
    //使用匿名函数是因为传递管道的时候不用拿锁   todo:好像必须拿锁，因为不拿锁的话如果调用多次applyLog函数，可能会导致应用的顺序不一样
    for (auto &message: applyMsgs) {
        applyChan->Push(message);
    }
    sleepNMilliseconds(ApplyInterval);

}

bool Raft::CondInstallSnapshot(int lastIncludedTerm, int lastIncludedIndex, std::string snapshot) {
    return true;
    //// Your code here (2D).
    //rf.mu.Lock()
    //defer rf.mu.Unlock()
    //DPrintf("{Node %v} service calls CondInstallSnapshot with lastIncludedTerm %v and lastIncludedIndex {%v} to check whether snapshot is still valid in term %v", rf.me, lastIncludedTerm, lastIncludedIndex, rf.currentTerm)
    //// outdated snapshot
    //if lastIncludedIndex <= rf.commitIndex {
    //	return false
    //}
    //
    //lastLogIndex, _ := rf.getLastLogIndexAndTerm()
    //if lastIncludedIndex > lastLogIndex {
    //	rf.logs = make([]LogEntry, 0)
    //} else {
    //	rf.logs = rf.logs[rf.getSlicesIndexFromLogIndex(lastIncludedIndex)+1:]
    //}
    //// update dummy entry with lastIncludedTerm and lastIncludedIndex
    //rf.lastApplied, rf.commitIndex = lastIncludedIndex, lastIncludedIndex
    //
    //rf.persister.Save(rf.persistData(), snapshot)
    //return true
}

void Raft::doElection() {
    m_mtx.lock();

    if (m_status == Leader) {
        //fmt.Printf("[       ticker-func-rf(%v)              ] is a Leader,wait the  lock\n", rf.me)
    }
    //fmt.Printf("[       ticker-func-rf(%v)              ] get the  lock\n", rf.me)

    if (m_status != Leader) {
//                DPrintf("[       ticker-func-rf(%v)              ]  选举定时器到期且不是leader，开始选举 \n", rf.me)
        //当选举的时候定时器超时就必须重新选举，不然没有选票就会一直卡主
        //重竞选超时，term也会增加的
        m_status = Candidate;
        ///开始新一轮的选举
        m_currentTerm += 1;
        m_votedFor = m_me;//即是自己给自己投，也避免candidate给同辈的candidate投
        persist();
        std::shared_ptr<int> votedNum = std::make_shared<int>(1); // 使用 make_shared 函数初始化 !! 亮点
        //	重新设置定时器
        m_lastResetElectionTime = now();
        //	发布RequestVote RPC
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == m_me) {
                continue;
            }
            int lastLogIndex = -1, lastLogTerm = -1;
            getLastLogIndexAndTerm(&lastLogIndex, &lastLogTerm);//获取最后一个log的term和下标

            std::shared_ptr<RequestVoteArgs> requestVoteArgs = std::make_shared<RequestVoteArgs>();
            requestVoteArgs->set_term(m_currentTerm);
            requestVoteArgs->set_candidateid(m_me);
            requestVoteArgs->set_lastlogindex(lastLogIndex);
            requestVoteArgs->set_lastlogterm(lastLogTerm);
            std::shared_ptr<RequestVoteReply> requestVoteReply = std::make_shared<RequestVoteReply>();

            //使用匿名函数执行避免其拿到锁
            //使用 go 语句在每个迭代中都执行一个函数，并将循环变量作为参数传递进去。由于 go 语句会异步地执行函数，因此无法保证每次调用
            //函数时循环变量的值是相同的。这就可能导致函数不符合预期地处理某些参数，因为它们的值可能已经被修改或者还没有更新到期望的值。
            std::thread t(&Raft::sendRequestVote, this, i, requestVoteArgs, requestVoteReply,
                          votedNum); // 创建新线程并执行b函数，并传递参数
                          t.detach();

        }
    }
    m_mtx.unlock();

}

void Raft::doHeartBeat() {
    m_mtx.lock();

    if ( m_status == Leader) {
//                DPrintf("Leader: {%v} 的心跳定时器触发了\n", rf.me)
        auto appendNums = std::make_shared<int>(1); //正确返回的节点的数量

                //对Follower（除了自己外的所有节点发送AE）
                //todo 这里肯定是要修改的，最好使用一个单独的goruntime来负责管理发送log，因为后面的log发送涉及优化之类的
                //最少要单独写一个函数来管理，而不是在这一坨
                for (int i = 0; i < m_peers.size(); i++ ){
                    myAssert(m_nextIndex[i]>=1, format("rf.nextIndex[%d] = {%d}", i, m_nextIndex[i]));
                    //日志压缩加入后要判断是发送快照还是发送AE
                    if(m_nextIndex[i] <= m_lastSnapshotIncludeIndex){
//                        DPrintf("[func-ticker()-rf{%v}]rf.nextIndex[%v] {%v} <= rf.lastSnapshotIncludeIndex{%v},so leaderSendSnapShot", rf.me, i, rf.nextIndex[i], rf.lastSnapshotIncludeIndex)
                        std::thread t(&Raft::leaderSendSnapShot, this, i); // 创建新线程并执行b函数，并传递参数
                        t.detach();
                        continue;
                    }
                    //构造发送值
                    int preLogIndex = -1 ;int PrevLogTerm = -1;
                    getPrevLogInfo(i,&preLogIndex,&PrevLogTerm);
                    std::shared_ptr<AppendEntriesArgs> appendEntriesArgs= std::make_shared<AppendEntriesArgs>();
                    appendEntriesArgs->set_term(m_currentTerm);
                    appendEntriesArgs->set_leaderid(m_me);
                    appendEntriesArgs->set_prevlogindex(preLogIndex);
                    appendEntriesArgs->set_prevlogterm(PrevLogTerm);
                    appendEntriesArgs->clear_entries();
                    appendEntriesArgs->set_leadercommit(m_commitIndex);
                    if (preLogIndex != m_lastSnapshotIncludeIndex) {
                        for (int j = getSlicesIndexFromLogIndex(preLogIndex) + 1; j < m_logs.size(); ++j) {
                            LogEntry *sendEntryPtr = appendEntriesArgs->add_entries();
                            *sendEntryPtr = m_logs[j];  //=是可以点进去的，可以点进去看下protobuf如何重写这个的
                        }
                    } else {
                        for (auto& item:m_logs) {
                            LogEntry *sendEntryPtr = appendEntriesArgs->add_entries();
                            *sendEntryPtr = item;  //=是可以点进去的，可以点进去看下protobuf如何重写这个的
                        }
                    }
                    int lastLogIndex = getLastLogIndex();
                    //leader对每个节点发送的日志长短不一，但是都保证从prevIndex发送直到最后
                    myAssert(appendEntriesArgs->prevlogindex()+appendEntriesArgs->entries_size() == lastLogIndex,
                             format("appendEntriesArgs.PrevLogIndex{%d}+len(appendEntriesArgs.Entries){%d} != lastLogIndex{%d}", appendEntriesArgs->prevlogindex(),appendEntriesArgs->entries_size(),lastLogIndex));
                    //构造返回值
                    const std::shared_ptr<AppendEntriesReply> appendEntriesReply = std::make_shared<AppendEntriesReply>();
                    appendEntriesReply->set_appstate(Disconnected);

                    std::thread t(&Raft::sendAppendEntries, this, i, appendEntriesArgs, appendEntriesReply,
                                  appendNums); // 创建新线程并执行b函数，并传递参数
                    t.detach();

                }
                m_lastResetHearBeatTime = now(); //leader发送心跳，就不是随机时间了
    }
    m_mtx.unlock();
}

void Raft::electionTimeOutTicker() {
    // Your code here (2A)
    // Check if a Leader election should be started.
    while(true){
        m_mtx.lock();
        auto nowTime =  now();
        auto suitableSleepTime = getRandomizedElectionTimeout() + m_lastResetElectionTime- nowTime;
        if(suitableSleepTime.count() < 1){
            suitableSleepTime = std::chrono::milliseconds (1);
        }
        std::this_thread::sleep_for(suitableSleepTime);
        if((m_lastResetElectionTime - nowTime).count()>0){  //说明睡眠的这段时间有重置定时器，那么就没有超时，再次睡眠
            continue;
        }
        doElection();
    }

}

std::vector<ApplyMsg> Raft::getApplyLogs() {

    std::vector<ApplyMsg> applyMsgs ;
    myAssert(m_commitIndex <= getLastLogIndex(), format("[func-getApplyLogs-rf{%d}] commitIndex{%d} >getLastLogIndex{%d}", m_me, m_commitIndex, getLastLogIndex()));

    while(m_lastApplied<m_commitIndex){
        m_lastApplied++;
        myAssert(m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].logindex()==m_lastApplied,
                 format("rf.logs[rf.getSlicesIndexFromLogIndex(rf.lastApplied)].LogIndex{%d} != rf.lastApplied{%d} ", m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].logindex(), m_lastApplied));
        ApplyMsg  applyMsg ;
        applyMsg.CommandValid = true;
        applyMsg.SnapshotValid = false;
        applyMsg.Command = m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].command();
        applyMsg.CommandIndex = m_lastApplied;
        applyMsgs.emplace_back( applyMsg);
//        DPrintf("[	applyLog func-rf{%v}	] apply Log,logIndex:%v  ，logTerm：{%v},command：{%v}\n", rf.me, rf.lastApplied, rf.logs[rf.getSlicesIndexFromLogIndex(rf.lastApplied)].LogTerm, rf.logs[rf.getSlicesIndexFromLogIndex(rf.lastApplied)].Command)
    }
    return applyMsgs;
}
// 获取新命令应该分配的Index
int Raft::getNewCommandIndex() {
    //	如果len(logs)==0,就为快照的index+1，否则为log最后一个日志+1
    auto lastLogIndex = getLastLogIndex();
    return lastLogIndex+1;
}
// getPrevLogInfo
// leader调用，传入：服务器index，传出：发送的AE的preLogIndex和PrevLogTerm
void Raft::getPrevLogInfo(int server, int *preIndex, int *preTerm) {
    //logs长度为0返回0,0，不是0就根据nextIndex数组的数值返回
    if(m_nextIndex[server] == m_lastSnapshotIncludeIndex + 1){ //要发送的日志是第一个日志，因此直接返回m_lastSnapshotIncludeIndex和m_lastSnapshotIncludeTerm
        *preIndex = m_lastSnapshotIncludeIndex;
        *preTerm = m_lastSnapshotIncludeTerm;
        return;
    }
    auto nextIndex = m_nextIndex[server];
    *preIndex = nextIndex -1;
    *preTerm = m_logs[getSlicesIndexFromLogIndex(*preIndex)].logterm();
}
// GetState return currentTerm and whether this server
// believes it is the Leader.
void Raft::GetState(int *term, bool *isLeader) {
    m_mtx.lock();
    Defer ec1([this]()->void {   //todo 暂时不清楚会不会导致死锁
       m_mtx.unlock();
    });

    // Your code here (2A).
    *term = m_currentTerm;
    *isLeader = (m_status == Leader);

}

void Raft::InstallSnapshot(InstallSnapshotRequest *args, InstallSnapshotResponse *reply) {
    m_mtx.lock();
    Defer ec1([this]()->void {
       m_mtx.unlock();
    });
    if(args->term()<m_currentTerm){
        reply->set_term(m_currentTerm);
//        DPrintf("[func-InstallSnapshot-rf{%v}] leader{%v}.term{%v}<rf{%v}.term{%v} ", rf.me, args.LeaderId, args.Term, rf.me, rf.currentTerm)

        return;
    }
    if(args->term()>m_currentTerm){ //后面两种情况都要接收日志
        m_currentTerm = args->term();
        m_votedFor = -1;
        m_status = Follower;
        persist();
    }
    m_status = Follower;
    m_lastResetElectionTime = now();
    // outdated snapshot
    if(args->lastsnapshotincludeindex() <= m_lastSnapshotIncludeIndex){
//        DPrintf("[func-InstallSnapshot-rf{%v}] leader{%v}.LastSnapShotIncludeIndex{%v} <= rf{%v}.lastSnapshotIncludeIndex{%v} ", rf.me, args.LeaderId, args.LastSnapShotIncludeIndex, rf.me, rf.lastSnapshotIncludeIndex)
        return;
    }
    //截断日志，修改commitIndex和lastApplied
    //截断日志包括：日志长了，截断一部分，日志短了，全部清空，其实两个是一种情况
    //但是由于现在getSlicesIndexFromLogIndex的实现，不能传入不存在logIndex，否则会panic
    auto lastLogIndex  = getLastLogIndex();

    if (lastLogIndex > args->lastsnapshotincludeindex()) {
        m_logs.erase(m_logs.begin(),m_logs.begin()+ getSlicesIndexFromLogIndex(args->lastsnapshotincludeindex())+1);

    } else {
        m_logs.clear();
    }
    m_commitIndex = std::max(m_commitIndex,args->lastsnapshotincludeindex());
    m_lastApplied = std::max(m_lastApplied,args->lastsnapshotincludeindex());
    m_lastSnapshotIncludeIndex = args->lastsnapshotincludeindex();
    m_lastSnapshotIncludeTerm = args->lastsnapshotincludeterm();

    reply->set_term(m_currentTerm);
    ApplyMsg msg;
    msg.SnapshotValid = true;
    msg .Snapshot = args->data();
    msg.SnapshotTerm = args->lastsnapshotincludeterm();
    msg.SnapshotIndex = args->lastsnapshotincludeindex();

    applyChan->Push(msg);
    std::thread t(&Raft::pushMsgToKvServer, this, msg); // 创建新线程并执行b函数，并传递参数
    t.detach();
    //看下这里能不能再优化
//    DPrintf("[func-InstallSnapshot-rf{%v}] receive snapshot from {%v} ,LastSnapShotIncludeIndex ={%v} ", rf.me, args.LeaderId, args.LastSnapShotIncludeIndex)
    //持久化
    m_persister->Save(persistData(),args->data());

}

void Raft::pushMsgToKvServer(ApplyMsg msg) {
    applyChan->Push(msg);
}
//todo : 等待实现，因为序列化稍微比较复杂.
std::string Raft::persistData() {
    return std::string();
}

void Raft::leaderHearBeatTicker() {
    while(true){
        // Your code here (2A)
        auto nowTime  = now();
        m_mtx.lock();

        auto suitableSleepTime = std::chrono::milliseconds(HeartBeatTimeout) + m_lastResetHearBeatTime- nowTime;
        m_mtx.unlock();
        if (suitableSleepTime.count() < 1 ){
            suitableSleepTime = std::chrono::milliseconds (1);
        }
        std::this_thread::sleep_for(suitableSleepTime);
        if((m_lastResetHearBeatTime - nowTime).count()>0){ //说明睡眠的这段时间有重置定时器，那么就没有超时，再次睡眠
            continue;
        }
        doHeartBeat();
    }

}

void Raft::leaderSendSnapShot(int server) {

    m_mtx.lock();
    InstallSnapshotRequest args ;
    args.set_leaderid(m_me);
    args.set_term(m_currentTerm);
    args.set_lastsnapshotincludeindex(m_lastSnapshotIncludeIndex);
    args.set_lastsnapshotincludeterm(m_lastSnapshotIncludeTerm);
    args.set_data(m_persister->ReadSnapshot());

    InstallSnapshotResponse reply;
    m_mtx.unlock();
    m_peers[server]->InstallSnapshot(&args,&reply);
    //todo 2023-05-30 写到这了
    ok := rf.peers[server].Call("Raft.InstallSnapshot", args, reply)
    rf.mu.Lock()
    defer rf.mu.Unlock()
    if !ok {
                return
        }
    if rf.status != Leader || rf.currentTerm != args.Term {
        return //中间释放过锁，可能状态已经改变了
    }
    //	无论什么时候都要判断term
    if reply.Term > rf.currentTerm {
        //三变
        rf.currentTerm = reply.Term
        rf.votedFor = -1
        rf.status = Follower
        rf.persist()
        rf.lastResetElectionTime = time.Now()
        return
    }
    rf.matchIndex[server] = args.LastSnapShotIncludeIndex
    rf.nextIndex[server] = rf.matchIndex[server] + 1

}


