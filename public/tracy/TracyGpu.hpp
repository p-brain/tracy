#ifndef __TRACYGPUS2_HPP__
#define __TRACYGPUS2_HPP__

#ifndef TRACY_ENABLE

#define TracyGPUS2Context(device,queue) nullptr
#define TracyGPUS2Destroy(ctx)
#define TracyGPUS2ContextName(ctx, name, size)

#define TracyGPUS2NewFrame(ctx)

#define TracyGPUS2Zone(ctx, name)
#define TracyGPUS2ZoneC(ctx, name, color)
#define TracyGPUS2NamedZone(ctx, varname, name, active)
#define TracyGPUS2NamedZoneC(ctx, varname, name, color, active)
#define TracyD3D12ZoneTransient(ctx, varname, name, active)

#define TracyGPUS2ZoneS(ctx, name, depth)
#define TracyGPUS2ZoneCS(ctx, name, color, depth)
#define TracyGPUS2NamedZoneS(ctx, varname, name, depth, active)
#define TracyGPUS2NamedZoneCS(ctx, varname, name, color, depth, active)
#define TracyD3D12ZoneTransientS(ctx, varname, name, depth, active)

#define TracyGPUS2Collect(ctx)

namespace tracy
{
    class GPUS2ZoneScope {};
}

using TracyGPUS2Ctx = void *;

#else

#include <atomic>
#include <assert.h>
#include <stdlib.h>

#include "Tracy.hpp"
#include "../client/TracyProfiler.hpp"
#include "../client/TracyCallstack.hpp"
#include "../common/TracyAlign.hpp"
#include "../common/TracyAlloc.hpp"

namespace tracy
{

    class GPUS2Ctx
    {
        friend class GPUS2ZoneScope;

        enum { QueryCount = 64 * 1024 };

    public:
        GPUS2Ctx()
            : m_context( GetGpuCtxCounter().fetch_add( 1, std::memory_order_relaxed ) )
            , m_idx( 0 )
        {
            assert( m_context != 255 );

            int64_t tcpu = Profiler::GetTime();
            int64_t tgpu = Profiler::GetTime();     // callibrate

            uint8_t flags = 0;

            const float period = 1.f;
            auto *item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::GpuNewContext );
            MemWrite( &item->gpuNewContext.cpuTime, tcpu );
            MemWrite( &item->gpuNewContext.gpuTime, tgpu );
            memset( &item->gpuNewContext.thread, 0, sizeof( item->gpuNewContext.thread ) );
            MemWrite( &item->gpuNewContext.period, period );
            MemWrite( &item->gpuNewContext.context, m_context );
            MemWrite( &item->gpuNewContext.flags, flags );
            MemWrite( &item->gpuNewContext.type, GpuContextType::Direct3D11 );

#ifdef TRACY_ON_DEMAND
            GetProfiler().DeferItem( *item );
#endif

            Profiler::QueueSerialFinish();
        }

        ~GPUS2Ctx()
        {
        }

        void Name( const char *name, uint16_t len )
        {
            auto ptr = ( char * ) tracy_malloc( len );
            memcpy( ptr, name, len );

            auto item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::GpuContextName );
            MemWrite( &item->gpuContextNameFat.context, m_context );
            MemWrite( &item->gpuContextNameFat.ptr, ( uint64_t ) ptr );
            MemWrite( &item->gpuContextNameFat.size, len );
#ifdef TRACY_ON_DEMAND
            GetProfiler().DeferItem( *item );
#endif
            Profiler::QueueSerialFinish();
        }

        void Collect(uint16_t queryId, int64_t time)
        {

#ifdef TRACY_ON_DEMAND
            if ( !GetProfiler().IsConnected() )
            {
                m_idx = 0;
                return;
            }
#endif
            auto *item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::GpuTime );
            MemWrite( &item->gpuTime.gpuTime, time );
            MemWrite( &item->gpuTime.queryId, queryId );
            MemWrite( &item->gpuTime.context, m_context );
            Profiler::QueueSerialFinish();

            
        }

    private:
        tracy_force_inline unsigned int NextQueryId()
        {
            const auto id = m_idx;
            m_idx = ( m_idx + 1 ) % QueryCount;
            return id;
        }


        tracy_force_inline uint8_t GetId() const
        {
            return m_context;
        }


        uint8_t m_context;
        unsigned int m_idx;

    };

    class GPUS2ZoneScope
    {
    public:
        tracy_force_inline GPUS2ZoneScope( GPUS2Ctx *ctx, const SourceLocationData *srcloc, bool is_active )
#ifdef TRACY_ON_DEMAND
            : m_active( is_active &&GetProfiler().IsConnected() )
#else
            : m_active( is_active )
#endif
        {
            if ( !m_active ) return;
            m_ctx = ctx;

            const auto queryId = ctx->NextQueryId();

            auto *item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::GpuZoneBeginSerial );
            MemWrite( &item->gpuZoneBegin.cpuTime, Profiler::GetTime() );
            MemWrite( &item->gpuZoneBegin.srcloc, ( uint64_t ) srcloc );
            MemWrite( &item->gpuZoneBegin.thread, GetThreadHandle() );
            MemWrite( &item->gpuZoneBegin.queryId, uint16_t( queryId ) );
            MemWrite( &item->gpuZoneBegin.context, ctx->GetId() );

            Profiler::QueueSerialFinish();
        }

        tracy_force_inline GPUS2ZoneScope( GPUS2Ctx *ctx, const SourceLocationData *srcloc, int depth, bool is_active )
#ifdef TRACY_ON_DEMAND
            : m_active( is_active &&GetProfiler().IsConnected() )
#else
            : m_active( is_active )
#endif
        {
            if ( !m_active ) return;
            m_ctx = ctx;

            const auto queryId = ctx->NextQueryId();

            auto *item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::GpuZoneBeginCallstackSerial );
            MemWrite( &item->gpuZoneBegin.cpuTime, Profiler::GetTime() );
            MemWrite( &item->gpuZoneBegin.srcloc, ( uint64_t ) srcloc );
            MemWrite( &item->gpuZoneBegin.thread, GetThreadHandle() );
            MemWrite( &item->gpuZoneBegin.queryId, uint16_t( queryId ) );
            MemWrite( &item->gpuZoneBegin.context, ctx->GetId() );

            Profiler::QueueSerialFinish();

            GetProfiler().SendCallstack( depth );
        }

        tracy_force_inline GPUS2ZoneScope( GPUS2Ctx *ctx, uint32_t line, const char *source, size_t sourceSz, const char *function, size_t functionSz, const char *name, size_t nameSz, bool active )
#ifdef TRACY_ON_DEMAND
            : m_active( active &&GetProfiler().IsConnected() )
#else
            : m_active( active )
#endif
        {
            if ( !m_active ) return;
            m_ctx = ctx;

            const auto queryId = ctx->NextQueryId();

            const auto sourceLocation = Profiler::AllocSourceLocation( line, source, sourceSz, function, functionSz, name, nameSz );

            auto *item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::GpuZoneBeginAllocSrcLocSerial );
            MemWrite( &item->gpuZoneBegin.cpuTime, Profiler::GetTime() );
            MemWrite( &item->gpuZoneBegin.srcloc, sourceLocation );
            MemWrite( &item->gpuZoneBegin.thread, GetThreadHandle() );
            MemWrite( &item->gpuZoneBegin.queryId, static_cast< uint16_t >( queryId ) );
            MemWrite( &item->gpuZoneBegin.context, ctx->GetId() );

            Profiler::QueueSerialFinish();
        }

#ifdef TRACY_HAS_CALLSTACK

        tracy_force_inline GPUS2ZoneScope( GPUS2Ctx *ctx, uint32_t line, const char *source, size_t sourceSz, const char *function, size_t functionSz, const char *name, size_t nameSz, int depth, bool active )
#ifdef TRACY_ON_DEMAND
            : m_active( active &&GetProfiler().IsConnected() )
#else
            : m_active( active )
#endif
        {
            if ( !m_active ) return;
            m_ctx = ctx;

            const auto queryId = ctx->NextQueryId();

            const auto sourceLocation = Profiler::AllocSourceLocation( line, source, sourceSz, function, functionSz, name, nameSz );

            auto *item = Profiler::QueueSerialCallstack( Callstack( depth ) );
            MemWrite( &item->hdr.type, QueueType::GpuZoneBeginAllocSrcLocCallstackSerial );
            MemWrite( &item->gpuZoneBegin.cpuTime, Profiler::GetTime() );
            MemWrite( &item->gpuZoneBegin.srcloc, sourceLocation );
            MemWrite( &item->gpuZoneBegin.thread, GetThreadHandle() );
            MemWrite( &item->gpuZoneBegin.queryId, static_cast< uint16_t >( queryId ) );
            MemWrite( &item->gpuZoneBegin.context, ctx->GetId() );

            Profiler::QueueSerialFinish();
        }

#endif

        tracy_force_inline ~GPUS2ZoneScope()
        {
            if ( !m_active ) return;

            const auto queryId = m_ctx->NextQueryId();

            auto *item = Profiler::QueueSerial();
            MemWrite( &item->hdr.type, QueueType::GpuZoneEndSerial );
            MemWrite( &item->gpuZoneEnd.cpuTime, Profiler::GetTime() );
            MemWrite( &item->gpuZoneEnd.thread, GetThreadHandle() );
            MemWrite( &item->gpuZoneEnd.queryId, uint16_t( queryId ) );
            MemWrite( &item->gpuZoneEnd.context, m_ctx->GetId() );

            Profiler::QueueSerialFinish();
        }

    private:
        const bool m_active;

        GPUS2Ctx *m_ctx;
    };

    static inline GPUS2Ctx *CreateGPUS2Context()
    {
        auto ctx = ( GPUS2Ctx * ) tracy_malloc( sizeof( GPUS2Ctx ) );
        new( ctx ) GPUS2Ctx( );
        return ctx;
    }

    static inline void DestroyGPUS2Context( GPUS2Ctx *ctx )
    {
        ctx->~GPUS2Ctx();
        tracy_free( ctx );
    }
}

using TracyGPUS2Ctx = tracy::GPUS2Ctx *;

#define TracyGPUS2Context(  ) tracy::CreateGPUS2Context(  );
#define TracyGPUS2Destroy(ctx) tracy::DestroyGPUS2Context(ctx);
#define TracyGPUS2ContextName(ctx, name, size) ctx->Name(name, size);

#if defined TRACY_HAS_CALLSTACK && defined TRACY_CALLSTACK
#  define TracyGPUS2Zone( ctx, name ) TracyGPUS2NamedZoneS( ctx, ___tracy_gpu_zone, name, TRACY_CALLSTACK, true )
#  define TracyGPUS2ZoneC( ctx, name, color ) TracyGPUS2NamedZoneCS( ctx, ___tracy_gpu_zone, name, color, TRACY_CALLSTACK, true )
#  define TracyGPUS2NamedZone( ctx, varname, name, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,__LINE__) { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; tracy::GPUS2ZoneScope varname( ctx, &TracyConcat(__tracy_gpu_source_location,__LINE__), TRACY_CALLSTACK, active );
#  define TracyGPUS2NamedZoneC( ctx, varname, name, color, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,__LINE__) { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, color }; tracy::GPUS2ZoneScope varname( ctx, &TracyConcat(__tracy_gpu_source_location,__LINE__), TRACY_CALLSTACK, active );
#  define TracyGPUS2ZoneTransient(ctx, varname, name, active) TracyGPUS2ZoneTransientS(ctx, varname, cmdList, name, TRACY_CALLSTACK, active)
#else
#  define TracyGPUS2Zone( ctx, name ) TracyGPUS2NamedZone( ctx, ___tracy_gpu_zone, name, true )
#  define TracyGPUS2ZoneC( ctx, name, color ) TracyGPUS2NamedZoneC( ctx, ___tracy_gpu_zone, name, color, true )
#  define TracyGPUS2NamedZone( ctx, varname, name, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,__LINE__) { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; tracy::GPUS2ZoneScope varname( ctx, &TracyConcat(__tracy_gpu_source_location,__LINE__), active );
#  define TracyGPUS2NamedZoneC( ctx, varname, name, color, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,__LINE__) { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, color }; tracy::GPUS2ZoneScope varname( ctx, &TracyConcat(__tracy_gpu_source_location,__LINE__), active );
#  define TracyGPUS2ZoneTransient(ctx, varname, name, active) tracy::GPUS2ZoneScope varname{ ctx, __LINE__, __FILE__, strlen(__FILE__), __FUNCTION__, strlen(__FUNCTION__), name, strlen(name), active };
#endif

#ifdef TRACY_HAS_CALLSTACK
#  define TracyGPUS2ZoneS( ctx, name, depth ) TracyGPUS2NamedZoneS( ctx, ___tracy_gpu_zone, name, depth, true )
#  define TracyGPUS2ZoneCS( ctx, name, color, depth ) TracyGPUS2NamedZoneCS( ctx, ___tracy_gpu_zone, name, color, depth, true )
#  define TracyGPUS2NamedZoneS( ctx, varname, name, depth, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,__LINE__) { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; tracy::GPUS2ZoneScope varname( ctx, &TracyConcat(__tracy_gpu_source_location,__LINE__), depth, active );
#  define TracyGPUS2NamedZoneCS( ctx, varname, name, color, depth, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,__LINE__) { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, color }; tracy::GPUS2ZoneScope varname( ctx, &TracyConcat(__tracy_gpu_source_location,__LINE__), depth, active );
#  define TracyGPUS2ZoneTransientS(ctx, varname, name, depth, active) tracy::GPUS2ZoneScope varname{ ctx, __LINE__, __FILE__, strlen(__FILE__), __FUNCTION__, strlen(__FUNCTION__), name, strlen(name), depth, active };
#else
#  define TracyGPUS2ZoneS( ctx, name, depth, active ) TracyGPUS2Zone( ctx, name )
#  define TracyGPUS2ZoneCS( ctx, name, color, depth, active ) TracyGPUS2ZoneC( name, color )
#  define TracyGPUS2NamedZoneS( ctx, varname, name, depth, active ) TracyGPUS2NamedZone( ctx, varname, name, active )
#  define TracyGPUS2NamedZoneCS( ctx, varname, name, color, depth, active ) TracyGPUS2NamedZoneC( ctx, varname, name, color, active )
#  define TracyGPUS2ZoneTransientS(ctx, varname, name, depth, active) TracyD3D12ZoneTransient(ctx, varname, name, active)
#endif

#define TracyGPUS2Collect( ctx ) ctx->Collect();

#endif

#endif
