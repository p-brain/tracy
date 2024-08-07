#ifndef __TRACYTIMELINEITEMCORE_HPP__
#define __TRACYTIMELINEITEMCORE_HPP__

#include "TracyTimelineItem.hpp"
#include "TracyView.hpp"
#include "TracyViewData.hpp"
#include "TracyCpuZoneBuffer.hpp"
#include <stdint.h>
#include <string>


namespace tracy
{


struct TimelineContext;
class TimelinePreprocessor;
struct CpuData;
class Worker;


class TimelineItemCore final : public TimelineItem
{
public:
    TimelineItemCore( View& view, Worker& worker, const CpuData* coreInfo );

    bool PreventScrolling() const override;

protected:
    uint32_t HeaderColor() const override;
    uint32_t HeaderColorInactive() const override;
    uint32_t HeaderLineColor() const override;
    const char* HeaderLabel() const override;

    int64_t RangeBegin() const override;
    int64_t RangeEnd() const override;

    void HeaderTooltip( const char* label ) const override;

    bool DrawContents( const TimelineContext& ctx, int& offset ) override;
    void DrawUiControls( const TimelineContext &ctx, int start, int &offset, float xOffset ) override;
    void DrawFinished() override;

    bool IsEmpty() const override;

    void Preprocess( const TimelineContext& ctx, TaskDispatch& td, bool visible, int yPos ) override;

private:
    struct CoreInfo
    {
        uint32_t package;
        uint32_t core;
        uint32_t coreIndex;
        const CpuData *cpuData;
    };

    void BuildThreadDataLut();

    int PreprocessZones( const TimelineContext& ctx, const Vector<ContextSwitchCpu> &cslist, const CpuZoneRange &zoneRange, TimelinePreprocessor& preproc, bool visible );
    void PreprocessCpuCtxSwitches( const TimelineContext& ctx, const Vector<ContextSwitchCpu> &cslist, const ContextSwitchCpuRange &ctxRange );

    CoreInfo m_coreInfo;
    std::string m_Name;

    std::vector<TimelineDraw> m_draw;
    std::vector<ContextSwitchDraw> m_ctxDraw;
    int m_maxDepth;
    int m_depth;

    bool m_keepActive;

    TrackUiData m_trackUiData;

    CpuZoneBuffer m_cpuZoneBuffer;
    Vector<const ThreadData *> m_threadLut;
};

}


#endif
