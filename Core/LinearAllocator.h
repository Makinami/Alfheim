#pragma once

#include "GpuResource.h"
#include <vector>
#include <queue>
#include <mutex>

#define DEFAULT_ALIGN 256

struct DynAlloc
{
	DynAlloc(GpuResource& BaseResource, size_t ThisOffset, size_t ThisSize)
		: Buffer(BaseResource), Offset(ThisOffset), Size(ThisSize) {}

	GpuResource& Buffer;
	size_t Offset;
	size_t Size;
	void* DataPtr;
	D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
};

class LinearAllocationPage : public GpuResource
{
public:
	LinearAllocationPage(ID3D12Resource* pResource, D3D12_RESOURCE_STATES Usage) : GpuResource(pResource, Usage)
	{
		m_GpuVirtualAddress = m_pResource->GetGPUVirtualAddress();
		m_pResource->Map(0, nullptr, &m_CpuVirtualAddress);
	}

	~LinearAllocationPage()
	{
		Unmap();
	}

	void Map(void)
	{
		if (m_CpuVirtualAddress == nullptr)
		{
			m_pResource->Map(0, nullptr, &m_CpuVirtualAddress);
		}
	}

	void Unmap(void)
	{
		if (m_CpuVirtualAddress != nullptr)
		{
			m_pResource->Unmap(0, nullptr);
			m_CpuVirtualAddress = nullptr;
		}
	}

	void* m_CpuVirtualAddress;
	D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
};

enum LinearAllocatorType
{
	kInvalidAllocator = -1,

	kGpuExclusive = 0,  // DEFAULT  GPU-writable (via UAV)
	kCpuWritable = 1,   // UPLOAD   CPU-writable (but write combined)

	knumAllocatorTypes
};

enum
{
	kGpuAllocatorPageSize = 0x10000,    // 64K
	kCpuAllocatorPageSize = 0x200000    // 2MB
};

class LinearAllocatorPageManager
{
public:
	LinearAllocatorPageManager();
	LinearAllocationPage* RequestPage(void);
	LinearAllocationPage* CreateNewPage(size_t PageSize = 0);

	void DiscardPages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages);

	void FreeLargePages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages);

	void Destroy(void) { m_PagePool.clear(); }

private:

	static LinearAllocatorType sm_AutoType;

	LinearAllocatorType m_AllocationType;
	std::vector<std::unique_ptr<LinearAllocationPage>> m_PagePool;
	std::queue<std::pair<uint64_t, LinearAllocationPage*>> m_RetiredPages;
	std::queue<std::pair<uint64_t, LinearAllocationPage*>> m_DeletionQueue;
	std::queue<LinearAllocationPage*> m_AvailablePages;
	std::mutex m_Mutex;
};

class LinearAllocator
{
public:
	LinearAllocator(LinearAllocatorType Type)
		: m_AllocationType(Type)
	{
		ASSERT(Type > kInvalidAllocator && Type < knumAllocatorTypes);
		m_PageSize = (Type == kGpuExclusive ? kGpuAllocatorPageSize : kCpuAllocatorPageSize);
	}

	DynAlloc Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);

	void CleanupUsedPages(uint64_t FenceID);

	static void DestroyAll(void)
	{
		for (auto& manager : sm_PageManager)
			manager.Destroy();
	}

private:
	DynAlloc AllocateLargePage(size_t SizeInBytes);

	static LinearAllocatorPageManager sm_PageManager[2];

	LinearAllocatorType m_AllocationType;
	size_t m_PageSize = 0;
	size_t m_CurOffset = ~(size_t)0;
	LinearAllocationPage* m_CurPage = nullptr;
	std::vector<LinearAllocationPage*> m_RetiredPages;
	std::vector<LinearAllocationPage*> m_LargePageList;
};

