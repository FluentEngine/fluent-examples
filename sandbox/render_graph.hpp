#pragma once

#include <list>
#include <unordered_map>
#include "fluent/fluent.hpp"

namespace fluent::rg
{

using PassCreateCallback  = void ( * )( RenderPass*, void* user_data );
using PassExecuteCallback = void ( * )( CommandBuffer*, void* user_data );
using PassDestroyCallback = void ( * )( void* user_data );

struct GraphPass
{
	RenderPassInfo      info;
	RenderPassBeginInfo begin_info;
	RenderPass*         pass;
	PassCreateCallback  create_callback;
	PassExecuteCallback execute_callback;
	PassDestroyCallback destroy_callback;
	void*               user_data;

	void
	execute( CommandBuffer* cmd )
	{
		if ( execute_callback )
		{
			execute_callback( cmd, user_data );
		}
	}

	void
	set_user_data( void* data )
	{
		this->user_data = data;
	}

	void
	set_create_callback( PassCreateCallback&& callback )
	{
		this->create_callback = callback;
	}

	void
	set_destroy_callback( PassDestroyCallback&& callback )
	{
		this->destroy_callback = callback;
	}

	void
	set_execute_callback( PassExecuteCallback&& callback )
	{
		this->execute_callback = callback;
	}

	void
	set_color_clear_value( u32 idx, const Vector4& color )
	{
		info.color_attachment_count++;
		begin_info.clear_values[ idx ].color[ 0 ] = color.r;
		begin_info.clear_values[ idx ].color[ 1 ] = color.g;
		begin_info.clear_values[ idx ].color[ 2 ] = color.b;
		begin_info.clear_values[ idx ].color[ 3 ] = color.a;
	}
};

template <class T>
inline void
hash_combine( std::size_t& s, const T& v )
{
	std::hash<T> h;
	s ^= h( v ) + 0x9e3779b9 + ( s << 6 ) + ( s >> 2 );
}

template <class T>
struct PassHash;

template <>
struct PassHash<GraphPass>
{
	std::size_t
	operator()( RenderPassInfo const& info ) const
	{
		std::size_t res = 0;
		hash_combine( res, info.color_attachment_count );
		hash_combine( res, info.width );
		hash_combine( res, info.height );
		for ( u32 i = 0; i < info.color_attachment_count; ++i )
		{
			hash_combine( res, info.color_attachment_load_ops[ i ] );
			hash_combine( res, info.color_image_states[ i ] );
			hash_combine( res, info.color_attachments[ i ] );
		}
		hash_combine( res, info.depth_stencil );
		hash_combine( res, info.depth_stencil_load_op );
		hash_combine( res, info.depth_stencil_state );

		return res;
	}
};

using GraphPassHasher = PassHash<GraphPass>;

class RenderGraph
{
private:
	const Device* device;

	std::unordered_map<u32, RenderPass*> passes;

	std::vector<GraphPass> passes_to_execute;

	RenderPass*
	get_render_pass( const RenderPassInfo& info );

public:
	void
	init( const Device* device );

	void
	shutdown();

	void
	build();

	void
	execute( CommandBuffer* cmd, Image* image );

	GraphPass*
	add_pass( const std::string& name );
};

} // namespace fluent::rg
