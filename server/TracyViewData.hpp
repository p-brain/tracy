#ifndef __TRACYVIEWDATA_HPP__
#define __TRACYVIEWDATA_HPP__

#include <stdint.h>
#include <regex>
#include <unordered_map>
#include <string>

namespace tracy
{

struct Range
{
    void StartFrame() { hiMin = hiMax = false; }

    int64_t min = 0;
    int64_t max = 0;
    bool active = false;
    bool hiMin = false;
    bool hiMax = false;
    bool modMin = false;
    bool modMax = false;
};

struct RangeSlim
{
    bool operator==( const Range& other ) const { return other.active == active && other.min == min && other.max == max; }
    bool operator!=( const Range& other ) const { return !(*this == other); }
    void operator=( const Range& other ) { active = other.active; min = other.min; max = other.max; }

    int64_t min, max;
    bool active = false;
};



struct ViewData
{
    enum EThreadStackCollapse { CollapseDynamic = 0, CollapseMax = 1, CollapseLimit = 2 };
    enum EPlotViz { Disable = 0, Top, Bottom };
    const char *const ppszPlotViz[ 3 ]{ "Disable Plots", "Plots Above Zones", "Plots Below Zone" };


    int64_t zvStart = 0;
    int64_t zvEnd = 0;
    int32_t frameScale = 0;
    int32_t frameStart = 0;

    uint8_t drawGpuZones = true;
    uint8_t drawZones = true;
    uint8_t drawLocks = true;
    uint8_t drawPlots = EPlotViz::Top;
    uint8_t onlyContendedLocks = true;
    uint8_t drawEmptyLabels = false;
    uint8_t drawFrameTargets = false;
    uint8_t drawContextSwitches = true;
    uint8_t darkenContextSwitches = true;
    uint8_t drawCpuData = true;
    uint8_t drawCpuUsageGraph = true;
    uint8_t drawSamples = true;
    uint8_t dynamicColors = 1;
    uint8_t forceColors = false;
    uint8_t ghostZones = true;

    uint32_t frameTarget = 60;
    float    flFrameHeightScale = 1.0f;
    int32_t frameOverviewMaxTimeMS = 15;

    uint8_t zoneNameShortening = 4;
    uint8_t stackCollapseMode = EThreadStackCollapse::CollapseDynamic;
    int32_t stackCollapseClamp = 0;
};

struct Annotation
{
    std::string text;
    Range range;
    uint32_t color;
};

struct SourceRegex
{
    std::string pattern;
    std::string target;
    std::regex regex;
};

}

#endif
