#ifndef FE_COLOR_HPP
#define FE_COLOR_HPP

#include "fe_types.hpp"
#include <string>

class FeColor
{
public:
	FeColor();
	FeColor( Color &c );
	FeColor( const std::string &s );
	FeColor( const uint8_t r, const uint8_t g, const uint8_t b, const int16_t a = -1 );

	bool setColor( Color &c );
	Color getColor();

	bool fromRgb( const uint8_t r, const uint8_t g, const uint8_t b, const int16_t a = -1 );
	bool fromRgb( const std::string &s );
	bool fromHex( const std::string &s );
	bool fromString( const std::string &s );

	bool hasAlpha();

	std::string toRgbString();
	std::string toRgbaString();
	std::string toHexString();
	std::string toHexaString();

private:
	Color m_color{Color::Transparent};
	bool m_has_alpha{true};
};

#endif // FE_COLOR_HPP
