#pragma once
#include <functional>
#include <iostream>
#include <queue>
#include <thread>
#include <mutex> // pthread_mutex_t
#include <condition_variable> // pthread_condition_t
#include <sstream>
#include "config.h"
#include <random>

class Defer final
{
public:
    explicit Defer(std::function<void()> fun) : m_funCall(fun) {}
    ~Defer() { m_funCall(); }

private:
    std::function<void()> m_funCall;
};

void myAssert(bool condition, std::string message = "Assertion failed!")
{
    if (!condition)
    {
        std::cerr << "Error: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

template <typename... Args>
std::string format(const char *format_str, Args... args)
{
    std::stringstream ss;
    int _[] = {((ss << args), 0)...};
    (void)_;
    return ss.str();

}

std::chrono::_V2::system_clock::time_point now(){
    return std::chrono::high_resolution_clock::now();
}

std::chrono::milliseconds getRandomizedElectionTimeout(){
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(minRandomizedElectionTime, maxRandomizedElectionTime);

    return std::chrono::milliseconds(dist(rng));
}
void sleepNMilliseconds(int N){
    std::this_thread::sleep_for(std::chrono::milliseconds(N));
};


// ////////////////////////异步写日志的日志队列
//read is blocking!!! LIKE  go chan
template<typename T>
class LockQueue
{
public:
    // 多个worker线程都会写日志queue
    void Push(const T &data)
    {
        std::lock_guard<std::mutex> lock(m_mutex); //使用lock_gurad，即RAII的思想保证锁正确释放
        m_queue.push(data);
        m_condvariable.notify_one();
    }

    // 一个线程读日志queue，写日志文件
    T Pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty())
        {
            // 日志队列为空，线程进入wait状态
            m_condvariable.wait(lock);//这里用unique_lock是因为lock_guard不支持解锁，而unique_lock支持
        }
        T data = m_queue.front();
        m_queue.pop();
        return data;
    }

    bool timeOutPop(int timeout ,T* ResData) // 添加一个超时时间参数，默认为 50 毫秒
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        // 获取当前时间点，并计算出超时时刻
        auto now = std::chrono::system_clock::now();
        auto timeout_time = now + std::chrono::milliseconds(timeout);

        // 在超时之前，不断检查队列是否为空
        while (m_queue.empty()) {
            // 如果已经超时了，就返回一个空对象
            if (m_condvariable.wait_until(lock, timeout_time) == std::cv_status::timeout) {
                return false;
            }else{
                break;
            }
        }

        T data = m_queue.front();
        m_queue.pop();
        *ResData = data;
        return true;
    }
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condvariable;
};
// 两个对锁的管理用到了RAII的思想，防止中途出现问题而导致资源无法释放的问题！！！
// std::lock_guard 和 std::unique_lock 都是 C++11 中用来管理互斥锁的工具类，它们都封装了 RAII（Resource Acquisition Is Initialization）技术，使得互斥锁在需要时自动加锁，在不需要时自动解锁，从而避免了很多手动加锁和解锁的繁琐操作。
// std::lock_guard 是一个模板类，它的模板参数是一个互斥量类型。当创建一个 std::lock_guard 对象时，它会自动地对传入的互斥量进行加锁操作，并在该对象被销毁时对互斥量进行自动解锁操作。std::lock_guard 不能手动释放锁，因为其所提供的锁的生命周期与其绑定对象的生命周期一致。
// std::unique_lock 也是一个模板类，同样的，其模板参数也是互斥量类型。不同的是，std::unique_lock 提供了更灵活的锁管理功能。可以通过 lock()、unlock()、try_lock() 等方法手动控制锁的状态。当然，std::unique_lock 也支持 RAII 技术，即在对象被销毁时会自动解锁。另外， std::unique_lock 还支持超时等待和可中断等待的操作。


// 这个Op是kv传递给raft的command
class Op {
public:
    // Your definitions here.
    // Field names must start with capital letters,
    // otherwise RPC will break.
    std::string Operation;  // "Get" "Put" "Append"
    std::string Key;
    std::string Value;
    std::string  ClientId;  //客户端号码
    int RequestId;  //客户端号码请求的Request的序列号，为了保证线性一致性
    // IfDuplicate bool // Duplicate command can't be applied twice , but only for PUT and APPEND

public:
    //todo
    //为了协调raftRPC中的command只设置成了string,这个的限制就是正常字符中不能包含|
    //当然后期可以换成更高级的序列化方法，比如protobuf
    std::string asString() const {
        std::ostringstream ss;
        ss << Operation << '|' << Key << '|' << Value << '|' << ClientId << '|' << RequestId;
        return ss.str();
    }

    bool parseFromString(std::string str) {
        std::string sop,skey,svalue,sclienid,srequestid;
        int i = 0;
        for (; i < str.size(); ++i) {
            if(str[i] != '|'){
                sop.push_back(str[i]);
            }else{
                break;
            }
        }
        for(;i<str.size();++i){
            if(str[i] != '|'){
                skey.push_back(str[i]);
            }else{
                break;
            }
        }
        for(;i<str.size();++i){
            if(str[i] != '|'){
                svalue.push_back(str[i]);
            }else{
                break;
            }
        }
        for(;i<str.size();++i){
            if(str[i] != '|'){
                sclienid.push_back(str[i]);
            }else{
                break;
            }
        }
        for(;i<str.size();++i){
            if(str[i] != '|'){
                srequestid.push_back(str[i]);
            }else{
                break;
            }
        }
        if(srequestid.empty()){return false;} //至少有一个没有解析到；
        Operation = sop;
        Key = skey;
        Value = svalue;
        ClientId = sclienid;
        RequestId = atoi(srequestid.c_str());
        return true;
    }
};

///////////////////////////////////////////////kvserver reply err to clerk

const std::string OK = "OK";
const std::string ErrNoKey = "ErrNoKey";
const std::string ErrWrongLeader = "ErrWrongLeader";


