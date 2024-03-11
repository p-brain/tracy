#include "TracyImGui.hpp"
#include "TracyPrint.hpp"
#include "TracyTimelineContext.hpp"
#include "TracyTimelineItemPlot.hpp"
#include "TracyUtility.hpp"
#include "TracyView.hpp"
#include "TracyWorker.hpp"

namespace tracy
{

constexpr int PlotHeightPx = 100;
constexpr int MinVisSize = 3;

static uint32_t s_plotColors[] = { 0xFF44FF44, 0xFFfc3f8a, 0xFFffb133, 0xFF797d00, 0xFFb67eff, 0xFF564dfa, 0xfff1f1ff, 0xFF6cdc4f, 0xFFff5925, 0xFF7127d1, 0xFF06a1d2, 0xFFbabd08, 0xFFffe6ba, 0xFF004eba, 0xFFffbbd4 };
static int s_numPlotColors = ( int ) sizeof( s_plotColors ) / sizeof( s_plotColors[ 0 ] );

TimelineItemPlot::TimelineItemPlot( View& view, Worker& worker, PlotData* plot )
    : TimelineItem( view, worker, plot, true )
{
    auto& pd = m_plotDetails.push_next();
    pd.m_plot = plot;
    if ( plot->type == PlotType::SysTime ) SetVisible( false ); // Default off
}

void TimelineItemPlot::AddPlot( PlotData *plot )
{ 
    for ( auto &pd : m_plotDetails )
    {
        if ( pd.m_plot == plot )
            return;
    }

    auto &pd = m_plotDetails.push_next();
    pd.m_plot = plot;

    plot->color = s_plotColors[ std::min( s_numPlotColors, ( int ) m_plotDetails.size() ) - 1 ];
};


bool TimelineItemPlot::IsEmpty() const
{
    for ( auto &pd : m_plotDetails )
    {
        if ( !pd.m_plot->data.empty() )
            return false;
    }
    return true;
}

float TimelineItemPlot::HeaderLabelPrefix( const TimelineContext &ctx, int xOffset, int yOffset )
{
    auto draw = ImGui::GetWindowDrawList();
    const auto ty = ImGui::GetTextLineHeight();
    float yPos = 0.0f;
    float rounding = 0.17f * ( ty - 1 );

    if ( m_plotDetails.size() > 1 )
    {
        for ( auto &pd : m_plotDetails )
        {
            auto &plot = pd.m_plot;
            draw->AddRectFilled( ctx.wpos + ImVec2( xOffset + 1.0f, yOffset + yPos + 1.0f ), ctx.wpos + ImVec2( xOffset + ty - 1.0f, yOffset + yPos + ty - 1.0f ), GetPlotColor( *plot, m_worker ), rounding );
            yPos += ty;
        }
        return ty;
    }
    return 0.0f;
}

const char* TimelineItemPlot::HeaderLabel() const
{
    static char tmp[1024];
    tmp[ 0 ] = 0;
    for ( auto &pd : m_plotDetails )
    {
        auto &plot = pd.m_plot;
        if ( tmp[ 0 ] ) strcat( tmp, "\n" );
        switch ( plot->type )
        {
            case PlotType::User:
            case PlotType::Zone:
            case PlotType::AdditionalZone:
                strcat( tmp, m_worker.GetString( plot->name ) );
                break;
            case PlotType::Memory:
                if ( plot->name == 0 )
                {
                    return ICON_FA_MEMORY " Memory usage";
                }
                else
                {
                    sprintf( tmp, ICON_FA_MEMORY " %s", m_worker.GetString( plot->name ) );
                    return tmp;
                }
            case PlotType::SysTime:
                return ICON_FA_GAUGE_HIGH " CPU usage";
            case PlotType::Power:
                sprintf( tmp, ICON_FA_BOLT " %s", m_worker.GetString( plot->name ) );
                return tmp;
            default:
                assert( false );
                return nullptr;
        }
    }
    return tmp;
}

void TimelineItemPlot::HeaderTooltip( const char* label ) const
{
    ImGui::BeginTooltip();

    char *pLabel = strdup( label );
    char *pTokenInit = pLabel;
    char *pToken = nullptr;
    for ( auto &pd : m_plotDetails )
    {
        auto &plot = pd.m_plot;

        SmallColorBox( GetPlotColor( *plot, m_worker ) );
        ImGui::SameLine();
        pToken = strtok( pTokenInit, "\n" );
        pTokenInit = nullptr;
        TextFocused( "Plot", pToken ? pToken : label );
        ImGui::Separator();

        const auto first = RangeBegin();
        const auto last = RangeEnd();
        const auto activity = last - first;
        const auto traceLen = m_worker.GetLastTime() - m_worker.GetFirstTime();

        TextFocused( "Appeared at", TimeToString( first ) );
        TextFocused( "Last event at", TimeToString( last ) );
        TextFocused( "Activity time:", TimeToString( activity ) );
        ImGui::SameLine();
        char buf[ 64 ];
        PrintStringPercent( buf, activity / double( traceLen ) * 100 );
        TextDisabledUnformatted( buf );
        ImGui::Separator();
        TextFocused( "Data points:", RealToString( plot->data.size() ) );
        TextFocused( "Data range:", FormatPlotValue( plot->max - plot->min, plot->format ) );
        TextFocused( "Min value:", FormatPlotValue( plot->min, plot->format ) );
        TextFocused( "Max value:", FormatPlotValue( plot->max, plot->format ) );
        TextFocused( "Avg value:", FormatPlotValue( plot->sum / plot->data.size(), plot->format ) );
        TextFocused( "Data/second:", RealToString( double( plot->data.size() ) / activity * 1000000000ll ) );

        const auto it = std::lower_bound( plot->data.begin(), plot->data.end(), last - 1000000000ll * 10, [] ( const auto &l, const auto &r ) { return l.time.Val() < r; } );
        const auto tr10 = last - it->time.Val();
        if ( tr10 != 0 )
        {
            TextFocused( "D/s (10s):", RealToString( double( std::distance( it, plot->data.end() ) ) / tr10 * 1000000000ll ) );
        }
    }
    free(pLabel);
    ImGui::EndTooltip();
}

void TimelineItemPlot::HeaderExtraContents( const TimelineContext& ctx, int offset, float labelWidth )
{
    auto draw = ImGui::GetWindowDrawList();
    const auto ty = ImGui::GetTextLineHeight();

    char tmp[1024];
    tmp[ 0 ] = 0;
    size_t tmpSize = sizeof( tmp );
    for ( auto &pd : m_plotDetails )
    {
        auto &plot = pd.m_plot;
        if( tmp[0] ) strncat( tmp, "\n", tmpSize );
        strncat( tmp, "( y - range: ", tmpSize );
        strncat( tmp, FormatPlotValue( plot->rMax - plot->rMin, plot->format ), tmpSize );
        strncat( tmp, ", visible data points: ", tmpSize );
        strncat( tmp, RealToString( plot->num ), tmpSize );
        strncat( tmp, ")", tmpSize );
    }
    tmp[ tmpSize - 1 ] = 0;
    draw->AddText( ctx.wpos + ImVec2( ty * 1.5f + labelWidth, offset ), 0xFF226E6E, tmp );
}

void TimelineItemPlot::HeaderExtraPopupItems()
{
    if ( m_plotDetails[0].m_plot[0].type == PlotType::Zone && ImGui::MenuItem( ICON_FA_SKULL_CROSSBONES " Delete" ) )
    {
        for ( auto &pd : m_plotDetails )
        {
            m_worker.QueuePlotForDelete( pd.m_plot );
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::Separator();
    ImGui::Text( ICON_FA_RULER_VERTICAL " Y axis Max:" );
    ImGui::Indent();

    int bFixed = m_userMax >= 0.0;
    ImGui::RadioButton( "Auto", &bFixed, 0 );
    ImGui::RadioButton( "Fixed Value", &bFixed, 1 );
    if ( bFixed )
    {
        ImGui::SameLine();
        if ( m_userMax < 0.0 ) m_userMax = m_max;
        const auto scale = GetScale();
        ImGui::SetNextItemWidth( 90 * scale );
        ImGui::InputDouble( "", &m_userMax, 0.1, 0.5, "%.2f" );
    }
    else
    {
        m_userMax = -1.0;
    }

    ImGui::Unindent();
    ImGui::Separator();

}

int64_t TimelineItemPlot::RangeBegin() const
{
    int64_t retVal = m_plotDetails[ 0 ].m_plot->data.front().time.Val();

    for ( int i = 1; i < m_plotDetails.size(); ++i )
    {
        auto &plot = m_plotDetails[ i ].m_plot;
        retVal = std::min( retVal, plot->data.front().time.Val() );
    }
    return retVal;
}

int64_t TimelineItemPlot::RangeEnd() const
{
    int64_t retVal = m_plotDetails[ 0 ].m_plot->data.back().time.Val();
    for ( int i = 1; i < m_plotDetails.size(); ++i )
    {
        auto &plot = m_plotDetails[ i ].m_plot;
        retVal = std::max( retVal, plot->data.back().time.Val() );
    }
    return retVal;
}

bool TimelineItemPlot::DrawContents( const TimelineContext &ctx, int &offset )
{
    if ( m_plotDetails.size() > 1 )
    {
        int initialOffset = offset;
        for ( auto &pd : m_plotDetails )
        {
            offset = initialOffset;
            m_view.DrawPlot( ctx, *pd.m_plot, pd.m_draw, offset, m_userMax > 0 ? m_userMax : m_max, true );
        }
    }
    else
    {
        m_view.DrawPlot( ctx, *m_plotDetails[ 0 ].m_plot, m_plotDetails[ 0 ].m_draw, offset, m_userMax, false );
    }
    return true;
}


void TimelineItemPlot::DrawFinished()
{
    for ( auto &pd : m_plotDetails )
    {
        pd.m_draw.clear();
    }
}

void TimelineItemPlot::Preprocess( const TimelineContext& ctx, TaskDispatch& td, bool visible, int yPos )
{
    assert( m_plotDetails[0].m_draw.empty() );

    if( !visible ) return;
    if( yPos > ctx.yMax ) return;
    if( IsEmpty() ) return;
    const auto PlotHeight = int( round( PlotHeightPx * GetScale() ) );
    if( yPos + PlotHeight < ctx.yMin ) return;

    td.Queue( [this, &ctx] {
        const auto vStart = ctx.vStart;
        const auto vEnd = ctx.vEnd;
        const auto nspx = ctx.nspx;
        const auto MinVisNs = int64_t( round( MinVisSize * nspx ) );
        m_max = 0;

        for ( auto &pd : m_plotDetails )
        {
            auto p = pd.m_plot;
            auto &vec = p->data;

        vec.ensure_sorted();
        if( vec.front().time.Val() > vEnd || vec.back().time.Val() < vStart )
        {
            p->rMin = 0;
            p->rMax = 0;
            p->num = 0;
            continue;
        }

        auto it = std::lower_bound( vec.begin(), vec.end(), vStart, [] ( const auto& l, const auto& r ) { return l.time.Val() < r; } );
        auto end = std::lower_bound( it, vec.end(), vEnd, [] ( const auto& l, const auto& r ) { return l.time.Val() < r; } );

        if( end != vec.end() ) end++;
        if( it != vec.begin() ) it--;

        double min = it->val;
        double max = it->val;
        const auto num = end - it;
        if( num > 1000000 )
        {
            min = p->min;
            max = p->max;
        }
        else
        {
            auto tmp = it;
            while( ++tmp < end )
            {
                if( tmp->val < min ) min = tmp->val;
                else if( tmp->val > max ) max = tmp->val;
            }
        }
        if( min == max )
        {
            min--;
            max++;
        }

        p->rMin = min;
        p->rMax = max;
        p->num = num;

        m_max = std::max( p->rMax, m_max );

        pd.m_draw.emplace_back( 0 );
        pd.m_draw.emplace_back( it - vec.begin() );

        ++it;
        while( it < end )
        {
            auto next = std::upper_bound( it, end, int64_t( it->time.Val() + MinVisNs ), [] ( const auto& l, const auto& r ) { return l < r.time.Val(); } );
            assert( next > it );
            const auto rsz = uint32_t( next - it );
            if( rsz < 4 )
            {
                for( int i=0; i<rsz; i++ )
                {
                    pd.m_draw.emplace_back( 0 );
                    pd.m_draw.emplace_back( it - vec.begin() );
                    ++it;
                }
            }
            else
            {
                // Sync with View::DrawPlot()!
                constexpr int NumSamples = 256;
                uint32_t samples[NumSamples];
                uint32_t cnt = 0;
                uint32_t offset = it - vec.begin();
                if( rsz < NumSamples )
                {
                    for( cnt=0; cnt<rsz; cnt++ )
                    {
                        samples[cnt] = offset + cnt;
                    }
                }
                else
                {
                    const auto skip = ( rsz + NumSamples - 1 ) / NumSamples;
                    const auto limit = rsz / skip;
                    for( cnt=0; cnt<limit; cnt++ )
                    {
                        samples[cnt] = offset + cnt * skip;
                    }
                    if( cnt == limit ) cnt--;
                    samples[cnt++] = offset + rsz - 1;
                }
                it = next;

                pdqsort_branchless( samples, samples+cnt, [&vec] ( const auto& l, const auto& r ) { return vec[l].val < vec[r].val; } );

                assert( rsz > 0 );
                pd.m_draw.emplace_back( rsz );
                pd.m_draw.emplace_back( offset );
                pd.m_draw.emplace_back( samples[0] );
                pd.m_draw.emplace_back( samples[cnt-1] );
            }
        }

        }
    } );
}

}
