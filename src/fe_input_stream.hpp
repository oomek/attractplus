/*
 *
 *  Attract-Mode Plus frontend
 *  Copyright (C) 2026 Radek Dutkiewicz
 *
 *  This file is part of Attract-Mode Plus
 *
 *  Attract-Mode Plus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode Plus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode Plus.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef FE_INPUT_STREAM_HPP
#define FE_INPUT_STREAM_HPP

#include <cstddef>
#include <cstring>
#include <optional>

class FeInputStream
{
public:
	virtual ~FeInputStream() = default;

	virtual std::optional<std::size_t> read( void *data, std::size_t size ) = 0;
	virtual std::optional<std::size_t> seek( std::size_t position ) = 0;
	virtual std::optional<std::size_t> tell() = 0;
	virtual std::optional<std::size_t> getSize() = 0;
};

class FeMemoryInputStream : public FeInputStream
{
public:
	FeMemoryInputStream( const void *data, std::size_t size_in_bytes )
		: m_data( static_cast<const unsigned char *>( data ) ),
		  m_size( size_in_bytes ),
		  m_offset( 0 )
	{
	}

	std::optional<std::size_t> read( void *data, std::size_t size ) override
	{
		if ( !data && size > 0 )
			return std::nullopt;

		const std::size_t remaining = ( m_offset < m_size ) ? ( m_size - m_offset ) : 0;
		const std::size_t count = ( size < remaining ) ? size : remaining;
		if ( count > 0 )
			std::memcpy( data, m_data + m_offset, count );

		m_offset += count;
		return count;
	}

	std::optional<std::size_t> seek( std::size_t position ) override
	{
		if ( position > m_size )
			return std::nullopt;

		m_offset = position;
		return m_offset;
	}

	std::optional<std::size_t> tell() override
	{
		return m_offset;
	}

	std::optional<std::size_t> getSize() override
	{
		return m_size;
	}

private:
	const unsigned char *m_data;
	std::size_t m_size;
	std::size_t m_offset;
};

#endif
