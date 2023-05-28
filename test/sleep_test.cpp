#include <iostream>
#include <chrono>
#include <thread>


/// @brief c++sleep的学习
/// @return 
int main()
{
    std::cout << "start wait......" << std::endl;
    /**
        在C++11 中 chrono中有三种时钟：system_clock, steady_clock 和 high_resolution_clock
        system_clock : 是不稳定的，因为时钟是可调的。它可以输出当前时间距离：1970年1月1日00:00的毫秒数
                       它一般用于需要得到 ：绝对时点的场景。

        steady_clock：一般用在需要得到时间间隔的场景。

        high_resolution_clock：介于上面两者之间，时间精度比steady_clock()更精确
    */
    auto start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout<< (elapsed.count()>=0) <<std::endl;
    std::cout << "waited: " << elapsed.count() << " ms" << std::endl;
}

