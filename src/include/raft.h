#pragma once
#include "raftrpc.h"
#include <mutex>
#include <iostream>
#include <chrono>
#include <thread>
#include "ApplyMsg.h"
#include <vector>
/// @brief //////////// 网络状态表示  todo：可以删除
const int Disconnected = 0; // 方便网络分区的时候debug，网络异常的时候为disconnected，只要网络正常就为AppNormal，防止matchIndex[]数组异常减小
const int AppNormal = 1;

class Raft : mprrpc::raftRpc
{
private:
    // 日志实体，因为可能涉及rpc通信，为了避免错误排查，直接设置成大写开头
    std::vector<RaftRpc *> m_peers;
    int m_me;
    int m_currentTerm;
    int m_votedFor;
    std::vector<LogEntry> m_logs; //// 日志条目数组，包含了状态机要执行的指令集，以及收到领导时的任期号
                                  // 这两个状态所有结点都在维护，易失
    int m_commitIndex;
    int m_lastApplied; // 已经汇报给状态机（上层应用）的log 的index

    // 这两个状态是由服务器来维护，易失
    std::vector<int> m_nextIndex; // 这两个状态的下标1开始，因为通常commitIndex和lastApplied从0开始，应该是一个无效的index，因此下标从1开始
    std::vector<int> m_matchIndex;
    enum Status
    {
        Follower,
        Candidate,
        Leader
    };
    // 身份
    Status m_status;

    // applyChan chan ApplyMsg         // client从这里取日志（2B），client与raft通信的接口
    // ApplyMsgQueue chan ApplyMsg // raft内部使用的chan，applyChan是用于和服务层交互，最后好像没用上

    // 选举超时

    std::chrono::_V2::system_clock::time_point m_lastResetElectionTime;
    // 心跳超时，用于leader
    std::chrono::_V2::system_clock::time_point m_lastResetHearBeatTime;

    // 2D中用于传入快照点
    // 储存了快照中的最后一个日志的Index和Term
    int m_lastSnapshotIncludeIndex;
    int m_lastSnapshotIncludeTerm;

public:
    void AppendEntries1(AppendEntriesArgs *args, AppendEntriesReply *reply);
    void applierTicker();
    bool CondInstallSnapshot(int lastIncludedTerm, int lastIncludedIndex, string snapshot);
    void doElection();
    void doHeartBeat();
    void electionTimeOutTicker();
    std::vector<ApplyMsg> getApplyLogs();
    int getNewCommandIndex();
    void getPrevLogInfo(int server, int *preIndex, int *preTerm);
    void GetState(int *term, bool *isLeader);
    void InstallSnapshot(InstallSnapshotRequest *args, InstallSnapshotResponse *reply);
    void leaderHearBeatTicker();
    void doHeartBeat();
    void leaderSendSnapShot(int server);
    void leaderUpdateCommitIndex();
    bool matchLog(int logIndex, int logTerm);
    void persist();
    void RequestVote(RequestVoteArgs *args, RequestVoteReply *reply);
    bool UpToDate(int index, int term);
    int getLastLogIndex();
    void getLastLogIndexAndTerm(int *lastLogIndex, int *lastLogTerm);
    int getLogTermFromLogIndex(int logIndex);
    int GetRaftStateSize();
    int getSlicesIndexFromLogIndex(int logIndex);

public:
    // 重写基类方法,因为rpc远程调用真正调用的是这个方法
    void AppendEntries(google::protobuf::RpcController *controller,
                       const ::mprrpc::AppendEntriesArgs *request,
                       ::mprrpc::AppendEntriesReply *response,
                       ::google::protobuf::Closure *done);
    void InstallSnapshot(google::protobuf::RpcController *controller,
                         const ::mprrpc::InstallSnapshotRequest *request,
                         ::mprrpc::InstallSnapshotResponse *response,
                         ::google::protobuf::Closure *done);
    void RequestVote(google::protobuf::RpcController *controller,
                     const ::mprrpc::RequestVoteArgs *request,
                     ::mprrpc::RequestVoteReply *response,
                     ::google::protobuf::Closure *done);
};
