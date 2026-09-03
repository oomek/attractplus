#ifndef FE_COLOR_HPP
#define FE_COLOR_HPP

#include <SFML/Graphics/Color.hpp>
#include <cstdint>
#include <string>

struct Color
{
	std::uint8_t r;
	std::uint8_t g;
	std::uint8_t b;
	std::uint8_t a;

	constexpr Color()
		: r( 0 ),
		  g( 0 ),
		  b( 0 ),
		  a( 255 )
	{
	}

	constexpr Color( std::uint8_t pr, std::uint8_t pg, std::uint8_t pb, std::uint8_t pa = 255 )
		: r( pr ),
		  g( pg ),
		  b( pb ),
		  a( pa )
	{
	}

	Color( const sf::Color &c )
		: r( c.r ),
		  g( c.g ),
		  b( c.b ),
		  a( c.a )
	{
	}

	operator sf::Color() const { return sf::Color( r, g, b, a ); }

	static const Color Transparent;
	static const Color Black;
	static const Color White;
	static const Color Red;
	static const Color Green;
	static const Color Blue;
	static const Color Yellow;
	static const Color Magenta;
	static const Color Cyan;
};

[[nodiscard]] constexpr bool operator==( const Color &lhs, const Color &rhs )
{
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

[[nodiscard]] constexpr bool operator!=( const Color &lhs, const Color &rhs )
{
	return !( lhs == rhs );
}

[[nodiscard]] constexpr Color operator*( const Color &lhs, const Color &rhs )
{
	return Color(
		static_cast<std::uint8_t>( static_cast<unsigned int>( lhs.r ) * rhs.r / 255u ),
		static_cast<std::uint8_t>( static_cast<unsigned int>( lhs.g ) * rhs.g / 255u ),
		static_cast<std::uint8_t>( static_cast<unsigned int>( lhs.b ) * rhs.b / 255u ),
		static_cast<std::uint8_t>( static_cast<unsigned int>( lhs.a ) * rhs.a / 255u ) );
}

inline constexpr Color Color::Transparent{ 0, 0, 0, 0 };
inline constexpr Color Color::Black{ 0, 0, 0, 255 };
inline constexpr Color Color::White{ 255, 255, 255, 255 };
inline constexpr Color Color::Red{ 255, 0, 0, 255 };
inline constexpr Color Color::Green{ 0, 255, 0, 255 };
inline constexpr Color Color::Blue{ 0, 0, 255, 255 };
inline constexpr Color Color::Yellow{ 255, 255, 0, 255 };
inline constexpr Color Color::Magenta{ 255, 0, 255, 255 };
inline constexpr Color Color::Cyan{ 0, 255, 255, 255 };

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
