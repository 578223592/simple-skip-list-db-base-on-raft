#include <functional>
#include <iostream>
using namespace std;
/// @brief /////////////c++ 实现类似go中defer的作用
// 但是要可以传入类
class Defer final
{
public:
    explicit Defer(std::function<void()> fun) : m_funCall(fun) {}
    ~Defer() { m_funCall(); }

private:
    std::function<void()> m_funCall;
};
class modeA{
    public:
    int num =1;
    modeA(){
        num = 0;
    }
    void show(){
        cout<<num<<endl;
    }
};

void test(modeA& a){
    Defer ec1([&a]
              { a.num = 1; });
}
//输出为1
int main(int argc, char **argv)
{
    modeA a;

    test(a);
    a.show();
    return 0;
}