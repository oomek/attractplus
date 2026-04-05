#ifndef FE_FONT_HPP
#define FE_FONT_HPP

#include <SFML/Graphics/Glyph.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/InputStream.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <cstddef>
#include <cstdint>

class FeFont
{
public:
	struct TexturePageId
	{
		const FeFont *font;
		unsigned int character_size;
	};

	FeFont();
	~FeFont();

	bool openFromFile( const std::filesystem::path &filename );
	bool openFromMemory( const void *data, std::size_t size_in_bytes );
	bool openFromStream( sf::InputStream &stream );
	void clear();

	const sf::Glyph &getGlyph( char32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness = 0 ) const;
	bool hasGlyph( char32_t codePoint ) const;
	float getKerning( std::uint32_t first, std::uint32_t second, unsigned int characterSize, bool bold = false ) const;
	float getLineSpacing( unsigned int characterSize ) const;
	float getUnderlinePosition( unsigned int characterSize ) const;
	float getUnderlineThickness( unsigned int characterSize ) const;

	const TexturePageId *getTexturePageId( unsigned int characterSize ) const;
	std::uint64_t getTextureVersion( unsigned int characterSize ) const;
	bool getTextureSize( unsigned int characterSize, unsigned int &width, unsigned int &height ) const;
	bool copyTexturePixels( unsigned int characterSize, std::vector<unsigned char> &pixels, unsigned int &width, unsigned int &height ) const;
	bool copyTexturePixelsTo( unsigned int characterSize, void *pixels, std::size_t pixel_count, unsigned int &width, unsigned int &height ) const;

	void setSmooth( bool smooth );
	bool isSmooth() const;

private:
	struct Row
	{
		Row( unsigned int row_top, unsigned int row_height );

		unsigned int width;
		unsigned int top;
		unsigned int height;
	};

	using GlyphTable = std::unordered_map<std::uint64_t, sf::Glyph>;

	struct Page
	{
		Page( bool smooth, const FeFont *owner, unsigned int size );

		GlyphTable glyphs;
		TexturePageId texture_id;
		unsigned int width;
		unsigned int height;
		unsigned int nextRow;
		std::vector<Row> rows;
		std::vector<std::uint8_t> pixels;
		std::uint64_t version;
	};

	struct FontHandles;
	using PageTable = std::unordered_map<unsigned int, Page>;

	void cleanup();
	bool openFromStreamImpl( sf::InputStream &stream, const std::string &type );
	Page &loadPage( unsigned int characterSize ) const;
	sf::Glyph loadGlyph( char32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness ) const;
	sf::IntRect findGlyphRect( Page &page, sf::Vector2u size ) const;
	bool setCurrentSize( unsigned int characterSize ) const;
	bool resizePage( Page &page, unsigned int width, unsigned int height ) const;
	void markPageDirty( Page &page ) const;

	std::shared_ptr<FontHandles> m_fontHandles;
	bool m_isSmooth;
	mutable PageTable m_pages;
	mutable std::vector<std::uint8_t> m_pixelBuffer;
	std::shared_ptr<sf::InputStream> m_stream;
};

#endif
