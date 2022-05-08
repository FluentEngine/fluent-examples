#pragma once

#include <list>
#include <unordered_map>
#include "fluent/fluent.hpp"

namespace fluent::rg
{
struct GraphPass
{
	RenderPass* pass;

	void
	create( const Device* device, const RenderPassInfo& info )
	{
		create_render_pass( device, &info, &pass );
	}

	void
	destroy( const Device* device )
	{
		destroy_render_pass( device, pass );
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

	std::unordered_map<u32, GraphPass> passes;

	GraphPass*
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
};

} // namespace fluent::rg
