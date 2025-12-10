/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2013-2016 Andrew Mickelson
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

#ifndef FE_BASE_HPP
#define FE_BASE_HPP

#include <string>
#include <ostream>
#include <cstdint>

extern const char *FE_NAME;
extern const char *FE_COPYRIGHT;
extern const char *FE_VERSION;

extern const char *FE_WHITESPACE;
extern const char *FE_DIR_TOKEN;
extern const float FE_SCORE_MAX;

extern const char *FE_TAG_ICON;
extern const char *FE_HEART_ICON;
extern const char *FE_HEART_OUTLINE_ICON;
extern const char *FE_STAR_ICON;
extern const char *FE_STAR_OUTLINE_ICON;
extern const char *FE_STAR_HALF_ICON;
extern const char *FE_YES_ICON;
extern const char *FE_NO_ICON;

extern const char *FE_TAG_PREFIX;
extern const char *FE_TAG_DELIM;

extern const char *FE_DEFAULT_LANGUAGE;
extern const char *FE_DEFAULT_ARTWORK;
extern const char *FE_EMULATOR_SUBDIR;
extern const char *FE_TEMPLATE_SUBDIR;
extern const char *FE_TEMPLATE_EMULATOR_SUBDIR;
extern const char *FE_EMULATOR_TEMPLATES_SUBDIR; // Deprecated, left for migration cleanup
extern const char *FE_EMULATOR_FILE_EXTENSION;
extern const char *FE_EMULATOR_DEFAULT;

extern const std::uint32_t FE_CACHE_VERSION;

enum FeLogLevel
{
	FeLog_Silent,
	FeLog_Info,
	FeLog_Debug
};

std::ostream &FeLog();
std::ostream &FeDebug();
void fe_set_log_file( const std::string & );
void fe_set_log_level( enum FeLogLevel );
void fe_print_version();
const char *fe_get_log_level_string();

class FeBaseConfigurable
{
protected:
	void invalid_setting(
		const std::string &filename,
		const char *base,
		const std::string &setting,
		const char **valid1,
		const char **valid2=NULL,
		const char *label="setting" );

public:
	virtual int process_setting(
		const std::string &setting,
		const std::string &value,
		const std::string &filename
	) = 0;

	bool load_from_file(
		const std::string &filename,
		const char *sep=FE_WHITESPACE
	);

	bool load_from_string(
		const std::string &content,
		const char *sep=FE_WHITESPACE
	);

	bool load_from_binary(
		const char *content,
		const char *sep=FE_WHITESPACE
	);
};

#endif
