#pragma once

#define WIN32_LEAN_AND_MEAN

#include "windows.h"


#define OUTPUTDEBUGSTRING OutputDebugStringA
#define OUTPUTCONSOLESTRING printf		

FORCEINLINE void Msg( const char* fmt, ... )
{
	char		buff_ca[ 1024 ];
	char*		arg_cp;
	arg_cp = (char*)&fmt + sizeof( fmt );
	int ret = vsnprintf( buff_ca, 1024, fmt, arg_cp );
	if (ret > 0)
	{
		OUTPUTDEBUGSTRING( buff_ca );
		OUTPUTCONSOLESTRING( buff_ca );
	}
}
