//
// Created by swx on 23-6-1.
//
#include "../raft/include/util.h"
int main(){
    LockQueue<int> lq;
    lq.Push(1);
    int res = 0;
    std::thread t(&LockQueue<int>::timeOutPop,&lq,1000,&res);
    t.detach();
    int res2 =0;
    lq.timeOutPop(1000,&res2);
    while(true){

    }
    return 0;
}
