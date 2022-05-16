#pragma once

#include <fluent/fluent.hpp>

namespace fluent
{

void
editor_init( RendererAPI api );

void
editor_render();

void
editor_set_scene_image( Image* image );

b32
editor_exit_requested();

b32
editor_api_change_requested( RendererAPI* api );

b32
editor_viewport_resized( Vector2& size );

} // namespace fluent
