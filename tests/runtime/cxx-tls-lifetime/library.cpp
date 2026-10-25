#include <iostream>
#include <exception>

struct FlushAtDetach
{
    ~FlushAtDetach() { std::cout.flush(); }
};
static FlushAtDetach flush_at_detach;

extern "C" __declspec(dllexport) int touch_tls()
{
    return std::uncaught_exceptions();
}
