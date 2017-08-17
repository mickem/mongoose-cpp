#ifndef _MONGOOSE_UTILS_H
#define _MONGOOSE_UTILS_H
#pragma once

#include <string>

namespace Mongoose
{
    class Utils
    {
        public:
            static std::string htmlEntities(std::string data);
            static void xsleep(int ms);
    };
}

#endif

