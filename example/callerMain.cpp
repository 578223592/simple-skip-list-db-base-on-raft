//
// Created by swx on 23-6-4.
//
#include "clerk.h"
#include <iostream>
#include "util.h"
int main(){
    Clerk client;
    client.Init("test.conf");
    auto start = now();
    int count = 500;
    int tmp = count;
    while (tmp --){
        client.Put("x","1");

        std::string get1 = client.Get("x");
//        std::printf("get return :{%s}",get1.c_str());
    }
    auto end = now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    cout << "执行平均时间为 " << duration.count()/count << " 微秒" << endl;
    return 0;
}