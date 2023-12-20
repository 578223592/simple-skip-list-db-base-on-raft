//
// Created by swx on 23-6-16.
//
#include <cstdio>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdarg>
#include <functional>
#include <iostream>
#include <queue>
#include <thread>
#include <mutex> // pthread_mutex_t
#include <condition_variable> // pthread_condition_t
#include <sstream>
#include <random>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <boost/serialization/access.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
std::chrono::_V2::system_clock::time_point now(){
    return std::chrono::high_resolution_clock::now();
}

std::chrono::milliseconds getRandomizedElectionTimeout(){
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(300, 500);

    return std::chrono::milliseconds(dist(rng));
}
using  namespace  std;
int main(){
    auto start = now();
    getRandomizedElectionTimeout();
    auto end = now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    cout << "执行平均时间为 " << duration.count() << " 微秒" << endl;
//    执行平均时间为 29 微秒
    return 0;
}