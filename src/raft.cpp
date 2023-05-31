#include "raft.h"
#include <util.h>

void Raft::AppendEntries1(const  AppendEntriesArgs *args,  AppendEntriesReply *reply) {
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
                        m_me, getLastLogIndex(), args->prevlogindex(), args->entries_size()));
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

            std::thread t(&Raft::sendRequestVote, this, i, requestVoteArgs, requestVoteReply,
                          votedNum); // 创建新线程并执行b函数，并传递参数
            t.detach();

        }
    }
    m_mtx.unlock();

}

void Raft::doHeartBeat() {
    m_mtx.lock();

    if (m_status == Leader) {
//                DPrintf("Leader: {%v} 的心跳定时器触发了\n", rf.me)
        auto appendNums = std::make_shared<int>(1); //正确返回的节点的数量

        //对Follower（除了自己外的所有节点发送AE）
        //todo 这里肯定是要修改的，最好使用一个单独的goruntime来负责管理发送log，因为后面的log发送涉及优化之类的
        //最少要单独写一个函数来管理，而不是在这一坨
        for (int i = 0; i < m_peers.size(); i++) {
            myAssert(m_nextIndex[i] >= 1, format("rf.nextIndex[%d] = {%d}", i, m_nextIndex[i]));
            //日志压缩加入后要判断是发送快照还是发送AE
            if (m_nextIndex[i] <= m_lastSnapshotIncludeIndex) {
//                        DPrintf("[func-ticker()-rf{%v}]rf.nextIndex[%v] {%v} <= rf.lastSnapshotIncludeIndex{%v},so leaderSendSnapShot", rf.me, i, rf.nextIndex[i], rf.lastSnapshotIncludeIndex)
                std::thread t(&Raft::leaderSendSnapShot, this, i); // 创建新线程并执行b函数，并传递参数
                t.detach();
                continue;
            }
            //构造发送值
            int preLogIndex = -1;
            int PrevLogTerm = -1;
            getPrevLogInfo(i, &preLogIndex, &PrevLogTerm);
            std::shared_ptr<AppendEntriesArgs> appendEntriesArgs = std::make_shared<AppendEntriesArgs>();
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
                for (auto &item: m_logs) {
                    LogEntry *sendEntryPtr = appendEntriesArgs->add_entries();
                    *sendEntryPtr = item;  //=是可以点进去的，可以点进去看下protobuf如何重写这个的
                }
            }
            int lastLogIndex = getLastLogIndex();
            //leader对每个节点发送的日志长短不一，但是都保证从prevIndex发送直到最后
            myAssert(appendEntriesArgs->prevlogindex() + appendEntriesArgs->entries_size() == lastLogIndex,
                     format("appendEntriesArgs.PrevLogIndex{%d}+len(appendEntriesArgs.Entries){%d} != lastLogIndex{%d}",
                            appendEntriesArgs->prevlogindex(), appendEntriesArgs->entries_size(), lastLogIndex));
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
    while (true) {
        m_mtx.lock();
        auto nowTime = now();
        auto suitableSleepTime = getRandomizedElectionTimeout() + m_lastResetElectionTime - nowTime;
        if (suitableSleepTime.count() < 1) {
            suitableSleepTime = std::chrono::milliseconds(1);
        }
        std::this_thread::sleep_for(suitableSleepTime);
        if ((m_lastResetElectionTime - nowTime).count() > 0) {  //说明睡眠的这段时间有重置定时器，那么就没有超时，再次睡眠
            continue;
        }
        doElection();
    }

}

std::vector<ApplyMsg> Raft::getApplyLogs() {

    std::vector<ApplyMsg> applyMsgs;
    myAssert(m_commitIndex <= getLastLogIndex(),
             format("[func-getApplyLogs-rf{%d}] commitIndex{%d} >getLastLogIndex{%d}", m_me, m_commitIndex,
                    getLastLogIndex()));

    while (m_lastApplied < m_commitIndex) {
        m_lastApplied++;
        myAssert(m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].logindex() == m_lastApplied,
                 format("rf.logs[rf.getSlicesIndexFromLogIndex(rf.lastApplied)].LogIndex{%d} != rf.lastApplied{%d} ",
                        m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].logindex(), m_lastApplied));
        ApplyMsg applyMsg;
        applyMsg.CommandValid = true;
        applyMsg.SnapshotValid = false;
        applyMsg.Command = m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].command();
        applyMsg.CommandIndex = m_lastApplied;
        applyMsgs.emplace_back(applyMsg);
//        DPrintf("[	applyLog func-rf{%v}	] apply Log,logIndex:%v  ，logTerm：{%v},command：{%v}\n", rf.me, rf.lastApplied, rf.logs[rf.getSlicesIndexFromLogIndex(rf.lastApplied)].LogTerm, rf.logs[rf.getSlicesIndexFromLogIndex(rf.lastApplied)].Command)
    }
    return applyMsgs;
}

// 获取新命令应该分配的Index
int Raft::getNewCommandIndex() {
    //	如果len(logs)==0,就为快照的index+1，否则为log最后一个日志+1
    auto lastLogIndex = getLastLogIndex();
    return lastLogIndex + 1;
}

// getPrevLogInfo
// leader调用，传入：服务器index，传出：发送的AE的preLogIndex和PrevLogTerm
void Raft::getPrevLogInfo(int server, int *preIndex, int *preTerm) {
    //logs长度为0返回0,0，不是0就根据nextIndex数组的数值返回
    if (m_nextIndex[server] ==
        m_lastSnapshotIncludeIndex + 1) { //要发送的日志是第一个日志，因此直接返回m_lastSnapshotIncludeIndex和m_lastSnapshotIncludeTerm
        *preIndex = m_lastSnapshotIncludeIndex;
        *preTerm = m_lastSnapshotIncludeTerm;
        return;
    }
    auto nextIndex = m_nextIndex[server];
    *preIndex = nextIndex - 1;
    *preTerm = m_logs[getSlicesIndexFromLogIndex(*preIndex)].logterm();
}

// GetState return currentTerm and whether this server
// believes it is the Leader.
void Raft::GetState(int *term, bool *isLeader) {
    m_mtx.lock();
    Defer ec1([this]() -> void {   //todo 暂时不清楚会不会导致死锁
        m_mtx.unlock();
    });

    // Your code here (2A).
    *term = m_currentTerm;
    *isLeader = (m_status == Leader);

}

void Raft::InstallSnapshot(const InstallSnapshotRequest *args, InstallSnapshotResponse *reply) {
    m_mtx.lock();
    Defer ec1([this]() -> void {
        m_mtx.unlock();
    });
    if (args->term() < m_currentTerm) {
        reply->set_term(m_currentTerm);
//        DPrintf("[func-InstallSnapshot-rf{%v}] leader{%v}.term{%v}<rf{%v}.term{%v} ", rf.me, args.LeaderId, args.Term, rf.me, rf.currentTerm)

        return;
    }
    if (args->term() > m_currentTerm) { //后面两种情况都要接收日志
        m_currentTerm = args->term();
        m_votedFor = -1;
        m_status = Follower;
        persist();
    }
    m_status = Follower;
    m_lastResetElectionTime = now();
    // outdated snapshot
    if (args->lastsnapshotincludeindex() <= m_lastSnapshotIncludeIndex) {
//        DPrintf("[func-InstallSnapshot-rf{%v}] leader{%v}.LastSnapShotIncludeIndex{%v} <= rf{%v}.lastSnapshotIncludeIndex{%v} ", rf.me, args.LeaderId, args.LastSnapShotIncludeIndex, rf.me, rf.lastSnapshotIncludeIndex)
        return;
    }
    //截断日志，修改commitIndex和lastApplied
    //截断日志包括：日志长了，截断一部分，日志短了，全部清空，其实两个是一种情况
    //但是由于现在getSlicesIndexFromLogIndex的实现，不能传入不存在logIndex，否则会panic
    auto lastLogIndex = getLastLogIndex();

    if (lastLogIndex > args->lastsnapshotincludeindex()) {
        m_logs.erase(m_logs.begin(), m_logs.begin() + getSlicesIndexFromLogIndex(args->lastsnapshotincludeindex()) + 1);

    } else {
        m_logs.clear();
    }
    m_commitIndex = std::max(m_commitIndex, args->lastsnapshotincludeindex());
    m_lastApplied = std::max(m_lastApplied, args->lastsnapshotincludeindex());
    m_lastSnapshotIncludeIndex = args->lastsnapshotincludeindex();
    m_lastSnapshotIncludeTerm = args->lastsnapshotincludeterm();

    reply->set_term(m_currentTerm);
    ApplyMsg msg;
    msg.SnapshotValid = true;
    msg.Snapshot = args->data();
    msg.SnapshotTerm = args->lastsnapshotincludeterm();
    msg.SnapshotIndex = args->lastsnapshotincludeindex();

    applyChan->Push(msg);
    std::thread t(&Raft::pushMsgToKvServer, this, msg); // 创建新线程并执行b函数，并传递参数
    t.detach();
    //看下这里能不能再优化
//    DPrintf("[func-InstallSnapshot-rf{%v}] receive snapshot from {%v} ,LastSnapShotIncludeIndex ={%v} ", rf.me, args.LeaderId, args.LastSnapShotIncludeIndex)
    //持久化
    m_persister->Save(persistData(), args->data());

}

void Raft::pushMsgToKvServer(ApplyMsg msg) {
    applyChan->Push(msg);
}

//todo : 等待实现，因为序列化稍微比较复杂.
std::string Raft::persistData() {
    return std::string();
}

void Raft::leaderHearBeatTicker() {
    while (true) {
        // Your code here (2A)
        auto nowTime = now();
        m_mtx.lock();

        auto suitableSleepTime = std::chrono::milliseconds(HeartBeatTimeout) + m_lastResetHearBeatTime - nowTime;
        m_mtx.unlock();
        if (suitableSleepTime.count() < 1) {
            suitableSleepTime = std::chrono::milliseconds(1);
        }
        std::this_thread::sleep_for(suitableSleepTime);
        if ((m_lastResetHearBeatTime - nowTime).count() > 0) { //说明睡眠的这段时间有重置定时器，那么就没有超时，再次睡眠
            continue;
        }
        doHeartBeat();
    }

}

void Raft::leaderSendSnapShot(int server) {

    m_mtx.lock();
    InstallSnapshotRequest args;
    args.set_leaderid(m_me);
    args.set_term(m_currentTerm);
    args.set_lastsnapshotincludeindex(m_lastSnapshotIncludeIndex);
    args.set_lastsnapshotincludeterm(m_lastSnapshotIncludeTerm);
    args.set_data(m_persister->ReadSnapshot());

    InstallSnapshotResponse reply;
    m_mtx.unlock();
    bool ok = m_peers[server]->InstallSnapshot(&args, &reply);
    m_mtx.lock();
    Defer ec1([this]() -> void {
        this->m_mtx.unlock();
    });
    if (!ok) { return; }
    if (m_status != Leader || m_currentTerm != args.term()) {
        return;//中间释放过锁，可能状态已经改变了
    }
    //	无论什么时候都要判断term
    if (reply.term() > m_currentTerm) {
        //三变
        m_currentTerm = reply.term();
        m_votedFor = -1;
        m_status = Follower;
        persist();
        m_lastResetElectionTime = now();
        return;
    }
    m_matchIndex[server] = args.lastsnapshotincludeindex();
    m_nextIndex[server] = m_matchIndex[server] + 1;
}

void Raft::leaderUpdateCommitIndex() {
    m_commitIndex = m_lastSnapshotIncludeIndex;
    //for index := rf.commitIndex+1;index < len(rf.log);index++ {
    //for index := rf.getLastIndex();index>=rf.commitIndex+1;index--{
    for (int index = getLastLogIndex(); index >= m_lastSnapshotIncludeIndex + 1; index--) {
        int sum = 0;
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == m_me) {
                sum += 1;
                continue;
            }
            if (m_matchIndex[i] >= index) {
                sum += 1;
            }
        }

//        !!!只有当前term有新提交的，才会更新commitIndex！！！！
        //log.Printf("lastSSP:%d, index: %d, commitIndex: %d, lastIndex: %d",rf.lastSSPointIndex, index, rf.commitIndex, rf.getLastIndex())
        if (sum >= m_peers.size() / 2 + 1 && getLogTermFromLogIndex(index) == m_currentTerm) {
            m_commitIndex = index;
            break;
        }

    }
//    DPrintf("[func-leaderUpdateCommitIndex()-rf{%v}] Leader %d(term%d) commitIndex %d",rf.me,rf.me,rf.currentTerm,rf.commitIndex)
}

//进来前要保证logIndex是存在的，即≥rf.lastSnapshotIncludeIndex	，而且小于等于rf.getLastLogIndex()
bool Raft::matchLog(int logIndex, int logTerm) {
    myAssert(logIndex >= m_lastSnapshotIncludeIndex && logIndex <= getLastLogIndex(),
             format("不满足：logIndex{%d}>=rf.lastSnapshotIncludeIndex{%d}&&logIndex{%d}<=rf.getLastLogIndex{%d}",
                    logIndex, m_lastSnapshotIncludeIndex, logIndex, getLastLogIndex()));
    return logTerm == getLogTermFromLogIndex(logIndex);
    // if logIndex == rf.lastSnapshotIncludeIndex {
    // 	return logTerm == rf.lastSnapshotIncludeTerm
    // } else {
    // 	return logTerm == rf.logs[rf.getSlicesIndexFromLogIndex(logIndex)].LogTerm
    // }
}

void Raft::persist() {
    // Your code here (2C).
    auto data = persistData();
    m_persister->SaveRaftState(data);
    //fmt.Printf("RaftNode[%d] persist starts, currentTerm[%d] voteFor[%d] log[%v]\n", rf.me, rf.currentTerm, rf.votedFor, rf.logs)
    //fmt.Printf("%v\n", string(data))
}

void Raft::RequestVote( const RequestVoteArgs *args, RequestVoteReply *reply) {


    // Your code here (2A, 2B).
    m_mtx.lock();
    Defer ec2([this]() -> void {
        m_mtx.unlock();
    });
    Defer ec1([this]() -> void { //应该先持久化，再撤销lock
        this->persist();
    });
    //对args的term的三种情况分别进行处理，大于小于等于自己的term都是不同的处理
    //reason: 出现网络分区，该竞选者已经OutOfDate(过时）
    if (args->term() < m_currentTerm) {
        reply->set_term(m_currentTerm);
        reply->set_votestate(Expire);
        reply->set_votegranted(false);
        return;
    }
    //fig2:右下角，如果任何时候rpc请求或者响应的term大于自己的term，更新term，并变成follower
    if (args->term() > m_currentTerm) {
//        DPrintf("[	    func-RequestVote-rf(%v)		] : 变成follower且更新term 因为candidate{%v}的term{%v}> rf{%v}.term{%v}\n ", rf.me, args.CandidateId, args.Term, rf.me, rf.currentTerm)
        m_status = Follower;
        m_currentTerm = args->term();
        m_votedFor = -1;

        //	重置定时器：收到leader的ae，开始选举，透出票
        //这时候更新了term之后，votedFor也要置为-1
    }
    myAssert(args->term() == m_currentTerm,
             format("[func--rf{%d}] 前面校验过args.Term==rf.currentTerm，这里却不等", m_me));
    //	现在节点任期都是相同的(任期小的也已经更新到新的args的term了)
    //	，要检查log的term和index是不是匹配的了
    int lastLogTerm = getLastLogIndex();
    //只有没投票，且candidate的日志的新的程度 ≥ 接受者的日志新的程度 才会授票
    if (UpToDate(args->lastlogindex(), args->lastlogterm()) == false) {
        //args.LastLogTerm < lastLogTerm || (args.LastLogTerm == lastLogTerm && args.LastLogIndex < lastLogIndex) {
        //日志太旧了
        if (args->lastlogterm() < lastLogTerm) {
//                    DPrintf("[	    func-RequestVote-rf(%v)		] : refuse voted rf[%v] ,because candidate_lastlog_term{%v} < lastlog_term{%v}\n", rf.me, args.CandidateId, args.LastLogTerm, lastLogTerm)
        } else {
//            DPrintf("[	    func-RequestVote-rf(%v)		] : refuse voted rf[%v] ,because candidate_log_index{%v} < log_index{%v}\n", rf.me, args.CandidateId, args.LastLogIndex, rf.getLastLogIndex())
        }
        reply->set_term(m_currentTerm);
        reply->set_votestate(Voted);
        reply->set_votegranted(false);


        return;
    }
    //todo ： 啥时候会出现rf.votedFor == args.CandidateId ，就算candidate选举超时再选举，其term也是不一样的呀
//    当因为网络质量不好导致的请求丢失重发就有可能！！！！
    if (m_votedFor != -1 && m_votedFor != args->candidateid()) {
//        DPrintf("[	    func-RequestVote-rf(%v)		] : refuse voted rf[%v] ,because has voted\n", rf.me, args.CandidateId)
        reply->set_term(m_currentTerm);
        reply->set_votestate(Voted);
        reply->set_votegranted(false);

        return;
    } else {
        m_votedFor = args->candidateid();
        m_lastResetElectionTime = now();//认为必须要在投出票的时候才重置定时器，
//        DPrintf("[	    func-RequestVote-rf(%v)		] : voted rf[%v]\n", rf.me, rf.votedFor)
        reply->set_term(m_currentTerm);
        reply->set_votestate(Normal);
        reply->set_votegranted(true);

        return;
    }

}

bool Raft::UpToDate(int index, int term) {
    //lastEntry := rf.log[len(rf.log)-1]


    int lastIndex = -1;int  lastTerm = -1;
    getLastLogIndexAndTerm(&lastIndex,&lastTerm);
    return term > lastTerm || (term == lastTerm && index >= lastIndex);
}

void Raft::getLastLogIndexAndTerm(int *lastLogIndex, int *lastLogTerm) {
    if(m_logs.empty()){
        *lastLogIndex=  m_lastSnapshotIncludeIndex;
        *lastLogTerm = m_lastSnapshotIncludeTerm;
        return ;
    }else {
        *lastLogIndex = m_logs[m_logs.size()-1].logindex();
        *lastLogTerm = m_logs[m_logs.size()-1].logterm();
        return ;
    }
}

int Raft::getLastLogIndex() {
    int lastLogIndex = -1;int _ = -1;
    getLastLogIndexAndTerm(&lastLogIndex,&_);
    return lastLogIndex;
}

int Raft::getLogTermFromLogIndex(int logIndex) {
    myAssert(logIndex >= m_lastSnapshotIncludeIndex,
             format("[func-getSlicesIndexFromLogIndex-rf{%d}]  index{%d} < rf.lastSnapshotIncludeIndex{%d}", m_me, logIndex, m_lastSnapshotIncludeIndex));

    int lastLogIndex = getLastLogIndex();


    myAssert(logIndex <= lastLogIndex,
             format("[func-getSlicesIndexFromLogIndex-rf{%d}]  logIndex{%d} > lastLogIndex{%d}", m_me, logIndex, lastLogIndex));

    if (logIndex ==  m_lastSnapshotIncludeIndex  ) {
        return m_lastSnapshotIncludeTerm;
    } else {
        return m_logs[getSlicesIndexFromLogIndex(logIndex)].logterm();
    }
}

int Raft::GetRaftStateSize() {
    return m_persister->RaftStateSize();

}
// 找到index对应的真实下标位置！！！
// 限制，输入的logIndex必须保存在当前的logs里面（不包含snapshot）
int Raft::getSlicesIndexFromLogIndex(int logIndex) {
    myAssert(logIndex > m_lastSnapshotIncludeIndex,
             format("[func-getSlicesIndexFromLogIndex-rf{%d}]  index{%d} <= rf.lastSnapshotIncludeIndex{%d}", m_me, logIndex, m_lastSnapshotIncludeIndex));
   int  lastLogIndex = getLastLogIndex();
    myAssert(logIndex <= lastLogIndex, format("[func-getSlicesIndexFromLogIndex-rf{%d}]  logIndex{%d} > lastLogIndex{%d}", m_me, logIndex, lastLogIndex));
   int  SliceIndex = logIndex - m_lastSnapshotIncludeIndex - 1;
    return SliceIndex;
}

bool Raft::sendRequestVote(int server, std::shared_ptr<mprrpc::RequestVoteArgs> args, std::shared_ptr<mprrpc::RequestVoteReply> reply,
                           std::shared_ptr<int> votedNum) {


    //这个ok是网络是否正常通信的ok，而不是requestVote rpc是否投票的rpc
    //ok := rf.peers[server].Call("Raft.RequestVote", args, reply)
    //todo
    bool ok = m_peers[server]->RequestVote(args.get(),reply.get());

    if (!ok) {
        return ok;//不知道为什么不加这个的话如果服务器宕机会出现问题的，通不过2B  todo
    }
    //for !ok {
    //
    //	//ok := rf.peers[server].Call("Raft.RequestVote", args, reply)
    //	//if ok {
    //	//	break
    //	//}
    //} //这里是发送出去了，但是不能保证他一定到达
    //对回应进行处理，要记得无论什么时候收到回复就要检查term
    m_mtx.lock();
    Defer ec1([this]()->void {
       m_mtx.unlock();
    });
    if(reply->term() > m_currentTerm){
        m_status = Follower; //三变：身份，term，和投票
        m_currentTerm = reply->term();
        m_votedFor = -1;
        persist();
        return true;
    } else if ( reply->term()   < m_currentTerm   ) {
        return true;
    }
    myAssert(reply->term() == m_currentTerm,
             format("assert {reply.Term==rf.currentTerm} fail"));

    //todo：这里没有按博客写
    if(!reply->votegranted()){
        return true;
    }

    *votedNum = *votedNum + 1;
    if (*votedNum >=  m_peers.size()/2+1) {
        //变成leader
        *votedNum = 0;
        if (m_status == Leader ){ //如果已经是leader了，那么是就是了，不会进行下一步处理了k
            myAssert(false,
                     format("[func-sendRequestVote-rf{%d}]  term:{%d} 同一个term当两次领导，error", m_me, m_currentTerm));

            }
        //	第一次变成leader，初始化状态和nextIndex、matchIndex
        m_status = Leader;


//        DPrintf("[func-sendRequestVote rf{%v}] elect success  ,current term:{%v} ,lastLogIndex:{%v}\n", rf.me, rf.currentTerm, rf.getLastLogIndex())


        int lastLogIndex =   getLastLogIndex();
        for (int i = 0; i <m_nextIndex.size()  ; i++) {
            m_nextIndex[i] = lastLogIndex + 1 ;//有效下标从1开始，因此要+1
            m_matchIndex[i] = 0;               //每换一个领导都是从0开始，见fig2
        }
        std::thread t(&Raft::doHeartBeat, this); //马上向其他节点宣告自己就是leader
        t.detach();

        persist();
    }
    return true;
}

bool
Raft::sendAppendEntries(int server, std::shared_ptr<AppendEntriesArgs> args, std::shared_ptr<AppendEntriesReply> reply,
                        std::shared_ptr<int> appendNums) {

    //这个ok是网络是否正常通信的ok，而不是requestVote rpc是否投票的rpc
    // 如果网络不通的话肯定是没有返回的，不用一直重试
    // todo： paper中5.3节第一段末尾提到，如果append失败应该不断的retries ,直到这个log成功的被store
    bool ok = m_peers[server]->AppendEntries(args.get(),reply.get());
    if (!ok) {
                return ok;
        }
    if (reply->appstate() == Disconnected) {
                return ok;
        }
    m_mtx.lock();
    Defer ec1([this]()->void {
       this->m_mtx.unlock();
    });

    //对reply进行处理
    // 对于rpc通信，无论什么时候都要检查term
    if(reply->term() > m_currentTerm){
        m_status = Follower;
        m_currentTerm = reply->term();
        m_votedFor = -1;
        return ok;
    } else if (reply->term() < m_currentTerm) {
//        DPrintf("[func -sendAppendEntries  rf{%v}]  节点：{%v}的term{%v}<rf{%v}的term{%v}\n", rf.me, server, reply.Term, rf.me, rf.currentTerm)
        return ok;
    }

    if (m_status != Leader) { //如果不是leader，那么就不要对返回的情况进行处理了
                return ok;
        }
    //term相等

    myAssert(reply->term() == m_currentTerm,
             format("reply.Term{%d} != rf.currentTerm{%d}   ", reply->term(), m_currentTerm));
    if (!reply->success()){
        //日志不匹配，正常来说就是index要往前-1，既然能到这里，第一个日志（idnex = 1）发送后肯定是匹配的，因此不用考虑变成负数
        //因为真正的环境不会知道是服务器宕机还是发生网络分区了
        if (reply->updatenextindex()  != -100) {
            //todo:待总结，就算term匹配，失败的时候nextIndex也不是照单全收的，因为如果发生rpc延迟，leader的term可能从不符合term要求
            //变得符合term要求
            //但是不能直接赋值reply.UpdateNextIndex
//            DPrintf("[func -sendAppendEntries  rf{%v}]  返回的日志term相等，但是不匹配，回缩nextIndex[%v]：{%v}\n", rf.me, server, reply.UpdateNextIndex)
            m_nextIndex[server] = reply->updatenextindex();  //失败是不更新mathIndex的
        }
        //	怎么越写越感觉rf.nextIndex数组是冗余的呢，看下论文fig2，其实不是冗余的
    } else {
        *appendNums = *appendNums +1;
        //rf.matchIndex[server] = len(args.Entries) //只要返回一个响应就对其matchIndex应该对其做出反应，
        //但是这么修改是有问题的，如果对某个消息发送了多遍（心跳时就会再发送），那么一条消息会导致n次上涨
        m_matchIndex[server] = std::max(m_matchIndex[server],args->prevlogindex()+args->entries_size()   );
        m_nextIndex[server] = m_matchIndex[server]+1;
        int lastLogIndex = getLastLogIndex();

        myAssert(m_nextIndex[server] <= lastLogIndex+1,
                 format("error msg:rf.nextIndex[%d] > lastLogIndex+1, len(rf.logs) = %d   lastLogIndex{%d} = %d", server,m_logs.size() , server, lastLogIndex));
        if (*appendNums >= 1+m_peers.size()/2) { //可以commit了
            //两种方法保证幂等性，1.赋值为0 	2.上面≥改为==
            *appendNums = 0;
            // todo https://578223592-laughing-halibut-wxvpggvw69qh99q4.github.dev/ 不断遍历来统计rf.commitIndex
            //改了好久！！！！！ leader只有在当前term有日志提交的时候才更新commitIndex，因为raft无法保证之前term的Index是否提交
            //只有当前term有日志提交，之前term的log才可以被提交，只有这样才能保证“领导人完备性{当选领导人的节点拥有之前被提交的所有log，当然也可能有一些没有被提交的}”
            // rf.leaderUpdateCommitIndex()
            if(args->entries_size() >0 && args->entries(args->entries_size()-1).logterm() == m_currentTerm){
                m_commitIndex = std::max(m_commitIndex,args->prevlogindex() + args->entries_size());
            }
            myAssert(m_commitIndex <= lastLogIndex,
                     format("[func-sendAppendEntries,rf{%d}] lastLogIndex:%d  rf.commitIndex:%d\n", m_me, lastLogIndex, m_commitIndex));
            //fmt.Printf("[func-sendAppendEntries,rf{%v}] len(rf.logs):%v  rf.commitIndex:%v\n", rf.me, len(rf.logs), rf.commitIndex)
        }
    }
    return ok;
}

void Raft::AppendEntries(google::protobuf::RpcController *controller, const ::mprrpc::AppendEntriesArgs *request,
                         ::mprrpc::AppendEntriesReply *response, ::google::protobuf::Closure *done) {
    AppendEntries1(request,response);
    done->Run();

}

void Raft::InstallSnapshot(google::protobuf::RpcController *controller, const ::mprrpc::InstallSnapshotRequest *request,
                           ::mprrpc::InstallSnapshotResponse *response, ::google::protobuf::Closure *done) {
    InstallSnapshot(request,response);

    done->Run();
}

void Raft::RequestVote(google::protobuf::RpcController *controller, const ::mprrpc::RequestVoteArgs *request,
                       ::mprrpc::RequestVoteReply *response, ::google::protobuf::Closure *done) {

    RequestVote(request,response);
    done->Run();
}



