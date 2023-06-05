


#include "util.h"



void myAssert(bool condition, std::string message)
{
    if (!condition)
    {
        std::cerr << "Error: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
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




