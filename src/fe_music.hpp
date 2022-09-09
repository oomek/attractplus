/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2022 Andrew Mickelson
 *
 *  This file is part of Attract-Mode.
 *
 *  Attract-Mode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef FE_MUSIC_HPP
#define FE_MUSIC_HPP

#include <SFML/Audio.hpp>
#include <cstring>

class sfMusic : public sf::Music
{
public:
    using sf::Music::setProcessingInterval;
};

class FeMusic
{
private:
	FeMusic( const FeMusic & );
	FeMusic &operator=( const FeMusic & );

	sfMusic m_music;

	std::string m_file_name;
	bool m_play_state;
	float m_volume;

public:
	FeMusic( bool loop=false );
	~FeMusic();

	void load( const std::string &fn );

    void set_file_name( const char * );
	const char *get_file_name();

	float get_volume();
	void set_volume( float );

	bool get_playing();
	void set_playing( bool );

    float get_pitch();
	void set_pitch( float );

	bool get_loop();
	void set_loop( bool );

	float get_x();
	float get_y();
	float get_z();
	void set_x( float );
	void set_y( float );
	void set_z( float );

	int get_duration();
	int get_time();
};

#endif // FE_MUSIC_HPP