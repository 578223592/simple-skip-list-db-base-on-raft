#include <functional>
#include <iostream>
class Defer final
{
public:
    explicit Defer(std::function<void()> fun) : m_funCall(fun) {}
    ~Defer() { m_funCall(); }

private:
    std::function<void()> m_funCall;
};

void myAssert(bool condition, const char *message = "Assertion failed!")
{
    if (!condition)
    {
        std::cerr << "Error: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

template <typename... Args>
char * format(const char *format_str, Args... args)
{
    std::stringstream ss;
    int _[] = {((ss << args), 0)...};
    (void)_;
    return ss.str().c_str();

}

std::chrono::_V2::system_clock::time_point now(){
    return std::chrono::high_resolution_clock::now();
}