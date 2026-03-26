#include "fe_font.hpp"

#include <SFML/System/Err.hpp>
#include <SFML/System/FileInputStream.hpp>
#include <SFML/System/MemoryInputStream.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H
#include FT_STROKER_H

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <string_view>

namespace
{
unsigned long ft_read( FT_Stream rec, unsigned long offset, unsigned char *buffer, unsigned long count )
{
	auto *stream = static_cast<sf::InputStream *>( rec->descriptor.pointer );
	if ( stream->seek( offset ) == offset )
	{
		if ( count > 0 )
			return static_cast<unsigned long>( stream->read( reinterpret_cast<char *>( buffer ), count ).value() );

		return 0;
	}

	return count > 0 ? 0 : 1;
}

void ft_close( FT_Stream )
{
}

template <typename T, typename U>
inline T reinterpret_bits( const U &input )
{
	T output;
	std::memcpy( &output, &input, sizeof( U ) );
	return output;
}

std::uint64_t combine_glyph_key( float outlineThickness, bool bold, std::uint32_t index )
{
	return ( std::uint64_t{ reinterpret_bits<std::uint32_t>( outlineThickness ) } << 32 ) |
		( std::uint64_t{ bold } << 31 ) |
		index;
}
}

struct FeFont::FontHandles
{
	FontHandles() = default;
	~FontHandles()
	{
		FT_Stroker_Done( stroker );
		FT_Done_Face( face );
		FT_Done_FreeType( library );
	}

	FontHandles( const FontHandles & ) = delete;
	FontHandles &operator=( const FontHandles & ) = delete;

	FT_Library library{};
	FT_StreamRec streamRec{};
	FT_Face face{};
	FT_Stroker stroker{};
};

FeFont::Row::Row( unsigned int row_top, unsigned int row_height )
	: width( 0 ),
	  top( row_top ),
	  height( row_height )
{
}

FeFont::Page::Page( bool smooth, const FeFont *owner, unsigned int size )
	: width( 128 ),
	  height( 128 ),
	  nextRow( 3 ),
	  version( 1 )
{
	texture_id.font = owner;
	texture_id.character_size = size;

	pixels.resize( static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 4, 0 );
	for ( std::size_t i = 0; i < pixels.size(); i += 4 )
	{
		pixels[ i + 0 ] = 255;
		pixels[ i + 1 ] = 255;
		pixels[ i + 2 ] = 255;
		pixels[ i + 3 ] = 0;
	}

	for ( unsigned int y = 0; y < 2; ++y )
		for ( unsigned int x = 0; x < 2; ++x )
		{
			const std::size_t index = static_cast<std::size_t>( x + y * width ) * 4;
			pixels[ index + 0 ] = 255;
			pixels[ index + 1 ] = 255;
			pixels[ index + 2 ] = 255;
			pixels[ index + 3 ] = 255;
		}
}

FeFont::FeFont()
	: m_isSmooth( true )
{
}

FeFont::~FeFont()
{
	cleanup();
}

void FeFont::clear()
{
	cleanup();
}

bool FeFont::openFromFile( const std::filesystem::path &filename )
{
	using namespace std::string_view_literals;
	cleanup();

	const auto stream = std::make_shared<sf::FileInputStream>();
	if ( !stream->open( filename ) )
	{
		sf::err() << "Failed to load font (failed to open file): " << std::strerror( errno ) << '\n'
			<< filename.string() << std::endl;
		return false;
	}

	if ( openFromStreamImpl( *stream, "file" ) )
	{
		m_stream = stream;
		return true;
	}

	sf::err() << filename.string() << std::endl;
	return false;
}

bool FeFont::openFromMemory( const void *data, std::size_t size_in_bytes )
{
	cleanup();
	if ( !data )
	{
		sf::err() << "Failed to load font from memory (provided data pointer is null)" << std::endl;
		return false;
	}

	const auto memory_stream = std::make_shared<sf::MemoryInputStream>( data, size_in_bytes );
	if ( openFromStreamImpl( *memory_stream, "memory" ) )
	{
		m_stream = memory_stream;
		return true;
	}

	return false;
}

bool FeFont::openFromStream( sf::InputStream &stream )
{
	if ( !stream.seek( 0 ).has_value() )
	{
		sf::err() << "Failed to seek font stream" << std::endl;
		return false;
	}

	return openFromStreamImpl( stream, "stream" );
}

bool FeFont::openFromStreamImpl( sf::InputStream &stream, const std::string &type )
{
	cleanup();

	auto font_handles = std::make_shared<FontHandles>();
	if ( FT_Init_FreeType( &font_handles->library ) != 0 )
	{
		sf::err() << "Failed to load font from " << type << " (failed to initialize FreeType)" << std::endl;
		return false;
	}

	font_handles->streamRec.base = nullptr;
	font_handles->streamRec.size = static_cast<unsigned long>( stream.getSize().value() );
	font_handles->streamRec.pos = 0;
	font_handles->streamRec.descriptor.pointer = &stream;
	font_handles->streamRec.read = &ft_read;
	font_handles->streamRec.close = &ft_close;

	FT_Open_Args args = {};
	args.flags = FT_OPEN_STREAM;
	args.stream = &font_handles->streamRec;

	if ( FT_Open_Face( font_handles->library, &args, 0, &font_handles->face ) != 0 )
	{
		sf::err() << "Failed to load font from " << type << " (failed to create the font face)" << std::endl;
		return false;
	}

	if ( FT_Stroker_New( font_handles->library, &font_handles->stroker ) != 0 )
	{
		sf::err() << "Failed to load font from " << type << " (failed to create the stroker)" << std::endl;
		return false;
	}

	if ( FT_Select_Charmap( font_handles->face, FT_ENCODING_UNICODE ) != 0 )
	{
		sf::err() << "Failed to load font from " << type << " (failed to set the Unicode character set)" << std::endl;
		return false;
	}

	m_fontHandles = font_handles;
	return true;
}

void FeFont::cleanup()
{
	m_fontHandles.reset();
	m_pages.clear();
	std::vector<std::uint8_t>().swap( m_pixelBuffer );
	m_stream.reset();
}

bool FeFont::hasGlyph( char32_t codePoint ) const
{
	return FT_Get_Char_Index( m_fontHandles ? m_fontHandles->face : nullptr, codePoint ) != 0;
}

const sf::Glyph &FeFont::getGlyph( char32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness ) const
{
	GlyphTable &glyphs = loadPage( characterSize ).glyphs;
	const std::uint64_t key = combine_glyph_key(
		outlineThickness,
		bold,
		FT_Get_Char_Index( m_fontHandles ? m_fontHandles->face : nullptr, codePoint ) );

	GlyphTable::const_iterator it = glyphs.find( key );
	if ( it != glyphs.end() )
		return it->second;

	const sf::Glyph glyph = loadGlyph( codePoint, characterSize, bold, outlineThickness );
	return glyphs.try_emplace( key, glyph ).first->second;
}

float FeFont::getKerning( std::uint32_t first, std::uint32_t second, unsigned int characterSize, bool bold ) const
{
	if ( first == 0 || second == 0 )
		return 0.0f;

	FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;
	if ( face && setCurrentSize( characterSize ) )
	{
		const FT_UInt index1 = FT_Get_Char_Index( face, first );
		const FT_UInt index2 = FT_Get_Char_Index( face, second );
		const float first_rsb = static_cast<float>( getGlyph( first, characterSize, bold ).rsbDelta );
		const float second_lsb = static_cast<float>( getGlyph( second, characterSize, bold ).lsbDelta );

		FT_Vector kerning = { 0, 0 };
		if ( FT_HAS_KERNING( face ) )
			FT_Get_Kerning( face, index1, index2, FT_KERNING_UNFITTED, &kerning );

		if ( !FT_IS_SCALABLE( face ) )
			return static_cast<float>( kerning.x );

		return std::floor( ( second_lsb - first_rsb + static_cast<float>( kerning.x ) + 32 ) / float{ 1 << 6 } );
	}

	return 0.0f;
}

float FeFont::getLineSpacing( unsigned int characterSize ) const
{
	FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;
	if ( face && setCurrentSize( characterSize ) )
		return static_cast<float>( face->size->metrics.height ) / float{ 1 << 6 };
	return 0.0f;
}

float FeFont::getUnderlinePosition( unsigned int characterSize ) const
{
	FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;
	if ( face && setCurrentSize( characterSize ) )
	{
		if ( !FT_IS_SCALABLE( face ) )
			return static_cast<float>( characterSize ) / 10.0f;

		return -static_cast<float>( FT_MulFix( face->underline_position, face->size->metrics.y_scale ) ) / float{ 1 << 6 };
	}
	return 0.0f;
}

float FeFont::getUnderlineThickness( unsigned int characterSize ) const
{
	FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;
	if ( face && setCurrentSize( characterSize ) )
	{
		if ( !FT_IS_SCALABLE( face ) )
			return static_cast<float>( characterSize ) / 14.0f;

		return static_cast<float>( FT_MulFix( face->underline_thickness, face->size->metrics.y_scale ) ) / float{ 1 << 6 };
	}
	return 0.0f;
}

const FeFont::TexturePageId *FeFont::getTexturePageId( unsigned int characterSize ) const
{
	return &loadPage( characterSize ).texture_id;
}

std::uint64_t FeFont::getTextureVersion( unsigned int characterSize ) const
{
	return loadPage( characterSize ).version;
}

bool FeFont::getTextureSize( unsigned int characterSize, unsigned int &width, unsigned int &height ) const
{
	const Page &page = loadPage( characterSize );
	width = page.width;
	height = page.height;
	return true;
}

bool FeFont::copyTexturePixels( unsigned int characterSize, std::vector<unsigned char> &pixels, unsigned int &width, unsigned int &height ) const
{
	const Page &page = loadPage( characterSize );
	width = page.width;
	height = page.height;
	pixels = page.pixels;
	return true;
}

bool FeFont::copyTexturePixelsTo( unsigned int characterSize, void *pixels, std::size_t pixel_count, unsigned int &width, unsigned int &height ) const
{
	const Page &page = loadPage( characterSize );
	width = page.width;
	height = page.height;

	const std::size_t required = static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 4;
	if ( !pixels )
		return true;
	if ( pixel_count < required )
		return false;

	std::memcpy( pixels, page.pixels.data(), required );
	return true;
}

void FeFont::setSmooth( bool smooth )
{
	if ( smooth == m_isSmooth )
		return;

	m_isSmooth = smooth;
}

bool FeFont::isSmooth() const
{
	return m_isSmooth;
}

FeFont::Page &FeFont::loadPage( unsigned int characterSize ) const
{
	return m_pages.try_emplace( characterSize, m_isSmooth, this, characterSize ).first->second;
}

sf::Glyph FeFont::loadGlyph( char32_t codePoint, unsigned int characterSize, bool bold, float outlineThickness ) const
{
	sf::Glyph glyph;
	if ( !m_fontHandles || !m_fontHandles->face )
		return glyph;

	FT_Face face = m_fontHandles->face;
	if ( !setCurrentSize( characterSize ) )
		return glyph;

	FT_Int32 flags = FT_LOAD_TARGET_NORMAL | FT_LOAD_FORCE_AUTOHINT;
	if ( outlineThickness != 0 )
		flags |= FT_LOAD_NO_BITMAP;
	if ( FT_Load_Char( face, codePoint, flags ) != 0 )
		return glyph;

	FT_Glyph glyph_desc = nullptr;
	if ( FT_Get_Glyph( face->glyph, &glyph_desc ) != 0 )
		return glyph;

	const FT_Pos weight = 1 << 6;
	const bool outline = ( glyph_desc->format == FT_GLYPH_FORMAT_OUTLINE );
	if ( outline )
	{
		if ( bold )
		{
			FT_OutlineGlyph outline_glyph = reinterpret_cast<FT_OutlineGlyph>( glyph_desc );
			FT_Outline_Embolden( &outline_glyph->outline, weight );
		}

		if ( outlineThickness != 0 )
		{
			FT_Stroker stroker = m_fontHandles->stroker;
			FT_Stroker_Set(
				stroker,
				static_cast<FT_Fixed>( outlineThickness * float{ 1 << 6 } ),
				FT_STROKER_LINECAP_ROUND,
				FT_STROKER_LINEJOIN_ROUND,
				0 );
			FT_Glyph_Stroke( &glyph_desc, stroker, true );
		}
	}

	FT_Glyph_To_Bitmap( &glyph_desc, FT_RENDER_MODE_NORMAL, nullptr, 1 );
	FT_BitmapGlyph bitmap_glyph = reinterpret_cast<FT_BitmapGlyph>( glyph_desc );
	FT_Bitmap &bitmap = bitmap_glyph->bitmap;

	if ( !outline )
	{
		if ( bold )
			FT_Bitmap_Embolden( m_fontHandles->library, &bitmap, weight, weight );

		if ( outlineThickness != 0 )
			sf::err() << "Failed to outline glyph (no fallback available)" << std::endl;
	}

	glyph.advance = static_cast<float>( bitmap_glyph->root.advance.x >> 16 );
	if ( bold )
		glyph.advance += static_cast<float>( weight ) / float{ 1 << 6 };

	glyph.lsbDelta = static_cast<int>( face->glyph->lsb_delta );
	glyph.rsbDelta = static_cast<int>( face->glyph->rsb_delta );

	sf::Vector2u size( bitmap.width, bitmap.rows );
	if ( size.x > 0 && size.y > 0 )
	{
		const unsigned int padding = 2;
		size += 2u * sf::Vector2u( padding, padding );

		Page &page = loadPage( characterSize );
		glyph.textureRect = findGlyphRect( page, size );
		glyph.textureRect.position += sf::Vector2i( padding, padding );
		glyph.textureRect.size -= 2 * sf::Vector2i( padding, padding );

		glyph.bounds.position = sf::Vector2f( sf::Vector2i( bitmap_glyph->left, -bitmap_glyph->top ) );
		glyph.bounds.size = sf::Vector2f( sf::Vector2u( bitmap.width, bitmap.rows ) );

		m_pixelBuffer.resize( static_cast<std::size_t>( size.x ) * static_cast<std::size_t>( size.y ) * 4 );
		for ( std::size_t i = 0; i < m_pixelBuffer.size(); i += 4 )
		{
			m_pixelBuffer[ i + 0 ] = 255;
			m_pixelBuffer[ i + 1 ] = 255;
			m_pixelBuffer[ i + 2 ] = 255;
			m_pixelBuffer[ i + 3 ] = 0;
		}

		const std::uint8_t *src = bitmap.buffer;
		if ( bitmap.pixel_mode == FT_PIXEL_MODE_MONO )
		{
			for ( unsigned int y = padding; y < size.y - padding; ++y )
			{
				for ( unsigned int x = padding; x < size.x - padding; ++x )
				{
					const std::size_t index = static_cast<std::size_t>( x + y * size.x );
					m_pixelBuffer[ index * 4 + 3 ] =
						( ( src[ ( x - padding ) / 8 ] ) & ( 1 << ( 7 - ( ( x - padding ) % 8 ) ) ) ) ? 255 : 0;
				}
				src += bitmap.pitch;
			}
		}
		else
		{
			for ( unsigned int y = padding; y < size.y - padding; ++y )
			{
				for ( unsigned int x = padding; x < size.x - padding; ++x )
				{
					const std::size_t index = static_cast<std::size_t>( x + y * size.x );
					m_pixelBuffer[ index * 4 + 3 ] = src[ x - padding ];
				}
				src += bitmap.pitch;
			}
		}

		const sf::Vector2u dest = sf::Vector2u( glyph.textureRect.position ) - sf::Vector2u( padding, padding );
		const sf::Vector2u update_size = sf::Vector2u( glyph.textureRect.size ) + 2u * sf::Vector2u( padding, padding );
		for ( unsigned int row = 0; row < update_size.y; ++row )
		{
			const std::size_t src_offset = static_cast<std::size_t>( row ) * update_size.x * 4;
			const std::size_t dst_offset = ( static_cast<std::size_t>( dest.y + row ) * page.width + dest.x ) * 4;
			std::memcpy( &page.pixels[ dst_offset ], &m_pixelBuffer[ src_offset ], static_cast<std::size_t>( update_size.x ) * 4 );
		}
		markPageDirty( page );
	}

	FT_Done_Glyph( glyph_desc );
	return glyph;
}

sf::IntRect FeFont::findGlyphRect( Page &page, sf::Vector2u size ) const
{
	Row *row = nullptr;
	float best_ratio = 0.0f;
	for ( std::vector<Row>::iterator it = page.rows.begin(); it != page.rows.end() && !row; ++it )
	{
		const float ratio = static_cast<float>( size.y ) / static_cast<float>( it->height );
		if ( ratio < 0.7f || ratio > 1.0f )
			continue;
		if ( size.x > page.width - it->width )
			continue;
		if ( ratio < best_ratio )
			continue;
		row = &*it;
		best_ratio = ratio;
	}

	if ( !row )
	{
		const unsigned int row_height = size.y + size.y / 10;
		while ( ( page.nextRow + row_height >= page.height ) || ( size.x >= page.width ) )
		{
			const unsigned int max_size = sf::Texture::getMaximumSize();
			if ( ( page.width * 2 <= max_size ) && ( page.height * 2 <= max_size ) )
			{
				if ( !resizePage( page, page.width * 2, page.height * 2 ) )
				{
					sf::err() << "Failed to create new page texture" << std::endl;
					return { { 0, 0 }, { 2, 2 } };
				}
			}
			else
			{
				sf::err() << "Failed to add a new character to the font: the maximum texture size has been reached" << std::endl;
				return { { 0, 0 }, { 2, 2 } };
			}
		}

		page.rows.push_back( Row( page.nextRow, row_height ) );
		page.nextRow += row_height;
		row = &page.rows.back();
	}

	sf::IntRect rect( sf::Rect<unsigned int>( { row->width, row->top }, size ) );
	row->width += size.x;
	return rect;
}

bool FeFont::setCurrentSize( unsigned int characterSize ) const
{
	FT_Face face = m_fontHandles ? m_fontHandles->face : nullptr;
	if ( !face )
		return false;

	const FT_UShort current_size = face->size->metrics.x_ppem;
	if ( current_size != characterSize )
	{
		const FT_Error result = FT_Set_Pixel_Sizes( face, 0, characterSize );
		if ( result == FT_Err_Invalid_Pixel_Size )
		{
			if ( !FT_IS_SCALABLE( face ) )
			{
				sf::err() << "Failed to set bitmap font size to " << characterSize << '\n' << "Available sizes are: ";
				for ( int i = 0; i < face->num_fixed_sizes; ++i )
				{
					const long size = ( face->available_sizes[ i ].y_ppem + 32 ) >> 6;
					sf::err() << size << " ";
				}
				sf::err() << std::endl;
			}
			else
			{
				sf::err() << "Failed to set font size to " << characterSize << std::endl;
			}
		}

		return result == FT_Err_Ok;
	}

	return true;
}

bool FeFont::resizePage( Page &page, unsigned int width, unsigned int height ) const
{
	std::vector<std::uint8_t> new_pixels( static_cast<std::size_t>( width ) * static_cast<std::size_t>( height ) * 4, 0 );
	for ( std::size_t i = 0; i < new_pixels.size(); i += 4 )
	{
		new_pixels[ i + 0 ] = 255;
		new_pixels[ i + 1 ] = 255;
		new_pixels[ i + 2 ] = 255;
		new_pixels[ i + 3 ] = 0;
	}

	for ( unsigned int y = 0; y < page.height; ++y )
	{
		const std::size_t src_offset = static_cast<std::size_t>( y ) * page.width * 4;
		const std::size_t dst_offset = static_cast<std::size_t>( y ) * width * 4;
		std::memcpy( &new_pixels[ dst_offset ], &page.pixels[ src_offset ], static_cast<std::size_t>( page.width ) * 4 );
	}

	page.width = width;
	page.height = height;
	page.pixels.swap( new_pixels );
	markPageDirty( page );
	return true;
}

void FeFont::markPageDirty( Page &page ) const
{
	++page.version;
}
