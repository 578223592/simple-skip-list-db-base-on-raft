//
// Created by swx on 23-5-29.
//

#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <mutex>
using  namespace  std;
class MyClass {
public:
    int menber_num ;
    mutex mtx;
    void a() {
        cout<<"no lock"<<endl;
        mtx.lock();

//        mtx.lock();
cout<<"get lock"<<endl;
        std::shared_ptr<int> num = std::make_shared<int>(10); // 使用 make_shared 函数初始化
        menber_num = 1;
        cout<<"befor b,  num:"<<num<<endl;
        cout<<"befor b,  menber_num:"<<menber_num<<endl;
        cout<<"num address:"<<num<<"    *num :"<<*num<<endl;

        std::thread t(&MyClass::b, this, num); // 创建新线程并执行b函数，并传递参数
        t.detach();   //锁可以保证当前a函数执行完毕之后才会执行b函数
        //发现a执行完成之后b可以执行，代表锁的状态
        std::this_thread::sleep_for(std::chrono::seconds(10));
        cout<<"num.use_count()"<<num.use_count()<<endl;
        cout<<"after b,  num:"<<num<<endl;
        cout<<"after b,  menber_num:"<<menber_num<<endl;
        mtx.unlock();
    }

    void b( std::shared_ptr<int> num) { // 在b函数中接收参数  //相当于复制拷贝，这样的话就会增加引用技术了
        mtx.lock();
        cout<<"thread b sleep ..."<<endl;
        cout<<"num.use_count()"<<num.use_count()<<endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        cout<<"thread b start wake up"<<endl;
        cout<<"num.use_count()"<<num.use_count()<<endl;
        *num = 2;
        menber_num = 2;
        cout<<"num address:"<<num<<"    *num :"<<*num<<endl;
        mtx.unlock();
    }
    void show(){
        cout<<"func-show-"<<endl;
    }
};

int main() {
    MyClass obj{};
    obj.a(); // 执行a函数
    while (true){

    }
    return 0;
}

//输出很奇怪，为：
//befor b,  num:1
//befor b,  menber_num:1
//num address:1    *num :0x7ffdffd744a4
//after b,  num:1
//after b,  menber_num:1
//thread b sleep ...
//        thread b start wake up
//num address:0x7ffdffd744a4    *num :2
