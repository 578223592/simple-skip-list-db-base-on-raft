#include "raft.h"
#include <util.h>
void Raft::AppendEntries1(AppendEntriesArgs *args, AppendEntriesReply* reply)
{
    std::lock_guard<std::mutex> locker();
    reply->set_appstate(AppNormal);// 能接收到代表网络是正常的
                                       // Your code here (2A, 2B).
//	不同的人收到AppendEntries的反应是不同的，要注意无论什么时候收到rpc请求和响应都要检查term
    

    if (args->term()<m_currentTerm){
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(-100); // 论文中：让领导人可以及时更新自己
        // DPrintf("[func-AppendEntries-rf{%v}] 拒绝了 因为Leader{%v}的term{%v}< rf{%v}.term{%v}\n", rf.me, args.LeaderId, args.Term, rf.me, rf.currentTerm) 
        return; // 注意从过期的领导人收到消息不要重设定时器
        } 
        Defer ec1([this]()->void {this->persist();});//由于这个局部变量创建在锁之后，因此执行persist的时候应该也是拿到锁的.
        if (args->term()>m_currentTerm){
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
        if(args->prevlogindex()>getLastLogIndex())
        {
            reply->set_success(false);
            reply->set_term(m_currentTerm);
            reply->set_updatenextindex(getLastLogIndex()+1);
            //  DPrintf("[func-AppendEntries-rf{%v}] 拒绝了节点{%v}，因为日志太新,args.PrevLogIndex{%v} > lastLogIndex{%v}，返回值：{%v}\n", rf.me, args.LeaderId, args.PrevLogIndex, rf.getLastLogIndex(), reply) 
            return;
        }
        else if (args->prevlogindex() < m_lastSnapshotIncludeIndex)
        { // 如果prevlogIndex还没有更上快照
            reply->set_success(false);
            reply->set_term(m_currentTerm);
            reply->set_updatenextindex(m_lastSnapshotIncludeIndex + 1); // todo 如果想直接弄到最新好像不对，因为是从后慢慢往前匹配的，这里不匹配说明后面的都不匹配
            //  DPrintf("[func-AppendEntries-rf{%v}] 拒绝了节点{%v}，因为log太老，返回值：{%v}\n", rf.me, args.LeaderId, reply) return
        }
    //	本机日志有那么长，冲突(same index,different term),截断日志
    // 注意：这里目前当args.PrevLogIndex == rf.lastSnapshotIncludeIndex与不等的时候要分开考虑，可以看看能不能优化这块
    if(matchLog(args->prevlogindex(),args->prevlogterm())){
            //	todo：	整理logs ，不能直接截断，必须一个一个检查，因为发送来的log可能是之前的，直接截断可能导致“取回”已经在follower日志中的条目
            // 那意思是不是可能会有一段发来的AE中的logs中前半是匹配的，后半是不匹配的，这种应该：1.follower如何处理？ 2.如何给leader回复
            // 3. leader如何处理
        // 2023年5月28日：到这了
        for
            _, log : = range args.Entries
            {
                if log
                    .LogIndex > rf.getLastLogIndex()
                    {
                        rf.logs = append(rf.logs, log)
                    }
                else
                { // todo ： 这里可以改进为比较对应logIndex位置的term是否相等，term相等就代表匹配
                    //  todo：这个地方放出来会出问题,按理说index相同，term相同，log也应该相同才对
                    // rf.logs[entry.Index-firstIndex].Term ?= entry.Term

                    if rf
                        .logs[rf.getSlicesIndexFromLogIndex(log.LogIndex)].LogTerm == log.LogTerm &&rf.logs[rf.getSlicesIndexFromLogIndex(log.LogIndex)].Command != log.Command
                        {
                            assert(false, fmt.Sprintf("[func-AppendEntries-rf{%v}] 两节点logIndex{%v}和term{%v}相同，但是其command{%v:%v}    {%v:%v}却不同！！\n", rf.me, log.LogIndex, log.LogTerm, rf.me, rf.logs[rf.getSlicesIndexFromLogIndex(log.LogIndex)].Command, args.LeaderId, log.Command))
                        }
                    if rf
                        .logs[rf.getSlicesIndexFromLogIndex(log.LogIndex)].LogTerm != log.LogTerm
                        {
                            rf.logs[rf.getSlicesIndexFromLogIndex(log.LogIndex)] = log
                        }
                }
            }

        // 错误写法like：  rf.shrinkLogsToIndex(args.PrevLogIndex)
        // rf.logs = append(rf.logs, args.Entries...)
        // 因为可能会收到过期的log！！！ 因此这里是大于等于
        assert(rf.getLastLogIndex() >= args.PrevLogIndex + len(args.Entries), fmt.Sprintf("[func-AppendEntries1-rf{%v}]rf.getLastLogIndex(){%v} != args.PrevLogIndex{%v}+len(args.Entries){%v}", rf.me, rf.getLastLogIndex(), args.PrevLogIndex, len(args.Entries)))
            // if len(args.Entries) > 0 {
            //	fmt.Printf("[func-AppendEntries  rf:{%v}] ] : args.term:%v, rf.term:%v  ,rf.logs的长度：%v\n", rf.me, args.Term, rf.currentTerm, len(rf.logs))
            // }

            if args.LeaderCommit > rf.commitIndex{
                                       rf.commitIndex = intMin(args.LeaderCommit, rf.getLastLogIndex()) // 这个地方不能无脑跟上
                                   }

                                   // 领导会一次发送完所有的日志
                                   assert(rf.getLastLogIndex() >= rf.commitIndex, fmt.Sprintf("[func-AppendEntries1-rf{%v}]  rf.getLastLogIndex{%v} < rf.commitIndex{%v}", rf.me, rf.getLastLogIndex(), rf.commitIndex)) reply.Success,
            reply.Term = true, rf.currentTerm DPrintf("[func-AppendEntries-rf{%v}] 接收了来自节点{%v}的log，当前lastLogIndex{%v}，返回值：{%v}\n", rf.me, args.LeaderId, rf.getLastLogIndex(), reply)

                                   return
        }
    else
    {
        // 优化
        // PrevLogIndex 长度合适，但是不匹配，因此往前寻找 矛盾的term的第一个元素
        // 为什么该term的日志都是矛盾的呢？也不一定都是矛盾的，只是这么优化减少rpc而已
        // ？什么时候term会矛盾呢？很多情况，比如leader接收了日志之后马上就崩溃等等
        reply.UpdateNextIndex = args.PrevLogIndex

		for index := args.PrevLogIndex;
        index >= rf.lastSnapshotIncludeIndex;
        index--
        { // 这样的话最多回退到rf.lastSnapshotIncludeIndex，会不会有问题呢？
        // 应该不会，raft可以保证向上提交的commitIndex是一致的（状态机是一致的），因此snapshot是不冲突的
        if rf
            .getLogTermFromLogIndex(index) != rf.getLogTermFromLogIndex(args.PrevLogIndex)
            {
                reply.UpdateNextIndex = index + 1 break
            }
        }

        reply.Success, reply.Term = false, rf.currentTerm
                                               // 对UpdateNextIndex待优化  todo  找到符合的term的最后一个
                                               DPrintf("[func-AppendEntries-rf{%v}] 拒绝了节点{%v}，因为prevLodIndex{%v}的args.term{%v}不匹配当前节点的logterm{%v}，返回值：{%v}\n", rf.me, args.LeaderId, args.PrevLogIndex, args.PrevLogTerm, rf.logs[rf.getSlicesIndexFromLogIndex(args.PrevLogIndex)].LogTerm, reply) DPrintf("[func-AppendEntries-rf{%v}] 返回值: reply.UpdateNextIndex从{%v}优化到{%v}，优化了{%v}\n", rf.me, args.PrevLogIndex, reply.UpdateNextIndex, args.PrevLogIndex - reply.UpdateNextIndex) // 很多都是优化了0
                                           return
    }

    // fmt.Printf("[func-AppendEntries,rf{%v}]:len(rf.logs):%v, rf.commitIndex:%v\n", rf.me, len(rf.logs), rf.commitIndex)
}