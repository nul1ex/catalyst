#include <stdafx.hpp>

int main( )
{
	{
		if ( !g::console.initialize( " " ) )
		{
			return 1;
		}

		if ( !g::input.initialize( ) )
		{
			return 1;
		}

		if ( !g::memory.initialize( L"cs2.exe" ) )
		{
			return 1;
		}

		g::config.init( );
	}

	{
		if ( !g::modules.initialize( ) )
		{
			return 1;
		}

		if ( !g::offsets.initialize( ) )
		{
			return 1;
		}
	}

	{
		features::skinchanger::g_skindb.initialize();
		std::thread( threads::game ).detach( );
		std::thread( threads::combat ).detach( );
		std::thread( threads::write ).detach( );
		std::thread( threads::skinchanger ).detach( );


		if ( !g::render.initialize( ) )
		{
			return 1;
		}
	}

	return 0;
}