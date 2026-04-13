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

#include "fe_glslang.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <cstring>

namespace
{
	struct FeGlslangProcessGuard
	{
		FeGlslangProcessGuard()
			: initialized( glslang::InitializeProcess() )
		{
		}

		~FeGlslangProcessGuard()
		{
			if ( initialized )
				glslang::FinalizeProcess();
		}

		bool initialized;
	};

	FeGlslangProcessGuard &get_glslang_process_guard()
	{
		static FeGlslangProcessGuard guard;
		return guard;
	}


	void append_diagnostics( std::string &diagnostics, const char *text )
	{
		if ( !text || !text[ 0 ] )
			return;

		if ( !diagnostics.empty() )
			diagnostics += '\n';
		diagnostics += text;
	}

	void trim_trailing_newlines( std::string &text )
	{
		while ( !text.empty() && (( text.back() == '\n' ) || ( text.back() == '\r' )) )
			text.pop_back();
	}
}

bool fe_glslang_compile_to_spirv(
	const std::string &source_name,
	const std::string &source_code,
	bool vertex_stage,
	std::vector<std::uint8_t> &spirv_code,
	std::string &diagnostics )
{
	spirv_code.clear();
	diagnostics.clear();

	const FeGlslangProcessGuard &process_guard = get_glslang_process_guard();
	if ( !process_guard.initialized )
	{
		diagnostics = "glslang initialization failed";
		return false;
	}

	const EShLanguage stage = vertex_stage ? EShLangVertex : EShLangFragment;
	const char *source_text = source_code.c_str();
	const int source_length = static_cast<int>( source_code.size() );
	const char *source_name_text = source_name.c_str();
	const EShMessages messages = static_cast<EShMessages>( EShMsgSpvRules | EShMsgVulkanRules );

	glslang::TShader shader( stage );
	shader.setStringsWithLengthsAndNames( &source_text, &source_length, &source_name_text, 1 );
	shader.setEnvInput( glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100 );
	shader.setEnvClient( glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0 );
	shader.setEnvTarget( glslang::EShTargetSpv, glslang::EShTargetSpv_1_0 );

	if ( !shader.parse( GetDefaultResources(), 100, false, messages ) )
	{
		append_diagnostics( diagnostics, shader.getInfoLog() );
		append_diagnostics( diagnostics, shader.getInfoDebugLog() );
		trim_trailing_newlines( diagnostics );
		return false;
	}

	glslang::TProgram program;
	program.addShader( &shader );
	if ( !program.link( messages ) )
	{
		append_diagnostics( diagnostics, shader.getInfoLog() );
		append_diagnostics( diagnostics, shader.getInfoDebugLog() );
		append_diagnostics( diagnostics, program.getInfoLog() );
		append_diagnostics( diagnostics, program.getInfoDebugLog() );
		trim_trailing_newlines( diagnostics );
		return false;
	}

	const glslang::TIntermediate *intermediate = program.getIntermediate( stage );
	if ( !intermediate )
	{
		diagnostics = "glslang returned no intermediate";
		return false;
	}

	std::vector<unsigned int> spirv_words;
	spv::SpvBuildLogger logger;
	glslang::GlslangToSpv( *intermediate, spirv_words, &logger );
	append_diagnostics( diagnostics, logger.getAllMessages().c_str() );
	trim_trailing_newlines( diagnostics );

	if ( spirv_words.empty() )
	{
		if ( diagnostics.empty() )
			diagnostics = "glslang produced no SPIR-V output";
		return false;
	}

	spirv_code.resize( spirv_words.size() * sizeof( unsigned int ) );
	std::memcpy( spirv_code.data(), spirv_words.data(), spirv_code.size() );
	return true;
}