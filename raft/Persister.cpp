//
// Created by swx on 23-5-30.
//
#include "Persister.h"
#include "util.h"

void Persister::Save(const std::string raftstate, const std::string snapshot) {
    mtx.lock();
    Defer ec1([this]()->void {
       mtx.unlock();
    });
    // 将raftstate和snapshot写入本地文件
    std::ofstream outfile;
    outfile.open(raftStateFile);
    outfile << raftstate;
    outfile.close();

    outfile.open(snapshotFile);
    outfile << snapshot;
    outfile.close();
}

std::string Persister::ReadSnapshot() {
    std::lock_guard<std::mutex> lg(mtx);

    std::fstream ifs(snapshotFile,std::ios_base::in);
    if(!ifs.good()){
        return "";
    }
    std::string  snapshot;
    ifs>>snapshot;
    ifs.close();
    return snapshot;
}

void Persister::SaveRaftState(std::string data) {
    mtx.lock();
    Defer ec1([this]()->void {
        mtx.unlock();
    });
    // 将raftstate和snapshot写入本地文件
    std::ofstream outfile;
    outfile.open(raftStateFile);
    outfile << data;
    outfile.close();


}

int Persister::RaftStateSize() {
    mtx.lock();
    Defer ec1([this]()->void {
        mtx.unlock();
    });

    std::fstream ifs(raftStateFile,std::ios_base::in);
    if(!ifs.good()){
        return 0;
    }
    std::string  snapshot;
    ifs>>snapshot;
    ifs.close();
    return snapshot.size();
}

std::string Persister::ReadRaftState() {
    mtx.lock();
    Defer ec1([this]()->void {
        mtx.unlock();
    });

    std::fstream ifs(raftStateFile,std::ios_base::in);
    if(!ifs.good()){
        return "";
    }
    std::string  snapshot;
    ifs>>snapshot;
    ifs.close();
    return snapshot;
}

Persister::Persister(int me) :raftStateFile("raftstatePersist"+ std::to_string(me)+".txt"),snapshotFile("snapshotPersist"+ std::to_string(me)+".txt") {

    std::fstream file(raftStateFile, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        file.close();
    }
    file = std::fstream (snapshotFile, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        file.close();
    }
}



