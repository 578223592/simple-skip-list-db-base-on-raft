//
// Created by swx on 23-5-29.
//

#ifndef SKIP_LIST_ON_RAFT_CONFIG_H
#define SKIP_LIST_ON_RAFT_CONFIG_H

const bool Debug = false;

const int debugMul = 1;
const int HeartBeatTimeout = 25 *debugMul;//心跳时间应该要比选举超时小一个数量级
const int ApplyInterval = 10 *debugMul;//time.Millisecond

const int minRandomizedElectionTime = 300 * debugMul; //ms
const int maxRandomizedElectionTime = 500 * debugMul; //ms




const int CONSENSUS_TIMEOUT = 500 * debugMul;//ms
#endif //SKIP_LIST_ON_RAFT_CONFIG_H
