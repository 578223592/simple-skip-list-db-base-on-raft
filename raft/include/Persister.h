//
// Created by swx on 23-5-30.
//

#ifndef SKIP_LIST_ON_RAFT_PERSISTER_H
#define SKIP_LIST_ON_RAFT_PERSISTER_H
#include <mutex>
#include <fstream>
class Persister{
private:
    std::mutex mtx;
    std::string m_raftState;
    std::string m_snapshot;
    const std::string raftStateFile;
    const std::string snapshotFile;
public:
    void Save(std::string raftstate , std::string snapshot );
    std::string ReadSnapshot();
    void SaveRaftState(std::string data);
    int RaftStateSize();
    std::string ReadRaftState();
    explicit Persister(int me);
};


#endif //SKIP_LIST_ON_RAFT_PERSISTER_H
