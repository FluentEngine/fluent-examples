	// 1011.9.0
	 #pragma once
const uint32_t shader_main_vert[] = {
	0x07230203,0x00010000,0x0008000a,0x00000052,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0008000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000043,0x0000004b,0x0000004f,
	0x00030003,0x00000005,0x000001f4,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00040005,
	0x00000007,0x75706e49,0x00000074,0x00070006,0x00000007,0x00000000,0x74726576,0x695f7865,
	0x7865646e,0x00000000,0x00040005,0x0000000c,0x7074754f,0x00007475,0x00060006,0x0000000c,
	0x00000000,0x67617266,0x6c6f635f,0x0000726f,0x00060006,0x0000000c,0x00000001,0x69736f70,
	0x6e6f6974,0x00000000,0x00080005,0x0000000f,0x69616d40,0x7473286e,0x74637572,0x706e492d,
	0x752d7475,0x003b3131,0x00050005,0x0000000e,0x67617473,0x6e695f65,0x00747570,0x00060005,
	0x00000013,0x74726576,0x695f7865,0x7865646e,0x00000000,0x00060005,0x0000001a,0x67617473,
	0x756f5f65,0x74757074,0x00000000,0x00050005,0x00000028,0x65646e69,0x6c626178,0x00000065,
	0x00050005,0x00000039,0x65646e69,0x6c626178,0x00000065,0x00050005,0x00000041,0x67617473,
	0x6e695f65,0x00747570,0x00090005,0x00000043,0x67617473,0x6e695f65,0x2e747570,0x74726576,
	0x695f7865,0x7865646e,0x00000000,0x00050005,0x00000046,0x74616c66,0x546e6574,0x00706d65,
	0x00040005,0x00000047,0x61726170,0x0000006d,0x000a0005,0x0000004b,0x746e6540,0x6f507972,
	0x4f746e69,0x75707475,0x72662e74,0x635f6761,0x726f6c6f,0x00000000,0x00090005,0x0000004f,
	0x746e6540,0x6f507972,0x4f746e69,0x75707475,0x6f702e74,0x69746973,0x00006e6f,0x00040047,
	0x00000043,0x0000000b,0x0000002a,0x00040047,0x0000004b,0x0000001e,0x00000000,0x00040047,
	0x0000004f,0x0000000b,0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
	0x00040015,0x00000006,0x00000020,0x00000000,0x0003001e,0x00000007,0x00000006,0x00040020,
	0x00000008,0x00000007,0x00000007,0x00030016,0x00000009,0x00000020,0x00040017,0x0000000a,
	0x00000009,0x00000003,0x00040017,0x0000000b,0x00000009,0x00000004,0x0004001e,0x0000000c,
	0x0000000a,0x0000000b,0x00040021,0x0000000d,0x0000000c,0x00000008,0x00040015,0x00000011,
	0x00000020,0x00000001,0x00040020,0x00000012,0x00000007,0x00000011,0x0004002b,0x00000011,
	0x00000014,0x00000000,0x00040020,0x00000015,0x00000007,0x00000006,0x00040020,0x00000019,
	0x00000007,0x0000000c,0x0004002b,0x00000011,0x0000001b,0x00000001,0x00040017,0x0000001c,
	0x00000009,0x00000002,0x0004002b,0x00000006,0x0000001d,0x00000003,0x0004001c,0x0000001e,
	0x0000001c,0x0000001d,0x0004002b,0x00000009,0x0000001f,0xbf000000,0x0005002c,0x0000001c,
	0x00000020,0x0000001f,0x0000001f,0x0004002b,0x00000009,0x00000021,0x3f000000,0x0005002c,
	0x0000001c,0x00000022,0x00000021,0x0000001f,0x0004002b,0x00000009,0x00000023,0x00000000,
	0x0005002c,0x0000001c,0x00000024,0x00000023,0x00000021,0x0006002c,0x0000001e,0x00000025,
	0x00000020,0x00000022,0x00000024,0x00040020,0x00000027,0x00000007,0x0000001e,0x00040020,
	0x00000029,0x00000007,0x0000001c,0x0004002b,0x00000009,0x0000002c,0x3f800000,0x00040020,
	0x00000030,0x00000007,0x0000000b,0x0004001c,0x00000032,0x0000000a,0x0000001d,0x0006002c,
	0x0000000a,0x00000033,0x0000002c,0x00000023,0x00000023,0x0006002c,0x0000000a,0x00000034,
	0x00000023,0x0000002c,0x00000023,0x0006002c,0x0000000a,0x00000035,0x00000023,0x00000023,
	0x0000002c,0x0006002c,0x00000032,0x00000036,0x00000033,0x00000034,0x00000035,0x00040020,
	0x00000038,0x00000007,0x00000032,0x00040020,0x0000003a,0x00000007,0x0000000a,0x00040020,
	0x00000042,0x00000001,0x00000006,0x0004003b,0x00000042,0x00000043,0x00000001,0x00040020,
	0x0000004a,0x00000003,0x0000000a,0x0004003b,0x0000004a,0x0000004b,0x00000003,0x00040020,
	0x0000004e,0x00000003,0x0000000b,0x0004003b,0x0000004e,0x0000004f,0x00000003,0x00050036,
	0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003b,0x00000008,
	0x00000041,0x00000007,0x0004003b,0x00000019,0x00000046,0x00000007,0x0004003b,0x00000008,
	0x00000047,0x00000007,0x0004003d,0x00000006,0x00000044,0x00000043,0x00050041,0x00000015,
	0x00000045,0x00000041,0x00000014,0x0003003e,0x00000045,0x00000044,0x0004003d,0x00000007,
	0x00000048,0x00000041,0x0003003e,0x00000047,0x00000048,0x00050039,0x0000000c,0x00000049,
	0x0000000f,0x00000047,0x0003003e,0x00000046,0x00000049,0x00050041,0x0000003a,0x0000004c,
	0x00000046,0x00000014,0x0004003d,0x0000000a,0x0000004d,0x0000004c,0x0003003e,0x0000004b,
	0x0000004d,0x00050041,0x00000030,0x00000050,0x00000046,0x0000001b,0x0004003d,0x0000000b,
	0x00000051,0x00000050,0x0003003e,0x0000004f,0x00000051,0x000100fd,0x00010038,0x00050036,
	0x0000000c,0x0000000f,0x00000000,0x0000000d,0x00030037,0x00000008,0x0000000e,0x000200f8,
	0x00000010,0x0004003b,0x00000012,0x00000013,0x00000007,0x0004003b,0x00000019,0x0000001a,
	0x00000007,0x0004003b,0x00000027,0x00000028,0x00000007,0x0004003b,0x00000038,0x00000039,
	0x00000007,0x00050041,0x00000015,0x00000016,0x0000000e,0x00000014,0x0004003d,0x00000006,
	0x00000017,0x00000016,0x0004007c,0x00000011,0x00000018,0x00000017,0x0003003e,0x00000013,
	0x00000018,0x0004003d,0x00000011,0x00000026,0x00000013,0x0003003e,0x00000028,0x00000025,
	0x00050041,0x00000029,0x0000002a,0x00000028,0x00000026,0x0004003d,0x0000001c,0x0000002b,
	0x0000002a,0x00050051,0x00000009,0x0000002d,0x0000002b,0x00000000,0x00050051,0x00000009,
	0x0000002e,0x0000002b,0x00000001,0x00070050,0x0000000b,0x0000002f,0x0000002d,0x0000002e,
	0x00000023,0x0000002c,0x00050041,0x00000030,0x00000031,0x0000001a,0x0000001b,0x0003003e,
	0x00000031,0x0000002f,0x0004003d,0x00000011,0x00000037,0x00000013,0x0003003e,0x00000039,
	0x00000036,0x00050041,0x0000003a,0x0000003b,0x00000039,0x00000037,0x0004003d,0x0000000a,
	0x0000003c,0x0000003b,0x00050041,0x0000003a,0x0000003d,0x0000001a,0x00000014,0x0003003e,
	0x0000003d,0x0000003c,0x0004003d,0x0000000c,0x0000003e,0x0000001a,0x000200fe,0x0000003e,
	0x00010038
};
