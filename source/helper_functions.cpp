
#include "helper_functions.h"

std::string zeroPadInt(int value)
{
    if (value < 10)
        return "0" + std::to_string(value);
    else
        return std::to_string(value);
}