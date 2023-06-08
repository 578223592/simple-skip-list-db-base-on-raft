//
// Created by swx on 23-6-4.
//
#include "clerk.h"
#include <iostream>

int main(){
    Clerk client;
    client.Init("test.conf");
    client.Put("x","1");
    std::string get1 = client.Get("x");
    std::printf("get return :{%s}",get1.c_str());
    return 0;
}