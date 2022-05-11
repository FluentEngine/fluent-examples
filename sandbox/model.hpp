#pragma once

#include <fluent/fluent.hpp>

namespace fluent
{

struct Vertex
{
	Vector3 position;
	Vector3 normal;
	Vector2 tex_coord;

	bool
	operator==( const Vertex& other ) const
	{
		return position == other.position && normal == other.normal &&
		       tex_coord == other.tex_coord;
	}
};

template <typename T>
struct VertexHash;

template <>
struct VertexHash<Vertex>
{
	std::size_t
	operator()( Vertex const& vertex ) const
	{
		auto p = std::hash<glm::vec3>()( vertex.position );
		auto n = ( std::hash<glm::vec3>()( vertex.normal ) << 1 );
		auto t = std::hash<glm::vec2>()( vertex.tex_coord ) << 1;
		return ( ( p ^ n ) >> 1 ) ^ ( t );
	}
};

using VertexHasher = VertexHash<Vertex>;

struct Model
{
	u32 first_vertex;
	u32 first_index;
	u32 index_count;
};

static inline void
load_model( std::vector<Vertex>& vertices, std::vector<u32>& indices )
{
	tinyobj::attrib_t                attrib;
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      warn, err;

	auto model_path = fs::get_models_directory() + MODEL_NAME;

	auto res = tinyobj::LoadObj( &attrib,
	                             &shapes,
	                             &materials,
	                             &warn,
	                             &err,
	                             model_path.c_str() );

	FT_ASSERT( res && "failed to load model" );

	std::unordered_map<Vertex, u32, VertexHasher> unique_vertices {};

	for ( const auto& shape : shapes )
	{
		for ( const auto& index : shape.mesh.indices )
		{
			Vertex vertex {};

			vertex.position = { attrib.vertices[ 3 * index.vertex_index + 0 ],
				                attrib.vertices[ 3 * index.vertex_index + 1 ],
				                attrib.vertices[ 3 * index.vertex_index + 2 ] };

			vertex.tex_coord = {
				attrib.texcoords[ 2 * index.texcoord_index + 0 ],
				1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ]
			};

			vertex.normal = { attrib.normals[ 3 * index.vertex_index + 0 ],
				              attrib.normals[ 3 * index.vertex_index + 1 ],
				              attrib.normals[ 3 * index.vertex_index + 2 ] };

			if ( unique_vertices.count( vertex ) == 0 )
			{
				unique_vertices[ vertex ] =
				    static_cast<uint32_t>( vertices.size() );
				vertices.push_back( vertex );
			}

			indices.push_back( unique_vertices[ vertex ] );
		}
	}
}

} // namespace fluent
