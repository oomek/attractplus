#include "fe_color.hpp"
#include <sstream>
#include <iomanip>

namespace {
	inline bool is_digit( char c )
	{
		return c >= '0' && c <= '9';
	}

	inline bool is_hex( char c )
	{
		return is_digit( c ) || ( c >= 'A' && c <= 'F' ) || ( c >= 'a' && c <= 'f' );
	}

	inline bool is_delim( char c )
	{
		return c == ' ' || c == ',';
	}

	bool parse_component( const std::string &value, std::uint8_t &component )
	{
		if ( value.empty() )
			return false;

		unsigned int result = 0;
		for ( char c : value )
		{
			result = ( result * 10 ) + static_cast<unsigned int>( c - '0' );
			if ( result > 255 )
				return false;
		}

		component = static_cast<std::uint8_t>( result );
		return true;
	}
}

FeColor::FeColor()
{
}

FeColor::FeColor( Color &c )
{
	setColor( c );
}

FeColor::FeColor( const std::string &s )
{
	fromString( s );
}

FeColor::FeColor( const uint8_t r, const uint8_t g, const uint8_t b, const int16_t a )
{
	fromRgb( r, g, b, a );
}

// Return Color
Color FeColor::getColor()
{
	return m_color;
}

// Set Color
bool FeColor::setColor( Color &c )
{
	m_color = c;
	m_has_alpha = true;
	return true;
}

// Try all accepted string formats to parse colour
bool FeColor::fromString( const std::string &s )
{
	return fromHex( s ) || fromRgb( s );
}

// Set color from hex string: #RGB, #RGBA, #RRGGBB, #RRGGBBAA
bool FeColor::fromHex( const std::string &s )
{
	size_t n = s.size();

	if ( n < 4 )
		return false;

	if ( s[0] != '#' )
		return false;

	for ( size_t i = 1; i < n; ++i )
	{
		if ( is_hex( s[i] ) ) continue;
		return false;
	}

	std::stringstream hex;
	if ( n == 4 ) // #RGB
		hex << "0x" << s[1] << s[1] << s[2] << s[2] << s[3] << s[3] << "FF";
	else if ( n == 5 ) // #RGBA
		hex << "0x" << s[1] << s[1] << s[2] << s[2] << s[3] << s[3] << s[4] << s[4];
	else if ( n == 7 ) // #RRGGBB
		hex << "0x" << s[1] << s[2] << s[3] << s[4] << s[5] << s[6] << "FF";
	else if ( n == 9 ) // #RRGGBBAA
		hex << "0x" << s[1] << s[2] << s[3] << s[4] << s[5] << s[6] << s[7] << s[8];
	else
		return false;

	m_has_alpha = ( n == 5 || n == 9 );
	const std::uint32_t rgba = static_cast<std::uint32_t>( std::stoul( hex.str(), nullptr, 16 ) );
	m_color = Color(
		static_cast<std::uint8_t>( ( rgba >> 24 ) & 0xff ),
		static_cast<std::uint8_t>( ( rgba >> 16 ) & 0xff ),
		static_cast<std::uint8_t>( ( rgba >> 8 ) & 0xff ),
		static_cast<std::uint8_t>( rgba & 0xff ) );
	return true;
}

// Set color from rgb string: "r,g,b", "r,g,b,a"
bool FeColor::fromRgb( const std::string &s )
{
	size_t i = 0;
	size_t n = s.size();
	std::string r = "";
	std::string g = "";
	std::string b = "";
	std::string a = "";

	while ( i < n && is_digit( s[i] ) ) { r += s[i]; ++i; }
	while ( i < n && is_delim( s[i] ) ) ++i;
	while ( i < n && is_digit( s[i] ) ) { g += s[i]; ++i; }
	while ( i < n && is_delim( s[i] ) ) ++i;
	while ( i < n && is_digit( s[i] ) ) { b += s[i]; ++i; }
	while ( i < n && is_delim( s[i] ) ) ++i;
	while ( i < n && is_digit( s[i] ) ) { a += s[i]; ++i; }

	if ( i != n )
		return false;

	std::uint8_t red;
	std::uint8_t green;
	std::uint8_t blue;
	std::uint8_t alpha;
	if ( !parse_component( r, red )
			|| !parse_component( g, green )
			|| !parse_component( b, blue )
			|| ( !a.empty() && !parse_component( a, alpha ) ) )
		return false;

	return fromRgb( red, green, blue, a.empty() ? -1 : alpha );
}

// Set color from r,g,b,a
bool FeColor::fromRgb( const uint8_t r, const uint8_t g, const uint8_t b, const int16_t a )
{
	m_has_alpha = a >= 0;
	m_color = Color( r, g, b, m_has_alpha ? a : 255 );
	return true;
}

// Return true if alpha has been explicitly set using set_color or from*
bool FeColor::hasAlpha()
{
	return m_has_alpha;
}

// Return rgb string
std::string FeColor::toRgbString()
{
	std::stringstream str;
	str << (int)m_color.r << ","
		<< (int)m_color.g << ","
		<< (int)m_color.b;
	return str.str();
}

// Return rgba string
std::string FeColor::toRgbaString()
{
	std::stringstream str;
	str << (int)m_color.r << ","
		<< (int)m_color.g << ","
		<< (int)m_color.b << ","
		<< (int)m_color.a;
	return str.str();
}

// Return hex string
std::string FeColor::toHexString()
{
	std::stringstream str;
	str << std::setfill('0')
		<< "#"
		<< std::hex << std::setw(2) << static_cast<unsigned int>( m_color.r )
		<< std::hex << std::setw(2) << static_cast<unsigned int>( m_color.g )
		<< std::hex << std::setw(2) << static_cast<unsigned int>( m_color.b );
	return str.str();
}

// Return hex string with alpha
std::string FeColor::toHexaString()
{
	std::stringstream str;
	str << std::setfill('0')
		<< "#"
		<< std::hex << std::setw(2) << static_cast<unsigned int>( m_color.r )
		<< std::hex << std::setw(2) << static_cast<unsigned int>( m_color.g )
		<< std::hex << std::setw(2) << static_cast<unsigned int>( m_color.b )
		<< std::hex << std::setw(2) << static_cast<unsigned int>( m_color.a );
	return str.str();
}
