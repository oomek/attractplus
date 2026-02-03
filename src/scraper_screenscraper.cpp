/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2026 Andrew Mickelson & Radek Dutkiewicz
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

#include "fe_settings.hpp"
#include "fe_util.hpp"
#include "fe_net.hpp"

#include "nowide/fstream.hpp"
#include "nowide/cstdio.hpp"

#include <cstdlib>

#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <sstream>
#include <iomanip>

#ifdef USE_LIBCURL

#include <expat.h>

#ifdef USE_LIBARCHIVE
#include <zlib.h>
#endif

const char *SSHOSTNAME = "https://api.screenscraper.fr/api2";

namespace
{

const std::string SS_DEV_ID{char(78^1),char(110^1),char(108^1),char(100^1),char(106^1)};
const std::string SS_DEV_PASSWORD{char(84^1),char(102^1),char(77^1),char(110^1),char(53^1),char(87^1),char(87^1),char(57^1),char(50^1),char(112^1),char(85^1)};

const std::map<std::string, int> SS_SYSTEM_MAP =
{
	// Atari
	{ "3do", 29 },
	{ "atari 7800", 41 }, { "atari7800", 41 },
	{ "atari 2600", 26 }, { "atari2600", 26 },
	{ "atari 5200", 40 }, { "atari5200", 40 },
	{ "atari 800", 43 }, { "atari800", 43 },
	{ "atari lynx", 28 }, { "atarilynx", 28 },
	{ "atari jaguar", 27 }, { "atarijaguar", 27 },
	{ "atari st", 63 }, { "atarist", 52 },

	// Computer systems
	{ "amiga", 64 },
	{ "amstrad cpc", 65 }, { "amstradcpc", 65 },
	{ "amstrad pcw", 70 }, { "amstradpcw", 70 },
	{ "apple ii", 79 }, { "apple2", 79 },
	{ "apple iigs", 84 }, { "appleiigs", 84 },
	{ "commodore 64", 66 }, { "commodore64", 66 }, { "c64", 66 },
	{ "commodore 128", 71 }, { "commodore128", 71 }, { "c128", 71 },
	{ "msx", 113 },
	{ "vic-20", 72 }, { "vic20", 72 },
	{ "zx spectrum", 74 }, { "zxspectrum", 74 },

	// Nintendo
	{ "nes", 7 }, { "nintendo entertainment system", 7 },
	{ "famicom", 8 },
	{ "super nes", 4 }, { "snes", 4 }, { "super famicom", 4 }, { "sfc", 4 },
	{ "nintendo 64", 14 }, { "n64", 14 }, { "nintendo64", 14 },
	{ "nintendo 64dd", 15 }, { "n64dd", 15 },
	{ "gamecube", 13 }, { "nintendo gamecube", 13 }, { "nintendogamecube", 13 },
	{ "wii", 17 }, { "nintendo wii", 17 }, { "nintendowii", 17 },
	{ "wiiu", 18 }, { "wii u", 18 }, { "nintendo wiiu", 18 }, { "nintendowiiu", 18 },
	{ "switch", 19 }, { "nintendo switch", 19 },
	{ "gameboy", 9 }, { "game boy", 9 }, { "gb", 9 },
	{ "gameboy color", 10 }, { "game boy color", 10 }, { "gbc", 10 },
	{ "gameboy advance", 12 }, { "game boy advance", 12 }, { "gba", 12 },
	{ "virtual boy", 11 }, { "virtualboy", 11 },
	{ "nintendo ds", 16 }, { "nds", 16 }, { "nintendods", 16 },

	// Sega
	{ "genesis", 1 }, { "mega drive", 1 }, { "megadrive", 1 }, { "segadrive", 1 },
	{ "sega cd", 3 }, { "segacd", 3 }, { "megacd", 3 },
	{ "sega 32x", 5 }, { "sega32x", 5 },
	{ "segacd 32x", 6 }, { "segacd32x", 6 },
	{ "sega saturn", 21 }, { "sega_saturn", 21 }, { "saturn", 21 },
	{ "dreamcast", 23 },
	{ "master system", 2 }, { "segamastersystem", 2 }, { "segamaster", 2 }, { "sms", 2 }, { "sega master system", 2 },
	{ "game gear", 20 }, { "segagamegear", 20 }, { "gg", 20 },
	{ "sega pico", 45 }, { "segapico", 45 },

	// Sony
	{ "playstation", 57 }, { "ps1", 57 },
	{ "playstation 2", 22 }, { "playstation2", 22 }, { "ps2", 22 },
	{ "playstation 3", 60 }, { "playstation3", 60 }, { "ps3", 60 },
	{ "psp", 61 },

	// Xbox
	{ "xbox", 59 },
	{ "xbox 360", 25 }, { "xbox360", 25 },

	// NEC
	{ "pc engine", 31 }, { "pc-engine", 31 }, { "pcengine", 31 },
	{ "turbografx-16", 31 }, { "turbografx16", 31 }, { "tg16", 31 },
	{ "pc engine cd", 32 }, { "pc-engine cd", 32 }, { "pcenginecd", 32 },

	// SNK
	{ "neogeo", 142 }, { "neo geo", 142 },

	// Other
	{ "arcade", 75 },
	{ "coleco vision", 47 }, { "colecovision", 47 }, { "coleco", 47 },
	{ "intellivision", 53 },
	{ "odyssey2", 48 }, { "magnavox odyssey2", 48 },
	{ "vectrex", 54 },
	{ "sg-1000", 24 }, { "segasg1000", 24 }, { "sg1000", 24 },
	{ "wonderswan", 34 }, { "wonder swan", 34 }
};

struct SSGameData
{
	std::string name;
	std::string snap;
	std::string marquee;
	std::string flyer;
	std::string wheel;
	std::string fanart;
	std::string video;

	std::string snap_crc;
	std::string marquee_crc;
	std::string flyer_crc;
	std::string wheel_crc;
	std::string fanart_crc;
	std::string video_crc;

	std::string year;
	std::string manufacturer;
	std::string publisher;
	std::string players;
	std::vector<std::string> genres;
	std::string rating;
	std::string description;
	std::string series;
	std::string language;
	std::string region;
};

class SSXMLParser
{
private:
	XML_Parser m_parser;
	enum State
	{
		Root,
		Data,
		Jeux,
		Jeu,
		Medias,
		Media,
		Noms,
		Nom,
		Dates,
		Date,
		Developpeur,
		Editeur,
		Joueurs,
		Genres,
		Genre,
		Synopsis,
		Familles,
		Famille,
		Roms,
		Rom,
		Romregions
	} m_state, m_last_state;

	SSGameData *m_current_game;
	std::vector<SSGameData> *m_games;
	std::string m_current_data;
	std::string m_media_type;
	std::string m_region;
	std::string m_media_crc;

public:
	SSXMLParser( std::vector<SSGameData> &games )
		: m_state( Root ),
		m_last_state( Root ),
		m_games( &games ),
		m_current_game( NULL )
	{
		m_parser = XML_ParserCreate( NULL );
		XML_SetUserData( m_parser, this );
		XML_SetElementHandler( m_parser, start_element, end_element );
		XML_SetCharacterDataHandler( m_parser, char_data );
	}

	~SSXMLParser()
	{
		XML_ParserFree( m_parser );
	}

	bool parse( const std::string &xml )
	{
		m_state = Root;
		m_current_data.clear();
		if ( XML_Parse( m_parser, xml.c_str(), (int)xml.length(), XML_TRUE ) == XML_STATUS_ERROR )
		{
			FeLog() << "XML Parse error: " << XML_ErrorString( XML_GetErrorCode( m_parser ) )
				<< " at line " << XML_GetCurrentLineNumber( m_parser ) << std::endl;
			return false;
		}
		return true;
	}

private:
	static void start_element( void *data, const char *el, const char **attr )
	{
		SSXMLParser *p = (SSXMLParser *)data;
		p->m_last_state = p->m_state;

		if ( strcmp( el, "Data" ) == 0 )
			p->m_state = Data;
		else if ( strcmp( el, "jeux" ) == 0 )
			p->m_state = Jeux;
		else if ( strcmp( el, "jeu" ) == 0 )
		{
			p->m_state = Jeu;
			p->m_games->push_back( SSGameData() );
			p->m_current_game = &(p->m_games->back());
		}
		else if ( strcmp( el, "medias" ) == 0 )
			p->m_state = Medias;
		else if ( strcmp( el, "media" ) == 0 )
		{
			p->m_state = Media;
			p->m_media_type.clear();
			p->m_media_crc.clear();

			// Parse attributes
			for ( int i = 0; attr[i]; i += 2 )
			{
				if ( strcmp( attr[i], "type" ) == 0 )
					p->m_media_type = attr[i+1];
				else if ( strcmp( attr[i], "region" ) == 0 )
					p->m_region = attr[i+1];
				else if ( strcmp( attr[i], "crc" ) == 0 )
					p->m_media_crc = attr[i+1];
			}
		}
		else if ( strcmp( el, "noms" ) == 0 )
			p->m_state = Noms;
		else if ( strcmp( el, "nom" ) == 0 )
			p->m_state = Nom;
		else if ( strcmp( el, "dates" ) == 0 )
			p->m_state = Dates;
		else if ( strcmp( el, "date" ) == 0 )
			p->m_state = Date;
		else if ( strcmp( el, "developpeur" ) == 0 )
			p->m_state = Developpeur;
		else if ( strcmp( el, "editeur" ) == 0 )
			p->m_state = Editeur;
		else if ( strcmp( el, "joueurs" ) == 0 )
			p->m_state = Joueurs;
		else if ( strcmp( el, "genres" ) == 0 )
			p->m_state = Genres;
		else if ( strcmp( el, "genre" ) == 0 )
		{
			p->m_state = Genre;
			p->m_region.clear();

			bool is_primary = false;
			for ( int i = 0; attr[i]; i += 2 )
			{
				if ( strcmp( attr[i], "langue" ) == 0 )
					p->m_region = attr[i+1];

				if ( strcmp( attr[i], "principale" ) == 0 && strcmp( attr[i+1], "1" ) == 0 )
					is_primary = true;
			}

			if ( is_primary )
				p->m_region += ":primary";
			else
				p->m_region += ":secondary";
		}
		else if ( strcmp( el, "synopsis" ) == 0 )
			p->m_state = Synopsis;
		else if ( strcmp( el, "familles" ) == 0 )
			p->m_state = Familles;
		else if ( strcmp( el, "roms" ) == 0 )
			p->m_state = Roms;
		else if ( strcmp( el, "rom" ) == 0 )
			p->m_state = Rom;
		else if ( strcmp( el, "romregions" ) == 0 )
			p->m_state = Romregions;

		p->m_current_data.clear();
	}

	static void end_element( void *data, const char *el )
	{
		SSXMLParser *p = (SSXMLParser *)data;

		if ( strcmp( el, "jeu" ) == 0 )
			p->m_current_game = NULL;
		else if ( strcmp( el, "nom" ) == 0 && p->m_current_game )
		{
			if ( p->m_current_game->name.empty() )
				p->m_current_game->name = p->m_current_data;
		}
		else if ( strcmp( el, "date" ) == 0 && p->m_current_game )
		{
			if ( p->m_current_game->year.empty() )
				p->m_current_game->year = p->m_current_data;
		}
		else if ( strcmp( el, "developpeur" ) == 0 && p->m_current_game )
		{
			if ( p->m_current_game->manufacturer.empty() )
				p->m_current_game->manufacturer = p->m_current_data;
		}
		else if ( strcmp( el, "editeur" ) == 0 && p->m_current_game )
		{
			if ( p->m_current_game->publisher.empty() )
				p->m_current_game->publisher = p->m_current_data;
		}
		else if ( strcmp( el, "joueurs" ) == 0 && p->m_current_game )
		{
			if ( p->m_current_game->players.empty() )
			{
				std::string temp = p->m_current_data;
				size_t pos = temp.find( "joueur" );
				if ( pos != std::string::npos )
					temp = temp.substr( 0, pos );

				while ( !temp.empty() && temp.back() == ' ' )
					temp.pop_back();
				p->m_current_game->players = temp;
			}
		}
		else if ( strcmp( el, "genre" ) == 0 && p->m_current_game )
		{
			std::string lang_code = "en";
			bool is_primary = false;

			size_t colon_pos = p->m_region.find( ':' );
			if ( colon_pos != std::string::npos )
			{
				lang_code = p->m_region.substr( 0, colon_pos );
				is_primary = ( p->m_region.substr( colon_pos + 1 ) == "primary" );
			}

			std::string genre_entry = lang_code + ":" + (is_primary ? "P" : "S") + ":" + p->m_current_data;
			p->m_current_game->genres.push_back( genre_entry );
		}
		else if ( strcmp( el, "synopsis" ) == 0 && p->m_current_game )
		{
			std::string desc = p->m_current_data;
			size_t pos = 0;
			while ( ( pos = desc.find( "&lt;", pos ) ) != std::string::npos )
			{
				desc.replace( pos, 4, "<" );
				pos += 1;
			}
			pos = 0;
			while ( ( pos = desc.find( "&gt;", pos ) ) != std::string::npos )
			{
				desc.replace( pos, 4, ">" );
				pos += 1;
			}
			pos = 0;
			while ( ( pos = desc.find( "&amp;", pos ) ) != std::string::npos )
			{
				desc.replace( pos, 5, "&" );
				pos += 1;
			}
			pos = 0;
			while ( ( pos = desc.find( "&quot;", pos ) ) != std::string::npos )
			{
				desc.replace( pos, 6, "\"" );
				pos += 1;
			}

			if ( p->m_current_game->description.empty() )
				p->m_current_game->description = desc;
		}
		else if ( strcmp( el, "famille" ) == 0 && p->m_current_game )
		{
			if ( p->m_current_game->series.empty() )
				p->m_current_game->series = p->m_current_data;
		}
		else if ( strcmp( el, "romregions" ) == 0 && p->m_current_game )
		{
			if ( p->m_current_game->region.empty() && !p->m_current_data.empty() )
				p->m_current_game->region = p->m_current_data;
		}
		else if ( strcmp( el, "media" ) == 0 && p->m_current_game )
		{
			if ( p->m_media_type == "ss" ) // snap
			{
				if ( p->m_current_game->snap.empty() )
				{
					p->m_current_game->snap = p->m_current_data;
					p->m_current_game->snap_crc = p->m_media_crc;
				}
			}
			else if ( p->m_media_type == "screenmarqueesmall" )
			{
				if ( p->m_current_game->marquee.empty() )
				{
					p->m_current_game->marquee = p->m_current_data;
					p->m_current_game->marquee_crc = p->m_media_crc;
				}
			}
			else if ( p->m_media_type == "box-2D" ) // flyer/boxart
			{
				if ( p->m_current_game->flyer.empty() )
				{
					p->m_current_game->flyer = p->m_current_data;
					p->m_current_game->flyer_crc = p->m_media_crc;
				}
			}
			else if ( p->m_media_type == "wheel" || p->m_media_type == "wheel-hd" )
			{
				if ( p->m_current_game->wheel.empty() )
				{
					p->m_current_game->wheel = p->m_current_data;
					p->m_current_game->wheel_crc = p->m_media_crc;
				}
			}
			else if ( p->m_media_type == "fanart" )
			{
				if ( p->m_current_game->fanart.empty() )
				{
					p->m_current_game->fanart = p->m_current_data;
					p->m_current_game->fanart_crc = p->m_media_crc;
				}
			}
			else if ( p->m_media_type == "video" )
			{
				if ( p->m_current_game->video.empty() )
				{
					p->m_current_game->video = p->m_current_data;
					p->m_current_game->video_crc = p->m_media_crc;
				}
			}
		}

		p->m_state = p->m_last_state;
		p->m_current_data.clear();
	}

	static void char_data( void *data, const char *s, int len )
	{
		SSXMLParser *p = (SSXMLParser *)data;
		p->m_current_data.append( s, len );
	}
};

std::string url_encode( const std::string &value )
{
	std::string result;
	for ( char c : value )
	{
		if ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
			( c >= '0' && c <= '9' ) || c == '-' || c == '_' || c == '.' || c == '~' )
		{
			result += c;
		}
		else
		{
			char hex[5];
			snprintf( hex, sizeof(hex), "%%%02X", (unsigned char)c );
			result += hex;
		}
	}
	return result;
}

int get_screenscraper_sysid( const std::string &emulator_name )
{
	std::string name = emulator_name;

	for ( auto &c : name )
		c = tolower( c );

	auto it = SS_SYSTEM_MAP.find( name );
	if ( it != SS_SYSTEM_MAP.end() )
		return it->second;

	FeLog() << " - Warning: No ScreenScraper system ID found for emulator: " << emulator_name << std::endl;
	return 75; // Default to generic arcade if unknown
}

std::string get_ss_game_search_url( const std::string &game_name,
	const std::string &username,
	const std::string &password,
	int systemeid )
{
	std::string url = SSHOSTNAME;
	url += "/jeuInfos.php?devid=" + SS_DEV_ID + "&devpassword=" + SS_DEV_PASSWORD;
	url += "&softname=attractplus";
	url += "&output=xml";
	url += "&systemeid=";
	url += std::to_string( systemeid );
	url += "&romnom=";
	url += url_encode( game_name );
	url += "&langue=en";  // Force English

	if ( !username.empty() && !password.empty() )
	{
		url += "&ssid=";
		url += url_encode( username );
		url += "&sspassword=";
		url += url_encode( password );
	}

	return url;
}

int get_ss_user_info( const std::string &username, const std::string &password )
{
	std::string url = SSHOSTNAME;
	url += "/ssuserInfos.php?devid=" + SS_DEV_ID + "&devpassword=" + SS_DEV_PASSWORD;
	url += "&softname=attractplus";
	url += "&output=xml";

	if ( !username.empty() && !password.empty() )
	{
		url += "&ssid=";
		url += url_encode( username );
		url += "&sspassword=";
		url += url_encode( password );
	}

	FeNetQueue q;
	q.add_buffer_task( url, 0 );
	FeNetWorker worker( q );

	int task_id = 0;
	std::string aux;
	bool completed = false;

	while ( !completed )
	{
		if ( q.pop_completed_task( task_id, aux ) )
			completed = true;
		else if ( q.all_done() )
		{
			FeLog() << " - Warning: User info task completed but no return" << std::endl;
			return 0;
		}
		else
			std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	}

	if ( aux.empty() )
	{
		FeLog() << " - Warning: Failed to get user info from ScreenScraper" << std::endl;
		return 0;
	}

	size_t pos = aux.find( "<requesters>" );
	if ( pos == std::string::npos )
		pos = aux.find( "<maxthreads>" );

	if ( pos == std::string::npos )
	{
		FeLog() << " - Warning: Workers count not found in user info response" << std::endl;
		return 0;
	}

	size_t start = pos + 12;
	size_t end = aux.find( '<', start );
	if ( end == std::string::npos )
		return 0;

	std::string workers_str = aux.substr( start, end - start );
	int workers = std::atoi( workers_str.c_str() );

	FeLog() << " - ScreenScraper workers count: " << workers << std::endl;
	return workers;
}

std::string calculate_file_crc32( const std::string &filename )
{
#ifdef USE_LIBARCHIVE
	nowide::ifstream file( filename, std::ios::binary );
	if ( !file.is_open() )
		return "";

	file.seekg( 0, std::ios::end );
	std::streamsize file_size = file.tellg();
	file.seekg( 0, std::ios::beg );

	if ( file_size == 0 )
		return "";

	std::vector<char> buffer( file_size );
	if ( !file.read( buffer.data(), file_size ) )
		return "";

	u_long crc = crc32( 0L, Z_NULL, 0 );
	crc = crc32( crc, (Bytef *)buffer.data(), file_size );

	// Format as uppercase hex
	char buf[9];
	snprintf( buf, sizeof(buf), "%08lX", (unsigned long)crc );
	return buf;
#else
	return "";
#endif
}

std::string format_genres( const std::vector<std::string> &genres )
{
	if ( genres.empty() )
		return "";

	std::vector<std::string> primary_en;
	std::vector<std::string> secondary_en;
	std::vector<std::string> primary_other;
	std::vector<std::string> secondary_other;

	for ( size_t i = 0; i < genres.size(); i++ )
	{
		const std::string &g = genres[i];

		size_t colon1 = g.find( ':' );
		if ( colon1 == std::string::npos )
			continue;

		size_t colon2 = g.find( ':', colon1 + 1 );
		if ( colon2 == std::string::npos )
			continue;

		std::string lang = g.substr( 0, colon1 );
		char type = g[colon1 + 1];
		std::string genre_name = g.substr( colon2 + 1 );

		if ( lang == "en" )
		{
			if ( type == 'P' )
				primary_en.push_back( genre_name );
			else
				secondary_en.push_back( genre_name );
		}
		else
		{
			if ( type == 'P' )
				primary_other.push_back( genre_name );
			else
				secondary_other.push_back( genre_name );
		}
	}

	std::string result;

	bool has_english = !primary_en.empty() || !secondary_en.empty();

	if ( has_english )
	{
		std::string primary_genre;
		if ( !primary_en.empty() )
			primary_genre = primary_en[0];

		std::string subcategory_genre;

		if ( !secondary_en.empty() )
			subcategory_genre = secondary_en[0];

		if ( !subcategory_genre.empty() )
			result = subcategory_genre;
		else if ( !primary_genre.empty() )
			result = primary_genre;
	}
	else
		return "";

	return result;
}

void download_media( FeNetQueue &q, const std::string &url,
	const std::string &romname, const std::string &art_type,
	const std::string &path_base, const std::string &server_crc = "" )
{
	if ( url.empty() )
		return;

	std::string filename = path_base;

	if ( filename.empty() || filename.back() != '/' )
		filename += "/";

	filename += art_type;
	filename += "/";
	confirm_directory( filename, "" );
	filename += romname;

	size_t ext_pos = url.find( "ext=" );
	if ( ext_pos != std::string::npos )
	{
		size_t ext_start = ext_pos + 4; // Skip "ext="
		size_t ext_end = url.find( "&", ext_start );
		if ( ext_end == std::string::npos )
			ext_end = url.size();
		filename += url.substr( ext_start, ext_end - ext_start );
	}
	else
	{
		// Fallback: determine from art_type if ext= not found
		if ( art_type == "video" )
			filename += ".mp4";
		else
			filename += ".png";
	}

	if ( file_exists( filename ) )
	{
		if ( !server_crc.empty() )
		{
			std::string local_crc = calculate_file_crc32( filename );

			if ( !local_crc.empty() && local_crc == uppercase( server_crc ) )
			{
				FeDebug() << " - Skipping download (CRC match): " << filename
					<< " (local: " << local_crc << ", server: " << uppercase( server_crc ) << ")" << std::endl;
				return;
			}
			else
			{
				FeDebug() << " - Redownloading (CRC mismatch): " << filename
					<< " (local: " << local_crc << ", server: " << uppercase( server_crc ) << ")" << std::endl;
			}
		}
		else
		{
			FeDebug() << " - Skipping download (file exists): " << filename << std::endl;
			return;
		}
	}

	FeDebug() << " - Downloading: " << filename << std::endl;
	q.add_file_task( url, filename );
}

} // namespace

bool FeSettings::screenscraper_scraper( FeImporterContext &c )
{
	std::string path = get_config_dir();
	confirm_directory( path, FE_SCRAPER_SUBDIR );
	path += FE_SCRAPER_SUBDIR;

	std::string username = get_info( FeSettings::ScreenScraperUser );
	std::string password = get_info( FeSettings::ScreenScraperPass );

	int ss_workers = 0;
	if ( !username.empty() && !password.empty() )
		ss_workers = get_ss_user_info( username, password );

	ParentMapType parent_map;
	build_parent_map( parent_map, c.romlist, c.emulator.is_mess() );

	std::string emu_name = c.emulator.get_info( FeEmulatorInfo::Name );

	int sysid = get_screenscraper_sysid( c.emulator.get_info( FeEmulatorInfo::System ) );
	FeDebug() << " - ScreenScraper systemeid: " << sysid << " for \""
		<< c.emulator.get_info( FeEmulatorInfo::System ) << "\"" << std::endl;

	confirm_directory( get_config_dir(), FE_SCRAPER_SUBDIR );
	std::string emu_path = get_config_dir();
	emu_path += FE_SCRAPER_SUBDIR;
	emu_path += emu_name;
	emu_path += "/";

	if ( m_scrape_marquees )
		confirm_directory( emu_path, "marquee" );
	if ( m_scrape_flyers )
		confirm_directory( emu_path, "flyer" );
	if ( m_scrape_wheels )
		confirm_directory( emu_path, "wheel" );
	if ( m_scrape_fanart )
		confirm_directory( emu_path, "fanart" );
	if ( m_scrape_vids )
		confirm_directory( emu_path, "video" );
	if ( m_scrape_overview )
		confirm_directory( emu_path, "overview" );

	int info_taskc = 0;
	int total_count = 0;

	FeNetQueue q;
	std::map<int, std::string> task_to_romname;

	for ( auto& rom : c.romlist )
	{
		rom.set_info( FeRomInfo::Emulator, emu_name );

		if ( c.scrape_art )
		{
			if ( has_same_name_as_parent( rom, parent_map ) )
			{
				FeDebug() << " - Skipping clone: " << rom.get_info( FeRomInfo::Romname ) << std::endl;
				continue;
			}
		}

		total_count++;

		std::string game_name = name_with_brackets_stripped( rom.get_info( FeRomInfo::Title ) );
		std::string url = get_ss_game_search_url( game_name, username, password, sysid );

		q.add_buffer_task( url, info_taskc );
		task_to_romname[info_taskc] = rom.get_info( FeRomInfo::Romname );
		info_taskc++;
	}

	if ( info_taskc == 0 )
	{
		FeLog() << " - No games to scrape." << std::endl;
		return true;
	}

	FeLog() << " - Querying " << info_taskc << " games from ScreenScraper" << std::endl;

	int num_workers = ( ss_workers > 0 && ss_workers <= 15 ) ? ss_workers : 1;
	FeLog() << " - Using " << num_workers << " worker threads for scraping" << std::endl;

	std::vector<std::unique_ptr<FeNetWorker>> workers;
	workers.reserve(num_workers);

	for (int i = 0; i < num_workers; i++)
		workers.emplace_back(std::make_unique<FeNetWorker>(q));

	int info_done = 0;
	const int initial_task_count = info_taskc;

	std::vector<SSGameData> games;
	int found( 0 );
	std::string aux;

	while ( !q.all_done() )
	{
		int id;
		std::string result;

		if ( q.pop_completed_task( id, result ) )
		{
			if ( id >= 0 )
			{
				info_done++;
				games.clear();
				SSXMLParser parser( games );

				if ( parser.parse( result ) && !games.empty() )
				{
					std::string romname = task_to_romname[id];
					std::string rom_title = name_with_brackets_stripped( romname );
					SSGameData *game = NULL;

					for ( auto& game_data : games )
					{
						std::string ss_title = name_with_brackets_stripped( game_data.name );
						if ( ss_title == rom_title )
						{
							game = &game_data;
							break;
						}
					}

					if ( game == NULL && !games.empty() )
						game = &games.front();

					if ( game != NULL && !game->name.empty() )
					{
						for ( auto& rom : c.romlist )
						{
							if ( rom.get_info( FeRomInfo::Romname ) == romname )
							{
								if ( !game->name.empty() )
									rom.set_info( FeRomInfo::Title, game->name );

								std::string formatted_genre = format_genres( game->genres );

								// Format: YYYY-MM-DD or just YYYY)
								if ( !game->year.empty() )
								{
									std::string year = game->year;
									if ( year.length() > 4 )
										year = year.substr( 0, 4 ); // Take just the year
									rom.set_info( FeRomInfo::Year, year );
								}

								if ( !game->manufacturer.empty() )
									rom.set_info( FeRomInfo::Manufacturer, game->manufacturer );

								if ( !game->publisher.empty() )
								{
									std::string manu = rom.get_info( FeRomInfo::Manufacturer );
									if ( !manu.empty() && manu != game->publisher )
										manu += " / " + game->publisher;
									else
										manu = game->publisher;
									rom.set_info( FeRomInfo::Manufacturer, manu );
								}

								if ( !formatted_genre.empty() )
									rom.set_info( FeRomInfo::Category, formatted_genre );

								if ( !game->players.empty() )
									rom.set_info( FeRomInfo::Players, game->players );

								if ( !game->series.empty() )
									rom.set_info( FeRomInfo::Series, game->series );

								if ( !game->language.empty() )
									rom.set_info( FeRomInfo::Language, game->language );

								if ( !game->region.empty() )
									rom.set_info( FeRomInfo::Region, game->region );

								break;
							}
						}

						if ( m_scrape_snaps && !game->snap.empty() )
						{
							download_media( q, game->snap, romname, "snap", emu_path, game->snap_crc );
							c.download_count++;
						}

						if ( m_scrape_marquees && !game->marquee.empty() )
						{
							download_media( q, game->marquee, romname, "marquee", emu_path, game->marquee_crc );
							c.download_count++;
						}

						if ( m_scrape_flyers && !game->flyer.empty() )
						{
							download_media( q, game->flyer, romname, "flyer", emu_path, game->flyer_crc );
							c.download_count++;
						}

						if ( m_scrape_wheels && !game->wheel.empty() )
						{
							download_media( q, game->wheel, romname, "wheel", emu_path, game->wheel_crc );
							c.download_count++;
						}

						if ( m_scrape_fanart && !game->fanart.empty() )
						{
							download_media( q, game->fanart, romname, "fanart", emu_path, game->fanart_crc );
							c.download_count++;
						}

						if ( m_scrape_vids && !game->video.empty() )
						{
							download_media( q, game->video, romname, "video", emu_path, game->video_crc );
						}

						if ( m_scrape_overview && !game->description.empty() )
						{
							set_game_overview( emu_name,
								romname,
								game->description,
								false );
							FeDebug() << " - Saved overview: " << romname << std::endl;
						}

						if ( game->flyer.empty() && game->wheel.empty() && game->fanart.empty() && game->video.empty() )
							FeDebug() << " - No media found for: " << romname << std::endl;
						else
							found++;

						aux = game->name;
					}
				}
				else
					FeDebug() << " - No game info found for query ID " << id << std::endl;
			}
			else if ( id == -1 )
			{
				if ( !result.empty() )
					FeLog() << " - Downloaded: " << result << std::endl;
				else
					FeLog() << " - Failed to download" << std::endl;
				c.download_count--;
			}

			if ( c.uiupdate && ( initial_task_count > 0 ) )
			{
				int p = c.progress_past + info_done * c.progress_range / initial_task_count;
				if ( c.uiupdate( c.uiupdatedata, p, aux ) == false )
					return false;
			}
		}
		else
			std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	}

	FeLog() << " - Scrape complete. Found " << found << " games." << std::endl;
	return true;
}

#else

bool FeSettings::screenscraper_scraper( FeImporterContext &c )
{
	FeLog() << "Error: ScreenScraper scraper requires libcurl "
		<< "Please rebuild with USE_LIBCURL" << std::endl;
	return false;
}

#endif
