//
// Created by swx on 23-5-29.
//

#include <iostream>
#include <thread>
#include <chrono>

using  namespace  std;
class MyClass {
public:
    int menber_num ;
    void a() {
        int num = 1;
        menber_num = 1;
        cout<<"befor b,  num:"<<num<<endl;
        cout<<"befor b,  menber_num:"<<menber_num<<endl;
        std::thread t(&MyClass::b, this, &num); // 创建新线程并执行b函数，并传递参数
        t.join(); // 等待新线程执行完毕
        cout<<"after b,  num:"<<num<<endl;
        cout<<"after b,  menber_num:"<<menber_num<<endl;
    }

    void b( int* num) { // 在b函数中接收参数
        *num = 2;
        menber_num = 2;
    }
};

int main() {
    MyClass obj;
    obj.a(); // 执行a函数
    return 0;
}

