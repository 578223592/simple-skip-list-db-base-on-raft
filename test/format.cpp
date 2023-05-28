#include <iostream>
#include <sstream>

template <typename... Args>
std::string format(const char *format_str, Args... args)
{
    std::stringstream ss;
    int _[] = {((ss << args), 0)...};
    (void)_;
    return ss.str();
}

int main()
{
    std::string s = format("%s %d %f", "Hello", 42, 3.14);
    std::cout << s << std::endl; // Output: "Hello 42 3.140000"
    return 0;
}
