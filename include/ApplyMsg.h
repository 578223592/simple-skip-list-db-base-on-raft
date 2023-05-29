#pragma once
#include <string>
class ApplyMsg
{
    bool CommandValid;
    std::string Command;
    int CommandIndex;
    bool SnapshotValid;
    std::string Snapshot;
    int SnapshotTerm;
    int SnapshotIndex;
};