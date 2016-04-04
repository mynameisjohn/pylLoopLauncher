#include "LoopLauncher.h"

#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include <pyliason.h>

#include <iostream>

// Implemented below
void InitPython();

int main( int argc, char ** argv )
{
	// Init pyliaison and exposed objects
	InitPython();

	// Load the driver script
	pyl::Object driverScript = pyl::Object::from_script( "../scripts/driver.py" );

	// Create and initialize the loop launcher
	LoopLauncher ll;
	driverScript.call_function( "Initialize", &ll );

	// Loop until the driver script says to stop
	bool loop = true;
	while ( loop )
	{
		// Call the update function with a pointer to the loop launcher
		driverScript.call_function( "Update", &ll ).convert( loop );

		// Sleep 10 milliseconds
		sf::sleep( sf::milliseconds( 10 ) );
	}

	return 0;
}

// returns true if the queried key k is pressed
bool sfmlIsKeyDown( int k )
{
	return sf::Keyboard::isKeyPressed( (sf::Keyboard::Key)k );
}

// Initialize pyliaison, looplauncher functions, sflm module def
void InitPython()
{
	using namespace pyl;

	// Initialize looplauncher python expose stuff
	LoopLauncher::PylInit();

	// Create a mini SFML module for basic keyboard access
	ModuleDef * pSFMLKeysDef = ModuleDef::CreateModuleDef<struct stLoopLauncherModule>( "pylSFMLKeys" );
	if ( pSFMLKeysDef != nullptr )
	{
		pSFMLKeysDef->RegisterFunction<struct st_fnIsKeyDown_t>( "IsKeyDown", make_function( sfmlIsKeyDown ) );
	}

	// Same but for basic time access, and register the time class and some useful conversion functions
	ModuleDef * pSFMLTimeDef = ModuleDef::CreateModuleDef<struct st_sfmlTime_t>( "pylSFMLTime" );

	pSFMLTimeDef->RegisterClass<sf::Time>( "SFMLTime" );

	std::function<sf::Int64( sf::Time * )> fnSFMLTimeAsuS = &sf::Time::asMicroseconds;
	pSFMLTimeDef->RegisterMemFunction<sf::Time, struct st_fnsfmlTimeAsuS>( "AsMicroseconds", fnSFMLTimeAsuS );

	std::function<sf::Int32( sf::Time * )> fnSFMLTimeAsmS = &sf::Time::asMilliseconds;
	pSFMLTimeDef->RegisterMemFunction<sf::Time, struct st_fnsfmlTimeAsMS>( "AsMilliseconds", fnSFMLTimeAsmS );

	std::function<float( sf::Time * )> fnSFMLTimeAsS = &sf::Time::asSeconds;
	pSFMLTimeDef->RegisterMemFunction<sf::Time, struct st_fnsfmlTimeAsS>( "AsSeconds", fnSFMLTimeAsS );

	// Initialize pyliaison
	pyl::initialize();

	// Expose key characters into the sfml keyboard module created above
	if ( pSFMLKeysDef != nullptr )
	{
		// A to Z
		for ( int i = (int) sf::Keyboard::Key::A; i <= (int) sf::Keyboard::Key::Z; i++ )
		{
			std::string K;
			K += (char) ('A' + i - (int) sf::Keyboard::Key::A);
			pSFMLKeysDef->AsObject().set_attr( K, i );
		}

		// as well as the number keys
		for ( int i = (int) sf::Keyboard::Key::Num0; i <= (int) sf::Keyboard::Key::Num9; i++ )
		{
			std::string K = "Num";
			K += (char) ('0' + i - (int) sf::Keyboard::Key::Num0);
			pSFMLKeysDef->AsObject().set_attr( K, i );
		}

		// And the escape key
		pSFMLKeysDef->AsObject().set_attr( "ESC", (int) sf::Keyboard::Key::Escape );
	}
}
