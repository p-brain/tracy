#ifndef __TRACYTIMELINEITEMPLOT_HPP__
#define __TRACYTIMELINEITEMPLOT_HPP__

#include "TracyEvent.hpp"
#include "TracyTimelineDraw.hpp"
#include "TracyTimelineItem.hpp"

namespace tracy
{

class TimelineItemPlot final : public TimelineItem
{
public:
    TimelineItemPlot( View& view, Worker& worker, PlotData* plot );
    void AddPlot( PlotData *plot );

protected:
    uint32_t HeaderColor() const override { return 0xFF44DDDD; }
    uint32_t HeaderColorInactive() const override { return 0xFF226E6E; }
    uint32_t HeaderLineColor() const override { return 0x8844DDDD; }
    const char* HeaderLabel() const override;

    int64_t RangeBegin() const override;
    int64_t RangeEnd() const override;

    void HeaderTooltip( const char* label ) const override;
    float HeaderLabelPrefix( const TimelineContext &ctx, int xOffset, int yOffset ) override; // Return width of prefix
    void HeaderExtraContents( const TimelineContext& ctx, int offset, float labelWidth ) override;
    void HeaderExtraPopupItems() override;

    bool DrawContents( const TimelineContext& ctx, int& offset ) override;
    void DrawFinished() override;

    bool IsEmpty() const override;

    void Preprocess( const TimelineContext& ctx, TaskDispatch& td, bool visible, int yPos ) override;

private:
    struct PlotDetails
    {
        PlotData * m_plot;
        std::vector<uint32_t> m_draw;
    };
    Vector<PlotDetails> m_plotDetails;
    double m_max;
    double m_userMax = -1.0;
};

}

#endif
