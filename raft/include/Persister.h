//
// Created by swx on 23-5-30.
//

#ifndef SKIP_LIST_ON_RAFT_PERSISTER_H
#define SKIP_LIST_ON_RAFT_PERSISTER_H
#include <mutex>
class Persister{
private:
    std::mutex mtx;
    std::string raftState;
    std::string snapshot;
public:
    void Save(const std::string raftstate , const std::string snapshot );
    std::string ReadSnapshot();
    void SaveRaftState(std::string data);
    int RaftStateSize();
};


#endif //SKIP_LIST_ON_RAFT_PERSISTER_H
