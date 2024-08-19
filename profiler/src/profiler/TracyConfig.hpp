#ifndef __TRACYCONFIG_HPP__
#define __TRACYCONFIG_HPP__

#include "TracyUtility.hpp"

namespace tracy
{

struct Config
{
    bool threadedRendering = true;
    bool focusLostLimit = true;
    int targetFps = 60;
    bool memoryLimit = false;
    int memoryLimitPercent = 80;
    bool achievements = false;
    bool achievementsAsked = true;
    int dynamicColors = 1;
    bool forceColors = false;
    int shortenName = (int)ShortenName::NoSpaceAndNormalize;
    bool keepSingleThreadLocks = false;
};

}

#endif