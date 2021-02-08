#pragma once

#include <mutex>
#include "CommandAllocatorPool.h"

class CommandQueue
{
	friend class CommandListManager;
	friend class CommandContext;

public:
	CommandQueue(D3D12_COMMAND_LIST_TYPE Type);
	~CommandQueue();

	void Create(ID3D12Device* pDevice);
	void Shutdown();

	inline bool IsReady()
	{
		return m_CommandQueue != nullptr;
	}

	uint64_t IncrementFence(void);
	bool IsFenceComplete(uint64_t FenceValue);
	void WaitForFence(uint64_t FenceValue);
	void WaitForIdle(void) { WaitForFence(IncrementFence()); }

	ID3D12CommandQueue* GetCommandQueue() { return m_CommandQueue; }

private:
	uint64_t ExecuteCommandList(ID3D12CommandList* List);
	ID3D12CommandAllocator* RequestAllocator(void);
	void DiscardAllocator(uint64_t FenceValueForReset, ID3D12CommandAllocator* Allocator);

	ID3D12CommandQueue* m_CommandQueue = nullptr;
	
	const D3D12_COMMAND_LIST_TYPE m_Type;

	CommandAllocatorPool m_AllocatorPool;
	std::mutex m_FenceMutex;
	std::mutex m_EventMutex;

	ID3D12Fence* m_pFence = nullptr;
	uint64_t m_NextFenceValue;
	uint64_t m_LastCompletedFenceValue;
	HANDLE m_FenceEventHandle;
};

class CommandListManager
{
	friend class CommandContext;

public:
	CommandListManager() = default;
	~CommandListManager();

	void Create(ID3D12Device* pDevice);
	void Shutdown();

	CommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE Type = D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		switch (Type)
		{
			case D3D12_COMMAND_LIST_TYPE_COMPUTE: return m_ComputeQueue;
			case D3D12_COMMAND_LIST_TYPE_COPY: return m_CopyQueue;
			default: return m_GraphicsQueue;
		}
	}

	ID3D12CommandQueue* GetCommandQueue()
	{
		return m_GraphicsQueue.GetCommandQueue();
	}

	void CreateNewCommandList(D3D12_COMMAND_LIST_TYPE Type, ID3D12GraphicsCommandList** List, ID3D12CommandAllocator** Allocator);

	bool IsFenceComplete(uint64_t FenceValue)
	{
		return GetQueue(D3D12_COMMAND_LIST_TYPE(FenceValue >> 56)).IsFenceComplete(FenceValue);
	}

	void WaitForFence(uint64_t FenceValue);

	void IdleGPU(void)
	{
		m_GraphicsQueue.WaitForIdle();
		m_ComputeQueue.WaitForIdle();
		m_CopyQueue.WaitForIdle();
	}

private:
	ID3D12Device* m_Device = nullptr;

	CommandQueue m_GraphicsQueue = { D3D12_COMMAND_LIST_TYPE_DIRECT };
	CommandQueue m_ComputeQueue = { D3D12_COMMAND_LIST_TYPE_COMPUTE };
	CommandQueue m_CopyQueue = { D3D12_COMMAND_LIST_TYPE_COPY };
};