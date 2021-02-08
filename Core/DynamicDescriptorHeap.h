#pragma once

#include "DescriptorHeap.h"
#include "RootSignature.h"
#include <vector>
#include <queue>

namespace Graphics
{
	extern ID3D12Device* g_Device;
}

class DynamicDescriptorHeap
{
public:
	DynamicDescriptorHeap(CommandContext& OwningContext, D3D12_DESCRIPTOR_HEAP_TYPE HeapType);

	static void DestroyAll(void)
	{
		for (auto& pool : sm_DescriptorHeapPool)
			pool.clear();
	}

	void CleanupUsedHeaps(uint64_t fenceValue);

	void SetGraphicsDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
	{
		m_GraphicsHandleCache.StageDescriptorHandles(RootIndex, Offset, NumHandles, Handles);
	}

	void SetComputeDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
	{
		m_ComputeHandleCache.StageDescriptorHandles(RootIndex, Offset, NumHandles, Handles);
	}

	void ParseGraphicsRootSignature(const RootSignature& RootSig)
	{
		m_GraphicsHandleCache.ParseRootSignature(m_DescriptorType, RootSig);
	}

	inline void CommitGraphicsRootDescriptorTables(ID3D12GraphicsCommandList* CmdList)
	{
		if (m_GraphicsHandleCache.m_StaleRootParamsBitMap != 0)
			CopyAndBindStagedTables(m_GraphicsHandleCache, CmdList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
	}

	inline void CommitComputeRootDescriptorTables(ID3D12GraphicsCommandList* CmdList)
	{
		if (m_ComputeHandleCache.m_StaleRootParamsBitMap != 0)
			CopyAndBindStagedTables(m_ComputeHandleCache, CmdList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
	}

private:

	static const uint32_t kNumDescriptorsPerHeap = 1024;
	static std::mutex sm_Mutex;
	static std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> sm_DescriptorHeapPool[2];
	static std::queue<std::pair<uint64_t, ID3D12DescriptorHeap*>> sm_RetiredDescriptorHeaps[2];
	static std::queue<ID3D12DescriptorHeap*> sm_AvailableDescriptorHeaps[2];

	static ID3D12DescriptorHeap* RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType);
	static void DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, uint64_t FenceValueForReset, const std::vector<ID3D12DescriptorHeap*>& UsedHeaps);

	CommandContext& m_OwningContext;
	ID3D12DescriptorHeap* m_CurrentHeapPtr = nullptr;
	const D3D12_DESCRIPTOR_HEAP_TYPE m_DescriptorType;
	uint32_t m_DescriptorSize;
	uint32_t m_CurrentOffset = 0;
	DescriptorHandle m_FirstDescriptor;
	std::vector<ID3D12DescriptorHeap*> m_RetireHeaps;

	struct DescriptorTableCache
	{
		uint32_t AssignedHandlesBitMap = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE* TableStart;
		uint32_t TableSize;
	};

	struct DescriptorHandleCache
	{
		void ClearCache()
		{
			m_RootDescriptorTablesBitMap = 0;
			m_MaxCachedDescriptors = 0;
		}

		uint32_t m_RootDescriptorTablesBitMap = 0;
		uint32_t m_StaleRootParamsBitMap;
		uint32_t m_MaxCachedDescriptors = 0;

		static const uint32_t kMaxNumDescriptors = 256;
		static const uint32_t kMaxNumDescriptorTables = 16;

		uint32_t ComputeStagedSize();
		void CopyAndBindStaleTables(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t DescriptorSize, DescriptorHandle DestHandleStart, ID3D12GraphicsCommandList* CmdList,
			void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE));

		DescriptorTableCache m_RootDescriptorTable[kMaxNumDescriptorTables];
		D3D12_CPU_DESCRIPTOR_HANDLE m_HandleCache[kMaxNumDescriptors];

		void UnbindAllValid();
		void StageDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);
		void ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE Type, const RootSignature& RootSig);
	};

	DescriptorHandleCache m_GraphicsHandleCache;
	DescriptorHandleCache m_ComputeHandleCache;

	bool HasSpace(uint32_t Count)
	{
		return (m_CurrentHeapPtr != nullptr && m_CurrentOffset + Count <= kNumDescriptorsPerHeap);
	}

	void RetireCurrentHeap(void);
	void RetireUsedHeaps(uint64_t fenceValue);
	ID3D12DescriptorHeap* GetHeapPointer();

	DescriptorHandle Allocate(UINT Count)
	{
		DescriptorHandle ret = m_FirstDescriptor + m_CurrentOffset * m_DescriptorSize;
		m_CurrentOffset += Count;
		return ret;
	}

	void CopyAndBindStagedTables(DescriptorHandleCache& HandleCache, ID3D12GraphicsCommandList* CmdList,
		void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE));

	void UnbindAllValid(void);
};

