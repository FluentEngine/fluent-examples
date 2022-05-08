#pragma once

#include <list>
#include <unordered_map>
#include "fluent/fluent.hpp"

namespace fluent::rg
{
class RenderGraph
{
private:
	const Device* device;
public:
	void
	init( const Device* device )
	{
		this->device = device;
	}

	void
	shutdown()
	{
	}

	void
	build() {};

	void
	execute(CommandBuffer* cmd, Image* image)
	{
		ImageBarrier barrier{};
		barrier.image = image;
		barrier.old_state = ResourceState::eUndefined;
		barrier.new_state = ResourceState::ePresent;

		cmd_barrier(cmd, 0, nullptr, 0, nullptr, 1, &barrier);
	};
};

} // namespace fluent::rg
