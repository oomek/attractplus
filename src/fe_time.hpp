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

#ifndef FE_TIME_HPP
#define FE_TIME_HPP

#include <cmath>
#include <cstdint>
#include <SDL3/SDL_timer.h>

class FeTime
{
public:
	constexpr FeTime()
		: m_microseconds( 0 )
	{
	}

	static constexpr FeTime fromMicroseconds( std::int64_t microseconds )
	{
		return FeTime( microseconds );
	}

	constexpr std::int64_t asMicroseconds() const
	{
		return m_microseconds;
	}

	constexpr std::int64_t asMilliseconds() const
	{
		return m_microseconds / 1000;
	}

	double asSeconds() const
	{
		return static_cast<double>( m_microseconds ) / 1000000.0;
	}

	constexpr FeTime operator-() const
	{
		return FeTime( -m_microseconds );
	}

	constexpr FeTime &operator+=( FeTime other )
	{
		m_microseconds += other.m_microseconds;
		return *this;
	}

	constexpr FeTime &operator-=( FeTime other )
	{
		m_microseconds -= other.m_microseconds;
		return *this;
	}

	friend constexpr FeTime operator+( FeTime lhs, FeTime rhs )
	{
		lhs += rhs;
		return lhs;
	}

	friend constexpr FeTime operator-( FeTime lhs, FeTime rhs )
	{
		lhs -= rhs;
		return lhs;
	}

	friend constexpr bool operator==( FeTime lhs, FeTime rhs )
	{
		return lhs.m_microseconds == rhs.m_microseconds;
	}

	friend constexpr bool operator!=( FeTime lhs, FeTime rhs )
	{
		return !( lhs == rhs );
	}

	friend constexpr bool operator<( FeTime lhs, FeTime rhs )
	{
		return lhs.m_microseconds < rhs.m_microseconds;
	}

	friend constexpr bool operator<=( FeTime lhs, FeTime rhs )
	{
		return lhs.m_microseconds <= rhs.m_microseconds;
	}

	friend constexpr bool operator>( FeTime lhs, FeTime rhs )
	{
		return lhs.m_microseconds > rhs.m_microseconds;
	}

	friend constexpr bool operator>=( FeTime lhs, FeTime rhs )
	{
		return lhs.m_microseconds >= rhs.m_microseconds;
	}

private:
	constexpr explicit FeTime( std::int64_t microseconds )
		: m_microseconds( microseconds )
	{
	}

	std::int64_t m_microseconds;
};

inline constexpr FeTime fe_microseconds( std::int64_t microseconds )
{
	return FeTime::fromMicroseconds( microseconds );
}

inline constexpr FeTime fe_milliseconds( std::int64_t milliseconds )
{
	return fe_microseconds( milliseconds * 1000 );
}

inline FeTime fe_seconds( double seconds )
{
	return fe_microseconds(
		static_cast<std::int64_t>( std::llround( seconds * 1000000.0 ) ) );
}

inline void fe_sleep( FeTime duration )
{
	const std::int64_t microseconds = duration.asMicroseconds();
	if ( microseconds <= 0 )
		return;

	SDL_DelayNS( static_cast<Uint64>( microseconds ) * 1000ULL );
}

class FeClock
{
public:
	FeClock()
		: m_elapsed_ticks( 0 ),
		m_start_ticks( SDL_GetPerformanceCounter() ),
		m_running( true )
		{
	}

	FeTime getElapsedTime() const
	{
		return fe_microseconds( to_microseconds( current_elapsed_ticks() ) );
	}

	void restart()
	{
		m_elapsed_ticks = 0;
		m_start_ticks = SDL_GetPerformanceCounter();
		m_running = true;
	}

	void reset()
	{
		m_elapsed_ticks = 0;
		m_start_ticks = SDL_GetPerformanceCounter();
		m_running = false;
	}

	void start()
	{
		if ( !m_running )
		{
			m_start_ticks = SDL_GetPerformanceCounter();
			m_running = true;
		}
	}

	void stop()
	{
		if ( m_running )
		{
			m_elapsed_ticks = current_elapsed_ticks();
			m_running = false;
		}
	}

	bool isRunning() const
	{
		return m_running;
	}

private:
	static std::int64_t to_microseconds( Uint64 ticks )
	{
		const Uint64 frequency = SDL_GetPerformanceFrequency();
		if ( frequency == 0 )
			return 0;

		return static_cast<std::int64_t>(
			( static_cast<long double>( ticks ) * 1000000.0L )
			/ static_cast<long double>( frequency ) );
	}

	Uint64 current_elapsed_ticks() const
	{
		if ( !m_running )
			return m_elapsed_ticks;

		return m_elapsed_ticks + ( SDL_GetPerformanceCounter() - m_start_ticks );
	}

	Uint64 m_elapsed_ticks;
	Uint64 m_start_ticks;
	bool m_running;
};

#endif
