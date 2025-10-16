/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2015 Andrew Mickelson
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

#include "fe_net.hpp"
#include "fe_base.hpp"
#include "nowide/fstream.hpp"
#include "rapidjson/document.h"
#include <iostream>
#include <cstring>
#include <chrono>

#include <curl/curl.h>

static size_t write_curl_callback( void *contents, size_t size, size_t nmemb, void *userp )
{
	size_t add_size = size * nmemb;

	std::vector<char> *mem = (std::vector<char> *)userp;
	size_t old_size = mem->size();

	mem->resize( old_size + add_size );
	memcpy( &((*mem)[old_size]), contents, add_size );

	return add_size;
}

FeNetTask::FeNetTask( const std::string &url,
		const std::string &filename,
		TaskType t )
	: m_type( t ),
	m_url( url ),
	m_filename( filename ),
	m_id( FileTaskError )
{
}

FeNetTask::FeNetTask( const std::string &url,
		int id )
	: m_type( BufferTask ),
	m_url( url ),
	m_id( id )
{
}

FeNetTask::FeNetTask()
	: m_type( NoTask ),
	m_id( 0 )
{
}

FeNetTask::FeNetTask( const FeNetTask &o )
{
	*this = o;
}

const FeNetTask &FeNetTask::operator=( const FeNetTask &o )
{
	m_type = o.m_type;
	m_url = o.m_url;
	m_filename = o.m_filename;
	m_result = o.m_result;
	m_id = o.m_id;

	return *this;
}

bool FeNetTask::do_task( long *code )
{
	const char *UA_VALUE = "Attract-Mode-Plus/3.x";

	std::vector<char> rbuff;

	CURL *curl_handle;
	CURLcode res;

	// set up our http request
	curl_handle = curl_easy_init();

	curl_easy_setopt( curl_handle, CURLOPT_WRITEFUNCTION, write_curl_callback );
	curl_easy_setopt( curl_handle, CURLOPT_WRITEDATA, (void *)&rbuff );
	curl_easy_setopt( curl_handle, CURLOPT_USERAGENT, UA_VALUE );

	// set to abort if slower than 30 bytes/sec during 60 seconds
	curl_easy_setopt( curl_handle, CURLOPT_LOW_SPEED_TIME, 60L );
	curl_easy_setopt( curl_handle, CURLOPT_LOW_SPEED_LIMIT, 30L );

	// set to complete connection within 10 seconds
	curl_easy_setopt( curl_handle, CURLOPT_CONNECTTIMEOUT, 10L );

	// follow redirection
	curl_easy_setopt( curl_handle, CURLOPT_FOLLOWLOCATION, 1L );

	// fail on 404 file not found messages
	curl_easy_setopt( curl_handle, CURLOPT_FAILONERROR, 1L );

	curl_easy_setopt( curl_handle, CURLOPT_URL, m_url.c_str() );

	res = curl_easy_perform( curl_handle );

	if ( res != CURLE_OK )
	{
		if ( res == CURLE_HTTP_RETURNED_ERROR )
		{
			long rcode;
			curl_easy_getinfo( curl_handle, CURLINFO_RESPONSE_CODE, &rcode );

			FeDebug() << " * Http error: " << rcode << " (" << m_url << ")" << std::endl;

			if ( code )
				*code = rcode;
		}
		else
		{
			m_result = curl_easy_strerror( res );

			if ( res == CURLE_COULDNT_RESOLVE_HOST ||
				 res == CURLE_COULDNT_CONNECT ||
				 res == CURLE_OPERATION_TIMEDOUT ||
				 res == CURLE_COULDNT_RESOLVE_PROXY )
			{
				FeDebug() << " - Network error: " << m_result
						<< " (" << m_url << ")" << std::endl;
			}
			else
			{
				FeLog() << " ! Error processing request: " << m_result
						<< " (" << m_url << ")" << std::endl;
			}
		}

		curl_easy_cleanup( curl_handle );
		return false;
	}

	curl_easy_cleanup( curl_handle );

	if (( m_type == FileTask ) || ( m_type == SpecialFileTask ))
	{
		nowide::ofstream outfile( m_filename.c_str(), std::ios_base::binary );
		if ( !outfile.is_open() )
		{
			FeLog() << " ! Unable to open file for writing: "
				<< m_filename << std::endl;
			return false;
		}

		outfile.write( &(rbuff[0]), rbuff.size() );
		outfile.close();

		m_id=m_type;
		m_result = m_filename;
	}
	else
	{
		m_result.clear();
		m_result.append( &(rbuff[0]), rbuff.size() );
	}

	return true;
}

void FeNetTask::grab_result( int &id, std::string &result )
{
	id = m_id;
	result.swap( m_result );
}

FeNetQueue::FeNetQueue()
	: m_in_flight( 0 )
{
}

void FeNetQueue::add_file_task( const std::string &url,
		const std::string &file_name,
		bool flag_special )
{
	std::lock_guard<std::recursive_mutex> l( m_mutex );
	m_in_queue.push_front( FeNetTask( url, file_name,
		flag_special ? FeNetTask::SpecialFileTask : FeNetTask::FileTask ) );
}

void FeNetQueue::add_buffer_task( const std::string &url,
		int id )
{
	std::lock_guard<std::recursive_mutex> l( m_mutex );
	m_in_queue.push_back( FeNetTask( url, id ) );
}

bool FeNetQueue::get_next_task( FeNetTask &t )
{
	// Grab next task from the input queue
	//
	std::lock_guard<std::recursive_mutex> l( m_mutex );

	if ( m_in_queue.empty() )
		return false;

	t = m_in_queue.front();
	m_in_queue.pop_front();

	m_in_flight++;
	return true;
}

void FeNetQueue::done_with_task( const FeNetTask &t, bool res )
{
	// Queue result
	//
	std::lock_guard<std::recursive_mutex> l( m_mutex );

	if ( res )
		m_out_queue.push( t );

	m_in_flight--;

	FeDebug() << "WORKERS: queue_in=" << m_in_queue.size() << ", in_progress=" << m_in_flight
		<< ", queue_out=" << m_out_queue.size() << std::endl;
}

bool FeNetQueue::pop_completed_task( int &id,
		std::string &result )
{
	std::lock_guard<std::recursive_mutex> l( m_mutex );
	if ( !m_out_queue.empty() )
	{
		m_out_queue.front().grab_result( id, result );
		m_out_queue.pop();
		return true;
	}
	return false;
}

void FeNetQueue::abort()
{
	std::lock_guard<std::recursive_mutex> l( m_mutex );

	while ( !m_in_queue.empty() )
		m_in_queue.pop_front();
}

bool FeNetQueue::all_done()
{
	std::lock_guard<std::recursive_mutex> l( m_mutex );

	bool retval = ( m_in_queue.empty() && m_out_queue.empty() && ( m_in_flight == 0 ) );
	return retval;
}

bool FeNetQueue::output_done()
{
	std::lock_guard<std::recursive_mutex> l( m_mutex );

	bool retval = ( m_out_queue.empty() && ( m_in_flight == 0 ) );
	return retval;
}

FeNetWorker::FeNetWorker( FeNetQueue &queue )
	: m_queue( queue ),
	m_thread( &FeNetWorker::work_process, this ),
	m_proceed( true )
{
}

FeNetWorker::~FeNetWorker()
{
	m_proceed = false;

	if ( m_thread.joinable() )
		m_thread.join();
}

void FeNetWorker::work_process()
{
	while ( m_proceed && !m_queue.all_done() )
	{
		bool res=false;
		FeNetTask t;

		if ( m_queue.get_next_task( t ) )
		{
			// Perform task
			//
			long code( 0 );
			res = t.do_task( &code );

			m_queue.done_with_task( t, res );

			if ( !res )
			{
				if ( code == 500 )
				{
					FeLog() << "Aborting scrape of server, encountered http error code: " << code << std::endl;
					m_queue.abort();
				}
			}
		}

		if ( !res ) // sleep if there is nothing in the queue
			std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
	}

	FeDebug() << "WORKER thread process completed." << std::endl;
}

FeVersionChecker::FeVersionChecker()
	: m_initiated( false )
{
}

FeVersionChecker::~FeVersionChecker()
{
	m_worker.reset();
	m_queue.reset();
}

bool FeVersionChecker::initiate()
{
	if ( m_initiated )
		return true;

	try
	{
		const std::string VERSION_CHECK_URL = "https://api.github.com/repos/oomek/attractplus/releases/latest";

		m_queue = std::make_unique<FeNetQueue>();
		m_worker = std::make_unique<FeNetWorker>( *m_queue );

		m_queue->add_buffer_task( VERSION_CHECK_URL, VERSION_CHECK_ID );

		m_initiated = true;
		FeDebug() << "Version check: initiated" << std::endl;
		return true;
	}
	catch ( ... )
	{
		FeDebug() << "Version check: failed to initiate" << std::endl;
		return false;
	}
}

bool FeVersionChecker::check_result()
{
	if ( !m_queue )
		return false;

	int result_id;
	std::string response;
	bool new_version_available = false;

	if ( m_queue->pop_completed_task( result_id, response ) )
	{		if ( result_id == VERSION_CHECK_ID )
		{
			if ( !response.empty() )
			{
				rapidjson::Document doc;
				doc.Parse( response.c_str() );

				if ( !doc.HasParseError() && doc.IsObject() && doc.HasMember( "tag_name" ) && doc["tag_name"].IsString() )
				{
					std::string remote_version = doc["tag_name"].GetString();
					m_remote_version = remote_version;
					m_current_version = FE_VERSION;

					if ( !m_current_version.empty() && m_current_version[0] == 'v' )
						m_current_version = m_current_version.substr( 1 );

					if ( !remote_version.empty() && remote_version[0] == 'v' )
						remote_version = remote_version.substr( 1 );

					std::string version_concat;
					for ( char c : remote_version )
					{
						if ( c != '.' )
							version_concat += c;
					}

					int remote_version_num = std::atoi( version_concat.c_str() );
					if ( remote_version_num > FE_VERSION_NUM )
					{
						FeLog() << "New AM+ version is available: " << m_remote_version
							   << ", current version: " << m_current_version << std::endl;
						new_version_available = true;
					}
					else
						FeDebug() << "Version Check: AM+ is up to date: " << m_current_version << std::endl;
				}
				else
					FeDebug() << "Version check: Unable to parse version information from server response" << std::endl;
			}
			else
				FeDebug() << "Version check: Unable to connect to server (network unavailable)" << std::endl;
		}

		return new_version_available;
	}

	return false;
}
