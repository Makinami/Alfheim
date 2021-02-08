#define PerfGraph_RootSig \
	"RootFlags(0), " \
	"CBV(b0), " \
	"DescriptorTable(UAV(u0, numDescriptors = 2)), " \
	"SRV(t0, visibility = SHADER_VISIBILITY_VERTEX), " \
	"RootConstants(b1, num32BitConstants = 3)"