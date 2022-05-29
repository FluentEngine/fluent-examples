#ifdef FT_VULKAN
    #define cbuf(name, bind) \
    [[vk::binding(bind)]] \
    cbuffer name : register(b##bind)
#else
    #define cbuf(name, bind) cbuffer name : register(b##bind)
#endif

#ifdef FT_VULKAN
    #define sampler(name, bind) \
    [[vk::binding(bind)]] \
    SamplerState name : register(s##bind)
#else
    #define sampler(name, bind) SamplerState name : register(s##bind)
#endif

#ifdef FT_VULKAN
    #define texture2D(type, name, bind) \
    [[vk::binding(bind)]] \
    Texture2D<type> name : register(t##bind)
#else
    #define texture2D(type, name, bind) Texture2D<type> name : register(t##bind)
#endif

#ifdef FT_VULKAN
    #define rwtexture2D(type, name, bind) \
	[[vk::binding(bind)]] \
	RWTexture2D<type> name : register(u##bind)
#else
    #define rwtexture2D(type, name, bind) RWTexture2D<type> name : register(u##bind)
#endif

#ifdef FT_VULKAN
    #define push_constant(name) [[vk::push_constant]] cbuffer name
#else
    #define push_constant(name) cbuffer name
#endif
