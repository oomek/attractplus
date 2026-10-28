/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2020 Andrew Mickelson
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

#include <list>
#include <map>
#include <queue>
#include <deque>
#include <iterator>
#include <string>
#include <mutex>
#include <new>
#include <thread>
#include <vector>
#include <condition_variable>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <chrono>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_mutex.h>

#include "fe_base.hpp" // logging
#include "fe_file.hpp"
#include "fe_util.hpp"

#include "media.hpp"

#include "image_loader.hpp"
#include "fe_present.hpp"
#include "fe_resource_cache.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace
{
	// stb_image callbacks that operate on a FeInputStream
	int read( void* user, char* data, int size )
	{
		FeInputStream* stream = static_cast<FeInputStream*>( user );
		auto result = stream->read( data, size );
		return result.value_or(0);
	}

	void skip( void* user, int size )
	{
		FeInputStream* stream = static_cast<FeInputStream*>( user );
		auto position = stream->tell().value_or(0);
		stream->seek( position + static_cast<std::size_t>( size ));
	}

	int eof( void* user )
	{
		FeInputStream* stream = static_cast<FeInputStream*>( user );
		auto pos = stream->tell().value_or(0);
		auto size = stream->getSize().value_or(0);
		return ( pos >= size ) ? 1 : 0;
	}
	std::recursive_mutex g_mutex;
	void mask_pixels( unsigned char *pixels, std::size_t count )
	{
		unsigned char color[4];
		std::memcpy( color, pixels, 4 );
		for ( std::size_t i = 0; i < count; ++i )
			if ( std::memcmp( pixels + i * 4, color, 4 ) == 0 ) pixels[i * 4 + 3] = 0;
	}

#ifdef FE_DEBUG
	std::atomic<int> g_entry_count( 0 );

	class EntryCountReporter
	{
	public:
		~EntryCountReporter()
		{
			FeLog() << "Image Loader Entry Count on exit: " << g_entry_count << std::endl;
		}
	};

	EntryCountReporter g_e_rep;
#endif
}

class FeImageMemory
{
public:
	explicit FeImageMemory( unsigned int limit ) : m_limit( limit ) {}
	bool acquire( bool &counted )
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		counted = m_gpu && !m_stopping;
		if ( counted && m_used == m_limit ) return false;
		if ( counted ) ++m_used;
		return true;
	}
	void release()
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		--m_used;
		m_ready.notify_all();
	}
	void wait()
	{
		std::unique_lock<std::mutex> lock( m_mutex );
		m_ready.wait( lock, [this]{ return !m_gpu || m_stopping || m_used < m_limit; } );
	}
	void set_gpu( bool enabled )
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		m_gpu = enabled;
		m_ready.notify_all();
	}
	void stop()
	{
		std::lock_guard<std::mutex> lock( m_mutex );
		m_stopping = true;
		m_ready.notify_all();
	}
private:
	std::mutex m_mutex;
	std::condition_variable m_ready;
	unsigned int m_limit, m_used = 0;
	bool m_gpu = false, m_stopping = false;
};

struct FeImagePixelsDeleter
{
	std::shared_ptr<FeImageMemory> memory;
	void operator()( unsigned char *pixels ) const
	{
		stbi_image_free( pixels );
		if ( memory ) memory->release();
	}
};

std::size_t FeImageLoaderEntry::trim_textures()
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	// Registry and variant map own two references. Draws and containers pin the rest.
	for ( auto it = m_textures.begin(); it != m_textures.end(); )
	{
		if ( it->second.use_count() == 2 && it->second->status != FeImageTexture::Pending )
			it = m_textures.erase( it );
		else ++it;
	}
	return gpu_bytes();
}



class FeImageUploadThread
{
public:
	FeImageUploadThread( SDL_GPUDevice *device, unsigned long long generation )
		: m_device( device ), m_generation( generation ), m_thread( &FeImageUploadThread::run, this ) {}
	~FeImageUploadThread() { stop(); }
	bool running() const { return m_run.load(); }

	void stop()
	{
		m_run = false;
		m_ready.notify_all();
		if ( m_thread.joinable() ) m_thread.join();
	}
	void cancel()
	{
		++m_epoch;
		m_ready.notify_all();
	}
	void loaded( const std::shared_ptr<FeImageLoaderEntry> &image )
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		for ( auto &job : m_jobs )
			if ( job.image.lock() == image )
			{
				job.image_pin = image;
				if ( !job.source && !job.pixels ) job.pixels = image->pixels();
			}
		m_ready.notify_one();
	}

	std::shared_ptr<FeImageTexture> request( const std::shared_ptr<FeImageLoaderEntry> &image, bool mipmap, bool cube_map )
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		if ( !m_run || image->m_canceled ) return {};
		auto &texture = image->m_textures[{ mipmap, cube_map }];
		if ( !texture || texture->generation != m_generation )
		{
			texture = make_texture( image->get_version() );
			texture->prefetch = image->m_prefetch;
			Job job;
			job.image = image;
			job.texture = texture;
			job.mipmap = mipmap;
			job.cube_map = cube_map;
			job.epoch = m_epoch;
			for ( const auto &item : image->m_textures )
			{
				const auto &source = item.second;
				if ( source != texture && source->status.load( std::memory_order_acquire ) == FeImageTexture::Ready &&
					source->generation == m_generation && source->info.type == SDL_GPU_TEXTURETYPE_2D &&
					source->info.width == static_cast<unsigned int>( image->get_width() ) &&
					source->info.height == static_cast<unsigned int>( image->get_height() ) )
				{
					job.source = source;
					texture->from_texture = true;
					break;
				}
			}
			if ( image->is_loaded() )
			{
				job.image_pin = image;
				if ( !job.source ) job.pixels = image->pixels();
			}
			if ( image->m_prefetch ) m_jobs.push_back( std::move( job ) );
			else m_jobs.push_front( std::move( job ) );
		}
		else if ( texture->prefetch && !image->m_prefetch )
		{
			texture->prefetch = false;
			for ( auto it = m_jobs.begin(); it != m_jobs.end(); ++it )
				if ( it->texture == texture )
				{
					std::rotate( m_jobs.begin(), it, std::next( it ) );
					break;
				}
		}
		m_ready.notify_one();
		return texture;
	}

	std::shared_ptr<FeImageTexture> video( std::shared_ptr<const unsigned char> pixels,
		unsigned int width, unsigned int height, bool mipmap, bool cube_map, unsigned long long version )
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		if ( !m_run || !pixels ) return {};
		auto texture = make_texture( version );
		Job job;
		job.texture = texture;
		job.pixels = std::move( pixels );
		job.width = width;
		job.height = height;
		job.mipmap = mipmap;
		job.cube_map = cube_map;
		job.video = true;
		job.epoch = m_epoch;
		m_jobs.push_front( std::move( job ) );
		m_ready.notify_one();
		return texture;
	}

private:
	static constexpr Uint32 buffer_size = 32 * 1024 * 1024;
	static constexpr Uint32 chunk_size = 2 * 1024 * 1024;
	struct Buffer
	{
		SDL_GPUTransferBuffer *buffer = nullptr;
		SDL_GPUFence *fence = nullptr;
	};
	struct Retired
	{
		SDL_GPUFence *fence;
		std::vector<std::shared_ptr<FeImageTexture>> textures;
	};
	struct Region
	{
		SDL_GPUTextureTransferInfo source = {};
		SDL_GPUTextureRegion target = {};
		SDL_GPUTextureLocation from = {};
	};
	struct Job
	{
		std::weak_ptr<FeImageLoaderEntry> image;
		std::shared_ptr<FeImageLoaderEntry> image_pin;
		std::shared_ptr<FeImageTexture> texture;
		std::shared_ptr<FeImageTexture> source;
		std::shared_ptr<const unsigned char> pixels;
		unsigned int width = 0, height = 0, row = 0, layer = 0;
		bool mipmap = false, cube_map = false, video = false;
		unsigned long long epoch = 0;
	};
	std::shared_ptr<FeImageTexture> make_texture( unsigned long long version )
	{
		auto texture = std::make_shared<FeImageTexture>();
		texture->generation = m_generation;
		texture->content_version = version;
		m_textures.push_back( texture );
		return texture;
	}

	void release( FeImageTexture &texture )
	{
		if ( texture.texture ) SDL_ReleaseGPUTexture( m_device, texture.texture );
		if ( texture.stream_buffer ) SDL_ReleaseGPUTransferBuffer( m_device, texture.stream_buffer );
		texture.texture = nullptr;
		texture.stream_buffer = nullptr;
	}

	void retire( bool discard=false )
	{
		for ( auto it = m_waiting.begin(); it != m_waiting.end(); )
		{
			if ( !discard && !SDL_QueryGPUFence( m_device, it->fence ) ) { ++it; continue; }
			SDL_ReleaseGPUFence( m_device, it->fence );
			for ( auto &texture : it->textures ) m_spares.push_back( std::move( texture ) );
			it = m_waiting.erase( it );
		}
		if ( discard )
		{
			for ( auto &texture : m_spares ) release( *texture );
			m_spares.clear();
			m_spare_bytes = 0;
		}
		Retired batch = {};
		for ( auto &texture : m_retired )
		{
			const auto bytes = texture->bytes + texture->stream_size;
			if ( !discard && texture->stream_buffer && texture->status == FeImageTexture::Ready &&
				m_spare_bytes + bytes <= 128 * 1024 * 1024 )
			{
				m_spare_bytes += bytes;
				batch.textures.push_back( std::move( texture ) );
			}
			else release( *texture );
		}
		m_retired.clear();
		if ( batch.textures.empty() ) return;
		// The last draw has already submitted before dropping its resource pin.
		// Reuse these video allocations only after that work has completed.
		auto *command = SDL_AcquireGPUCommandBuffer( m_device );
		if ( command ) batch.fence = SDL_SubmitGPUCommandBufferAndAcquireFence( command );
		if ( batch.fence ) m_waiting.push_back( std::move( batch ) );
		else for ( auto &texture : batch.textures )
		{
			m_spare_bytes -= texture->bytes + texture->stream_size;
			release( *texture );
		}
	}

	SDL_GPUTransferBuffer *create_buffer( Uint32 size )
	{
		SDL_GPUTransferBufferCreateInfo info = {};
		info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		info.size = size;
		auto *buffer = SDL_CreateGPUTransferBuffer( m_device, &info );
		if ( !buffer )
		{
			FeDebug() << "SDL: retrying artwork transfer allocation: " << size << " bytes" << std::endl;
			FeImageLoader::get_ref().prune_cache( true );
			collect();
			retire( true );
			buffer = SDL_CreateGPUTransferBuffer( m_device, &info );
		}
		return buffer;
	}

	void collect()
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		FeImageLoader::get_ref().prune_cache();
		for ( auto it = m_textures.begin(); it != m_textures.end(); )
		{
			if ( it->use_count() == 1 )
			{
				// Only this worker creates and retires artwork GPU allocations.
				auto texture = std::move( *it );
				it = m_textures.erase( it );
				// Release outside the registry lock below.
				m_retired.push_back( std::move( texture ) );
			}
			else ++it;
		}
	}

	bool allocate( Job &job )
	{
		auto &texture = *job.texture;
		auto &info = texture.info;
		unsigned int width = job.width, height = job.height;
		if ( !width || !height ) return false;
		if ( job.cube_map )
		{
			if ( width % 4 || height % 3 || width / 4 != height / 3 ) return false;
			width /= 4;
			height /= 3;
		}
		info.type = job.cube_map ? SDL_GPU_TEXTURETYPE_CUBE : SDL_GPU_TEXTURETYPE_2D;
		info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
		if ( job.mipmap ) info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
		info.width = width;
		info.height = height;
		info.layer_count_or_depth = job.cube_map ? 6 : 1;
		info.num_levels = 1;
		texture.bytes = 0;
		for ( unsigned int w = width, h = height; ; w = std::max( 1u, w / 2 ), h = std::max( 1u, h / 2 ) )
		{
			texture.bytes += static_cast<std::size_t>( w ) * h * 4 * info.layer_count_or_depth;
			if ( !job.mipmap || ( w == 1 && h == 1 ) ) break;
			++info.num_levels;
		}
		info.sample_count = SDL_GPU_SAMPLECOUNT_1;
		if ( job.video )
			for ( auto it = m_spares.begin(); it != m_spares.end(); ++it )
			{
				const auto &old = (*it)->info;
				if ( !(*it)->texture || old.width != info.width || old.height != info.height || old.type != info.type ||
					old.num_levels != info.num_levels || old.usage != info.usage ) continue;
				auto &spare = *it;
				m_spare_bytes -= spare->bytes;
				std::swap( texture.texture, spare->texture );
				spare->bytes = 0;
				if ( !spare->stream_buffer ) m_spares.erase( it );
				break;
			}
		if ( !texture.texture ) texture.texture = SDL_CreateGPUTexture( m_device, &info );
		if ( !texture.texture )
		{
			FeDebug() << "SDL: retrying artwork texture allocation: " << width << "x" << height << std::endl;
			FeImageLoader::get_ref().prune_cache( true );
			collect();
			retire( true );
			texture.texture = SDL_CreateGPUTexture( m_device, &info );
		}
		if ( !texture.texture ) return false;
		if ( job.video && !texture.stream_buffer )
		{
			const auto bytes = static_cast<std::uint64_t>( job.width ) * job.height * 4;
			if ( bytes > std::numeric_limits<Uint32>::max() ) return false;
			auto best = m_spares.end();
			for ( auto it = m_spares.begin(); it != m_spares.end(); ++it )
				if ( (*it)->stream_buffer && (*it)->stream_size >= bytes &&
					( best == m_spares.end() || (*it)->stream_size < (*best)->stream_size ) ) best = it;
			if ( best != m_spares.end() )
			{
				auto &spare = *best;
				std::swap( texture.stream_buffer, spare->stream_buffer );
				std::swap( texture.stream_size, spare->stream_size );
				m_spare_bytes -= texture.stream_size;
				if ( !spare->texture ) m_spares.erase( best );
			}
			else
			{
				texture.stream_buffer = create_buffer( static_cast<Uint32>( bytes ) );
				texture.stream_size = static_cast<Uint32>( bytes );
			}
			if ( !texture.stream_buffer ) return false;
		}
		return true;
	}

	void stage( Job &job, Buffer &buffer, void *mapped, Uint32 &offset, std::vector<Region> &regions )
	{
		auto &texture = *job.texture;
		const auto &info = texture.info;
		const std::uint64_t pitch = ( static_cast<std::uint64_t>( info.width ) * 4 + 255 ) & ~std::uint64_t( 255 );
		static const unsigned int columns[6] = { 2, 0, 1, 1, 1, 3 };
		static const unsigned int lines[6] = { 1, 1, 0, 2, 1, 1 };
		while ( job.layer < info.layer_count_or_depth && offset + pitch <= buffer_size )
		{
			const Uint32 rows = std::min<Uint32>( info.height - job.row, std::min<Uint32>( chunk_size, buffer_size - offset ) / pitch );
			const unsigned int x = job.cube_map ? columns[job.layer] * info.width : 0;
			const unsigned int y = job.cube_map ? lines[job.layer] * info.height : 0;
			Region region;
			region.source.transfer_buffer = buffer.buffer;
			region.source.offset = offset;
			region.source.pixels_per_row = static_cast<Uint32>( pitch / 4 );
			region.source.rows_per_layer = rows;
			region.target.texture = texture.texture;
			region.target.layer = job.layer;
			region.target.y = job.row;
			region.target.w = info.width;
			region.target.h = rows;
			region.target.d = 1;
			if ( job.source )
			{
				region.from.texture = job.source->texture;
				region.from.x = x;
				region.from.y = y + job.row;
			}
			else for ( Uint32 row = 0; row < rows; ++row )
				std::memcpy( static_cast<unsigned char *>( mapped ) + offset + row * pitch,
					job.pixels.get() + ( static_cast<std::size_t>( y + job.row + row ) * job.width + x ) * 4,
					static_cast<std::size_t>( info.width ) * 4 );
			regions.push_back( region );
			offset += static_cast<Uint32>( pitch * rows );
			// Keep each region's offset aligned for the next image's row pitch.
			offset = ( offset + 511 ) & ~Uint32( 511 );
			job.row += rows;
			if ( job.row == info.height ) { job.row = 0; ++job.layer; }
		}
	}

	bool submit( const std::vector<Job> &jobs, const std::vector<Region> &regions, Buffer &buffer )
	{
		auto *command = SDL_AcquireGPUCommandBuffer( m_device );
		if ( !command ) return false;
		auto *copy = SDL_BeginGPUCopyPass( command );
		if ( !copy )
		{
			SDL_CancelGPUCommandBuffer( command );
			return false;
		}
		for ( const auto &region : regions )
		{
			if ( region.from.texture )
			{
				SDL_GPUTextureLocation to = {};
				to.texture = region.target.texture;
				to.layer = region.target.layer;
				to.y = region.target.y;
				SDL_CopyGPUTextureToTexture( copy, &region.from, &to, region.target.w, region.target.h, 1, false );
			}
			else SDL_UploadToGPUTexture( copy, &region.source, &region.target, false );
		}
		SDL_EndGPUCopyPass( copy );
		for ( const auto &job : jobs )
			if ( job.layer == job.texture->info.layer_count_or_depth && job.texture->info.num_levels > 1 )
				SDL_GenerateMipmapsForGPUTexture( command, job.texture->texture );
		buffer.fence = SDL_SubmitGPUCommandBufferAndAcquireFence( command );
		return buffer.fence != nullptr;
	}

	void release_pixels( Job &job )
	{
		auto uploaded = std::move( job.pixels );
		std::shared_ptr<unsigned char> pixels;
		if ( auto image = job.image.lock() )
		{
			std::lock_guard<std::recursive_mutex> lock( g_mutex );
			job.texture->staged = true;
			bool pending = false;
			for ( const auto &item : image->m_textures )
				pending |= item.second != job.texture && item.second->status.load() == FeImageTexture::Pending && !item.second->from_texture;
			if ( !pending ) pixels = image->take_pixels();
		}
		// Free decoded allocations after dropping all registry locks. Their
		// deleter releases the output slot only when the last upload is done.
		uploaded.reset();
		pixels.reset();
	}

	void finish( Job &job, bool success, bool report=true )
	{
		if ( !success )
		{
			if ( report ) FeLog() << "SDL: artwork upload failed: " << SDL_GetError() << std::endl;
			release( *job.texture );
		}
		job.texture->status.store( success ? FeImageTexture::Ready : FeImageTexture::Failed, std::memory_order_release );
		release_pixels( job );
		auto image = job.image.lock();
		if ( image && success && !image->m_canceled ) FeImageLoader::get_ref().add_to_cache( image->m_key, image );
		FeImageLoader::get_ref().notify_loaded();
	}

	bool take_job( Job &job )
	{
		for ( auto it = m_jobs.begin(); it != m_jobs.end(); )
		{
			auto image = it->image.lock();
			if ( !it->video && !image ) { it->texture->status = FeImageTexture::Failed; it = m_jobs.erase( it ); continue; }
			const bool canceled = it->epoch != m_epoch || ( image && image->m_canceled );
			if ( !canceled && image && !image->is_loaded() ) { ++it; continue; }
			if ( !canceled && image )
			{
				if ( !it->source && !it->pixels ) it->pixels = image->pixels();
				it->width = it->source ? it->source->info.width : image->get_width();
				it->height = it->source ? it->source->info.height : image->get_height();
				if ( !it->source && !it->pixels && !image->failed() ) { ++it; continue; }
				it->image_pin = image;
			}
			job = std::move( *it );
			m_jobs.erase( it );
			return true;
		}
		return false;
	}

	void run()
	{
		std::array<Buffer, 2> buffers;
		for ( auto &buffer : buffers )
		{
			buffer.buffer = create_buffer( buffer_size );
		}
		FeDebug() << "SDL: artwork upload worker: 2 x " << buffer_size << " byte staging buffers" << std::endl;
		for ( ;; )
		{
			collect();
			retire();
			Job job;
			{
				std::unique_lock<std::recursive_mutex> lock( g_mutex );
				if ( !m_run ) break;
				if ( !take_job( job ) )
				{
					m_ready.wait_for( lock, std::chrono::milliseconds( 20 ) );
					continue;
				}
			}
			if ( !( job.pixels || job.source ) )
			{
				finish( job, false, false );
				continue;
			}
			bool success = job.epoch == m_epoch && ( job.pixels || job.source ) && buffers[0].buffer && buffers[1].buffer;
			if ( success )
			{
				Buffer *available = nullptr;
				for ( auto &buffer : buffers )
				{
					if ( buffer.fence && SDL_QueryGPUFence( m_device, buffer.fence ) )
					{
						SDL_ReleaseGPUFence( m_device, buffer.fence );
						buffer.fence = nullptr;
					}
					if ( buffer.fence ) continue;
					available = &buffer;
					break;
				}
				if ( !available )
				{
					{ std::lock_guard<std::recursive_mutex> lock( g_mutex ); m_jobs.push_front( std::move( job ) ); }
					SDL_Delay( 1 );
					continue;
				}
				void *mapped = SDL_MapGPUTransferBuffer( m_device, available->buffer, false );
				if ( !mapped ) { finish( job, false ); continue; }
				Uint32 offset = 0;
				std::vector<Job> jobs;
				std::vector<Region> regions;
				for ( ;; )
				{
					if ( job.epoch != m_epoch || !( job.pixels || job.source ) ) finish( job, false, false );
					else if ( !( job.texture->texture || allocate( job ) ) || static_cast<std::uint64_t>( job.texture->info.width ) * 4 > chunk_size ) finish( job, false );
					else
					{
						const auto before = offset;
						stage( job, *available, mapped, offset, regions );
						const bool complete = job.layer == job.texture->info.layer_count_or_depth;
						if ( complete ) release_pixels( job );
						jobs.push_back( std::move( job ) );
						if ( !complete || offset == before || offset == buffer_size ) break;
					}
					std::lock_guard<std::recursive_mutex> lock( g_mutex );
					if ( !m_run || !take_job( job ) ) break;
				}
				SDL_UnmapGPUTransferBuffer( m_device, available->buffer );
				success = m_run && !regions.empty() && submit( jobs, regions, *available );
				for ( auto &staged : jobs )
					if ( success && m_run && staged.epoch == m_epoch && staged.layer < staged.texture->info.layer_count_or_depth )
					{
						std::lock_guard<std::recursive_mutex> lock( g_mutex );
						m_jobs.push_front( std::move( staged ) );
					}
					else finish( staged, success && m_run && staged.epoch == m_epoch, m_run && staged.epoch == m_epoch );
				continue;
			}
			finish( job, success && m_run && job.epoch == m_epoch, m_run && job.epoch == m_epoch );
		}
		for ( auto &job : m_jobs ) finish( job, false, false );
		m_jobs.clear();
		for ( auto &texture : m_textures )
		{
			release( *texture );
			texture->status = FeImageTexture::Failed;
		}
		m_textures.clear();
		retire( true );
		for ( auto &buffer : buffers )
		{
			if ( buffer.fence ) SDL_ReleaseGPUFence( m_device, buffer.fence );
			if ( buffer.buffer ) SDL_ReleaseGPUTransferBuffer( m_device, buffer.buffer );
		}
	}

	SDL_GPUDevice *m_device;
	unsigned long long m_generation;
	std::atomic<bool> m_run{ true };
	std::atomic<unsigned long long> m_epoch{ 0 };
	std::condition_variable_any m_ready;
	std::deque<Job> m_jobs;
	std::vector<std::shared_ptr<FeImageTexture>> m_textures, m_retired;
	std::vector<std::shared_ptr<FeImageTexture>> m_spares;
	std::vector<Retired> m_waiting;
	std::size_t m_spare_bytes = 0;
	std::thread m_thread;
};

class FeImageLoaderThread
{
public:
	FeImageLoaderThread() : m_run( true ), m_next_image( m_images.end() )
	{
		const unsigned int count = std::min( 4u, std::max( 1u, std::thread::hardware_concurrency() / 4 ) );
		m_memory = std::make_shared<FeImageMemory>( count );
		for ( unsigned int i = 0; i < count; ++i )
		{
			m_threads.emplace_back( &FeImageLoaderThread::run_thread, this );
			m_video_threads.emplace_back( &FeImageLoaderThread::reap_videos, this );
		}
	}

	~FeImageLoaderThread() { stop(); }

	void stop()
	{
		{
			std::lock_guard<std::recursive_mutex> lock( g_mutex );
			m_run = false;
		}
		m_ready.notify_all();
		m_video_ready.notify_all();
		m_memory->stop();
		for ( auto &thread : m_threads )
			if ( thread.joinable() ) thread.join();
		for ( auto &thread : m_video_threads )
			if ( thread.joinable() ) thread.join();
	}

	void request( const std::string &name, std::shared_ptr<FeImageLoaderEntry> *entry, bool prefetch=false, bool mipmap=false )
	{
		{
			std::lock_guard<std::recursive_mutex> lock( g_mutex );
			*entry = m_images[name].lock();
			if ( !*entry || (*entry)->m_canceled || (*entry)->failed() )
			{
				entry->reset( new FeImageLoaderEntry() );
				(*entry)->m_filename = name;
				(*entry)->m_key = name;
				(*entry)->m_mipmap = mipmap;
				(*entry)->m_prefetch = prefetch;
				m_images[name] = *entry;
			}
			if ( !prefetch && (*entry)->m_prefetch )
			{
				(*entry)->m_prefetch = false;
				for ( auto it = m_in.begin(); it != m_in.end(); ++it )
					if ( it->second == *entry )
					{
						std::rotate( m_in.begin(), it, std::next( it ) );
						break;
					}
			}
			if ( !(*entry)->is_loaded() ) queue( name, *entry, prefetch );
		}
		m_ready.notify_one();
	}

	void reload( const std::shared_ptr<FeImageLoaderEntry> &entry, const std::shared_ptr<FeImageTexture> &texture )
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		if ( texture->status == FeImageTexture::Pending && !texture->from_texture && !texture->staged && !entry->has_pixels() && !entry->failed() )
			queue( entry->m_filename, entry, entry->m_prefetch );
		m_ready.notify_one();
	}
	void set_gpu( bool enabled ) { m_memory->set_gpu( enabled ); }

	void cancel()
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		for ( const auto &item : m_images )
			if ( auto image = item.second.lock() )
			{
				bool pending = image->m_decode_pending;
				for ( const auto &texture : image->m_textures ) pending |= texture.second->status == FeImageTexture::Pending;
				if ( pending ) image->m_canceled = true;
			}
		m_ready.notify_all();
	}

	void reap_video( FeMedia *vid )
	{
		{
			std::lock_guard<std::recursive_mutex> lock( g_mutex );
			m_vid.push( vid );
		}
		m_video_ready.notify_one();
	}

	void reap( std::shared_ptr<void> object )
	{
		std::unique_lock<std::recursive_mutex> lock( g_mutex );
		if ( m_run )
		{
			m_retired.push( std::move( object ) );
			lock.unlock();
			m_video_ready.notify_one();
		}
	}

private:
	void queue( const std::string &name, const std::shared_ptr<FeImageLoaderEntry> &entry, bool prefetch )
	{
		if ( !m_run || entry->m_canceled || entry->m_decode_pending || entry->has_pixels() ) return;
		entry->m_decode_pending = true;
		if ( prefetch ) m_in.emplace_back( name, entry );
		else m_in.emplace_front( name, entry );
	}

	void run_thread()
	{
		SDL_SetCurrentThreadPriority( SDL_THREAD_PRIORITY_NORMAL );
		for (;;)
		{
			std::unique_lock<std::recursive_mutex> lock( g_mutex );
			m_ready.wait( lock, [this]{ return !m_run || !m_in.empty(); } );
			if ( m_in.empty() ) return;
			bool counted = false;
			if ( m_run && !m_memory->acquire( counted ) )
			{
				lock.unlock();
				m_memory->wait();
				continue;
			}
			auto job = std::move( m_in.front() );
			m_in.pop_front();
			auto &entry = job.second;
			const bool wanted = m_run && !entry->m_canceled && ( entry->m_prefetch || entry.use_count() > 1 );
			if ( !wanted )
			{
				if ( counted ) m_memory->release();
				// A new request must not attach to a job we are discarding.
				auto found = m_images.find( job.first );
				if ( found != m_images.end() && found->second.lock() == entry ) m_images.erase( found );
				m_next_image = m_images.end();
			}
			if ( wanted ) entry->m_memory = counted ? m_memory : nullptr;
			lock.unlock();
			if ( wanted ) entry->load( job.first );
			lock.lock();
			entry->m_decode_pending = false;
			// Bound stale weak entries without a full registry scan on navigation.
			for ( int i = 0; i < 4 && !m_images.empty(); ++i )
			{
				if ( m_next_image == m_images.end() ) m_next_image = m_images.begin();
				auto current = m_next_image++;
				if ( current->second.expired() ) m_images.erase( current );
			}
			lock.unlock();
			if ( entry->m_canceled ) entry->release_pixels();
			if ( wanted && !entry->failed() )
			{
				FeImageLoader::get_ref().prepare_image( entry, entry->m_mipmap, false );
				FeImageLoader::get_ref().image_loaded( entry );
			}
		}
	}

	void reap_videos()
	{
		for (;;)
		{
			std::unique_lock<std::recursive_mutex> lock( g_mutex );
			m_video_ready.wait( lock, [this]{ return !m_run || !m_vid.empty() || !m_retired.empty(); } );
			if ( !m_retired.empty() )
			{
				auto entry = std::move( m_retired.front() );
				m_retired.pop();
				lock.unlock();
				entry.reset();
				continue;
			}
			if ( m_vid.empty() ) return;
			FeMedia *vid = m_vid.front();
			m_vid.pop();
			lock.unlock();
			delete vid;
		}
	}

	bool m_run;
	std::shared_ptr<FeImageMemory> m_memory;
	std::vector<std::thread> m_threads;
	std::vector<std::thread> m_video_threads;
	std::condition_variable_any m_ready;
	std::condition_variable_any m_video_ready;
	std::map<std::string, std::weak_ptr<FeImageLoaderEntry>> m_images;
	std::map<std::string, std::weak_ptr<FeImageLoaderEntry>>::iterator m_next_image;
	std::deque<std::pair<std::string, std::shared_ptr<FeImageLoaderEntry>>> m_in;
	std::queue<std::shared_ptr<void>> m_retired;
	std::queue<FeMedia *> m_vid;
};

class FeImageLoaderImp
{
public:
	FeImageLoaderImp()
		: m_cache( new FeImageCache( 0 ) ),
		m_load_images_in_bg( false ),
		m_load_mutex( SDL_CreateMutex() ),
		m_load_ready( SDL_CreateCondition() )
	{
		if ( !m_load_mutex || !m_load_ready )
		{
			FeLog() << "SDL: image loader wakeup initialization failed: " << SDL_GetError() << std::endl;
			SDL_DestroyCondition( m_load_ready );
			SDL_DestroyMutex( m_load_mutex );
			throw std::bad_alloc();
		}
	};

	~FeImageLoaderImp()
	{
		if ( m_gpu ) m_gpu->stop();
		m_bg_loader.stop();
		if ( m_cache )
			delete m_cache;
		SDL_DestroyCondition( m_load_ready );
		SDL_DestroyMutex( m_load_mutex );
	}

	FeImageCache *m_cache;
	std::shared_ptr<FeImageUploadThread> m_gpu;
	unsigned long long m_gpu_generation = 0;
	FeImageLoaderThread m_bg_loader;
	bool m_load_images_in_bg;
	SDL_Mutex *m_load_mutex;
	SDL_Condition *m_load_ready;
	unsigned long long m_load_version = 0;
};

FeImageLoaderEntry::FeImageLoaderEntry()
	: m_prefetch( false ), m_width( 0 ), m_height( 0 ),
	m_loaded( false )
{
	static std::atomic<unsigned long long> version( 1 );
	m_version = version.fetch_add( 1, std::memory_order_relaxed );
#ifdef FE_DEBUG
	g_entry_count++;
#endif
}

FeImageLoaderEntry::~FeImageLoaderEntry()
{
#ifdef FE_DEBUG
	g_entry_count--;
#endif
}

void FeImageLoaderEntry::load( const std::string &filename )
{
	FeFileInputStream stream( filename );
	const int error = errno;
	int width = 0, height = 0, ignored = 0;
	std::shared_ptr<unsigned char> data;
	if ( stream.isOpen() )
	{
		stbi_io_callbacks cb = { &read, &skip, &eof };
		auto *pixels = stbi_load_from_callbacks( &cb, &stream, &width, &height, &ignored, STBI_rgb_alpha );
		if ( pixels ) data.reset( pixels, FeImagePixelsDeleter{ m_memory } );
		if ( !data )
			FeLog() << "Error loading image: " << filename << " - " << stbi_failure_reason() << std::endl;
	}
	else
		FeLog() << "Failed to open file: " << filename << std::endl;
	if ( !data && m_memory ) m_memory->release();
	if ( data && m_masked ) mask_pixels( data.get(), static_cast<std::size_t>( width ) * height );
	{
		std::lock_guard<std::mutex> lock( m_pixel_mutex );
		m_data = std::move( data );
		m_width = m_data ? width : 0;
		m_height = m_data ? height : 0;
	}
	m_loaded.store( true, std::memory_order_release );
	m_pixel_ready.notify_all();
	FeImageLoader::get_ref().notify_loaded();
}

std::shared_ptr<unsigned char> FeImageLoaderEntry::pixels() const
{
	std::lock_guard<std::mutex> lock( m_pixel_mutex );
	return m_data;
}

bool FeImageLoaderEntry::has_pixels() const
{
	std::lock_guard<std::mutex> lock( m_pixel_mutex );
	return static_cast<bool>( m_data );
}

void FeImageLoaderEntry::release_pixels()
{
	take_pixels().reset();
}

std::shared_ptr<unsigned char> FeImageLoaderEntry::take_pixels()
{
	std::lock_guard<std::mutex> lock( m_pixel_mutex );
	return std::move( m_data );
}

std::size_t FeImageLoaderEntry::gpu_bytes() const
{
	std::size_t bytes = 0;
	for ( const auto &item : m_textures )
		if ( item.second->status.load( std::memory_order_acquire ) == FeImageTexture::Ready ) bytes += item.second->bytes;
	return bytes;
}

bool FeImageLoaderEntry::copy_pixels( std::vector<unsigned char> &output ) const
{
	auto data = pixels();
	if ( data )
	{
		output.assign( data.get(), data.get() + static_cast<std::size_t>( m_width.load() ) * m_height.load() * 4 );
		return true;
	}
	// Explicit CPU image operations keep their synchronous semantics, without
	// retaining a second image cache or reading pixels back from the GPU.
	FeFileInputStream stream( m_filename );
	if ( !stream.isOpen() ) return false;
	int width = 0, height = 0, ignored = 0;
	stbi_io_callbacks cb = { &read, &skip, &eof };
	data.reset( stbi_load_from_callbacks( &cb, &stream, &width, &height, &ignored, STBI_rgb_alpha ), stbi_image_free );
	if ( !data || width != m_width || height != m_height ) return false;
	if ( m_masked ) mask_pixels( data.get(), static_cast<std::size_t>( width ) * height );
	output.assign( data.get(), data.get() + static_cast<std::size_t>( width ) * height * 4 );
	return true;
}

size_t FeImageLoaderEntry::get_bytes()
{
	return static_cast<size_t>( m_width.load() ) * m_height.load() * 4;
}

int FeImageLoaderEntry::get_width()
{
	return m_width;
}

int FeImageLoaderEntry::get_height()
{
	return m_height;
}

bool FeImageLoaderEntry::is_loaded() const
{
	return m_loaded.load( std::memory_order_acquire );
}

FeImageLoader::FeImageLoader()
	: m_imp( new FeImageLoaderImp() )
{
}

FeImageLoader::~FeImageLoader()
{
	if ( m_imp )
		delete m_imp;
}

bool FeImageLoader::load_image_from_file( const std::string &fn, std::shared_ptr<FeImageLoaderEntry> *e, bool background, bool mipmap,
	std::shared_ptr<FeImageTexture> *texture )
{
	std::string file = fn;
	std::replace( file.begin(), file.end(), '\\', '/' );
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		if ( !m_imp->m_cache->get( file, e ) ) m_imp->m_bg_loader.request( file, e, false, mipmap );
		(*e)->m_prefetch = false;
		auto prepared = prepare_image( *e, mipmap, false );
		if ( texture ) *texture = std::move( prepared );
	}
	if ( !background && ( FePresent::script_get_fep() || !m_imp->m_load_images_in_bg ) && !(*e)->is_loaded() )
	{
		std::unique_lock<std::mutex> lock( (*e)->m_pixel_mutex );
		(*e)->m_pixel_ready.wait( lock, [&]{ return (*e)->is_loaded(); } );
	}
	return (*e)->is_loaded();
}

void FeImageLoader::set_gpu_device( SDL_GPUDevice *device )
{
	std::shared_ptr<FeImageUploadThread> old;
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		old = std::move( m_imp->m_gpu );
		m_imp->m_bg_loader.set_gpu( false );
	}
	if ( old ) old->stop();
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	const auto limit = m_imp->m_cache->get_max_size();
	delete m_imp->m_cache;
	m_imp->m_cache = new FeImageCache( limit );
	if ( device ) m_imp->m_gpu = std::make_shared<FeImageUploadThread>( device, ++m_imp->m_gpu_generation );
	m_imp->m_bg_loader.set_gpu( device != nullptr );
}

void FeImageLoader::cancel_pending()
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	m_imp->m_bg_loader.cancel();
	if ( m_imp->m_gpu ) m_imp->m_gpu->cancel();
}

void FeImageLoader::wait_for_texture( const std::shared_ptr<FeImageTexture> &texture )
{
	while ( texture->status.load( std::memory_order_acquire ) == FeImageTexture::Pending )
		wait_for_load( load_version() );
}

std::shared_ptr<FeImageTexture> FeImageLoader::prepare_image( const std::shared_ptr<FeImageLoaderEntry> &image,
	bool mipmap, bool cube_map, bool blocking )
{
	std::shared_ptr<FeImageUploadThread> gpu;
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		gpu = m_imp->m_gpu;
	}
	if ( !gpu || !image ) return {};
	auto texture = gpu->request( image, mipmap, cube_map );
	if ( !texture ) return {};
	if ( texture->status == FeImageTexture::Pending && !texture->from_texture ) m_imp->m_bg_loader.reload( image, texture );
	if ( blocking ) wait_for_texture( texture );
	return texture;
}

void FeImageLoader::image_loaded( const std::shared_ptr<FeImageLoaderEntry> &image )
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( m_imp->m_gpu ) m_imp->m_gpu->loaded( image );
}

std::shared_ptr<FeImageTexture> FeImageLoader::prepare_video( std::shared_ptr<const unsigned char> pixels,
	unsigned int width, unsigned int height, bool mipmap, bool cube_map, unsigned long long version )
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	return m_imp->m_gpu ? m_imp->m_gpu->video( std::move( pixels ), width, height, mipmap, cube_map, version ) : nullptr;
}

std::shared_ptr<FeImageLoaderEntry> FeImageLoader::mask_image( const std::shared_ptr<FeImageLoaderEntry> &image, bool mipmap )
{
	if ( !image || !image->is_loaded() || image->failed() ) return {};
	const std::string key = std::string( 1, '\0' ) + image->m_filename;
	std::shared_ptr<FeImageLoaderEntry> result;
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		if ( m_imp->m_cache->get( key, &result ) )
		{
			prepare_image( result, mipmap, false );
			return result;
		}
	}
	auto pixels = std::make_shared<std::vector<unsigned char>>();
	if ( !image->copy_pixels( *pixels ) ) return {};
	mask_pixels( pixels->data(), pixels->size() / 4 );
	result.reset( new FeImageLoaderEntry );
	result->m_filename = image->m_filename;
	result->m_key = key;
	result->m_masked = true;
	result->m_width = image->get_width();
	result->m_height = image->get_height();
	result->m_mipmap = mipmap;
	result->m_data = std::shared_ptr<unsigned char>( pixels, pixels->data() );
	result->m_loaded = true;
	prepare_image( result, mipmap, false );
	return result;
}

void FeImageLoader::release_entry( std::shared_ptr<FeImageLoaderEntry> *e )
{
	if ( e ) reap( std::move( *e ) );
}

void FeImageLoader::reap( std::shared_ptr<void> object )
{
	if ( object ) m_imp->m_bg_loader.reap( std::move( object ) );
}

void FeImageLoader::notify_loaded()
{
	SDL_LockMutex( m_imp->m_load_mutex );
	++m_imp->m_load_version;
	SDL_UnlockMutex( m_imp->m_load_mutex );
	SDL_SignalCondition( m_imp->m_load_ready );
}

unsigned long long FeImageLoader::load_version()
{
	SDL_LockMutex( m_imp->m_load_mutex );
	const auto version = m_imp->m_load_version;
	SDL_UnlockMutex( m_imp->m_load_mutex );
	return version;
}

void FeImageLoader::wait_for_load( unsigned long long version )
{
	// SDL's relative timeout avoids coarse MinGW timed waits between input samples.
	SDL_LockMutex( m_imp->m_load_mutex );
	if ( m_imp->m_load_version == version )
		SDL_WaitConditionTimeout( m_imp->m_load_ready, m_imp->m_load_mutex, 1 );
	SDL_UnlockMutex( m_imp->m_load_mutex );
}

bool FeImageLoader::check_loaded( FeImageLoaderEntry *e )
{
	return e && e->is_loaded();
}

void FeImageLoader::reap_video( FeMedia *vid )
{
	vid->signal_stop( true );

	FeImageLoader &il = get_ref();
	if ( il.m_imp )
		il.m_imp->m_bg_loader.reap_video( vid );
}

FeImageLoader &FeImageLoader::get_ref()
{
	static FeImageLoader g_image_loader;

	return g_image_loader;
}

void FeImageLoader::set_cache_size( size_t s )
{
	FeImageLoader &il = get_ref();
	std::lock_guard<std::recursive_mutex> lock( g_mutex );

	il.m_imp->m_cache->resize( s );
}

void FeImageLoader::prune_cache( bool unused )
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	m_imp->m_cache->prune( unused );
}

void FeImageLoader::set_background_loading( bool flag )
{
	if ( auto *present = FePresent::script_get_fep() )
	{
		present->set_script_background_loading( flag );
		return;
	}
	FeImageLoader &il = get_ref();
	il.m_imp->m_load_images_in_bg = flag;

	FeDebug() << "Set background image loading: " << flag << std::endl;
}

bool FeImageLoader::get_background_loading()
{
	if ( auto *present = FePresent::script_get_fep() )
		return present->get_script_background_loading( present->get_script_id() );
	FeImageLoader &il = get_ref();
	return il.m_imp->m_load_images_in_bg;
}

//
//
void FeImageLoader::cache_image( const char *fn )
{
	if ( !m_imp || !cache_max() )
		return;

	// Queue the filename for background loading
	std::string file = fn;
	std::replace( file.begin(), file.end(), '\\', '/' );

	std::shared_ptr<FeImageLoaderEntry> entry;
	{
		std::lock_guard<std::recursive_mutex> lock( g_mutex );
		m_imp->m_cache->get( file, &entry );
	}
	if ( !entry ) m_imp->m_bg_loader.request( file, &entry, true );
	prepare_image( entry, false, false );
	release_entry( &entry );
}

bool FeImageLoader::image_in_cache( const std::string &filename )
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp || !m_imp->m_cache )
		return false;

	return m_imp->m_cache->contains( filename );
}

void FeImageLoader::add_to_cache( const std::string &key, const std::shared_ptr<FeImageLoaderEntry> &entry )
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp || !m_imp->m_cache || !entry )
		return;

	if ( !entry->is_loaded() || !entry->gpu_bytes() )
		return;

	m_imp->m_cache->put(key, entry);
}

long long FeImageLoader::cache_max()
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp->m_cache )
		return 0;

	return m_imp->m_cache->get_max_size();
}

long long FeImageLoader::cache_size()
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp->m_cache )
		return 0;

	return m_imp->m_cache->get_size();
}

int FeImageLoader::cache_count()
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp->m_cache )
		return 0;

	return m_imp->m_cache->get_count();
}

const char *FeImageLoader::cache_get_name_at( int pos )
{
	static thread_local std::string name;
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	name = ( m_imp->m_cache && pos >= 0 && pos < m_imp->m_cache->get_count() )
		? m_imp->m_cache->get_name_at( pos ) : "";
	return name.c_str();
}

long long FeImageLoader::cache_get_size_at( int pos )
{
	std::lock_guard<std::recursive_mutex> lock( g_mutex );
	if ( !m_imp->m_cache || ( pos < 0 ) || ( pos >= (int)m_imp->m_cache->get_count() ))
		return 0;

	return m_imp->m_cache->get_size_at( pos );
}
