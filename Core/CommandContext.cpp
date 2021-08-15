#include "pch.h"
#include "CommandContext.h"
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "GraphicsCore.h"
#include "EngineProfiling.h"

#ifndef RELEASE
	#pragma warning(push)
	#pragma warning(disable: 4100)
	#include <d3d11_2.h>
	#include <pix3.h>
	#pragma	warning(pop)
#endif

using namespace Graphics;

CommandContext::CommandContext(D3D12_COMMAND_LIST_TYPE Type) :
	m_Type(Type),
	m_DynamicViewDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_DynamicSamplerDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
{}

void CommandContext::Reset(void)
{
	// We only call Reset() on previously freed contexts.  The command list persists, but we must
	// request a new allocator.
	ASSERT(m_CommandList != nullptr && m_CurrentAllocator == nullptr);
	m_CurrentAllocator = g_CommandManager.GetQueue(m_Type).RequestAllocator();
	m_CommandList->Reset(m_CurrentAllocator, nullptr);

	m_CurGraphicsRootSignature = nullptr;
	m_CurPipelineState = nullptr;
	m_CurComputeRootSignature = nullptr;
	m_NumBarriersToFlush = 0;

	BindDescriptorHeaps();
}

CommandContext::~CommandContext(void)
{
	if (m_CommandList != nullptr)
		m_CommandList->Release();
}

void CommandContext::DestroyAllContexts(void)
{
	LinearAllocator::DestroyAll();
	DynamicDescriptorHeap::DestroyAll();
	g_ContextManager.DestroyAllContexts();
}

CommandContext& CommandContext::Begin(const std::wstring ID)
{
	CommandContext* NewContext = g_ContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
	NewContext->SetID(ID);
	if (ID.length() > 0)
		EngineProfiling::BeginBlock(ID, NewContext);
	return *NewContext;
}

ComputeContext& ComputeContext::Begin(const std::wstring& ID, bool Async)
{
	ComputeContext& NewContext = g_ContextManager.AllocateContext(
		Async ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT)->GetComputeContext();
	NewContext.SetID(ID);
	if (ID.length() > 0)
		EngineProfiling::BeginBlock(ID, &NewContext);
	return NewContext;
}

void CommandContext::Initialize(void)
{
	g_CommandManager.CreateNewCommandList(m_Type, &m_CommandList, &m_CurrentAllocator);
}

uint64_t CommandContext::Finish(bool WaitForCompletion)
{
	ASSERT(m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT || m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE);

	FlushResourceBarriers();

	if (m_ID.length() > 0)
		EngineProfiling::EndBlock(this);

	ASSERT(m_CurrentAllocator != nullptr);

	CommandQueue& Queue = g_CommandManager.GetQueue(m_Type);

	uint64_t FenceValue = Queue.ExecuteCommandList(m_CommandList);
	Queue.DiscardAllocator(FenceValue, m_CurrentAllocator);
	m_CurrentAllocator = nullptr;

	m_CpuLinearAllocator.CleanupUsedPages(FenceValue);
	m_GpuLinearAllocator.CleanupUsedPages(FenceValue);
	m_DynamicViewDescriptorHeap.CleanupUsedHeaps(FenceValue);
	m_DynamicSamplerDescriptorHeap.CleanupUsedHeaps(FenceValue);

	if (WaitForCompletion)
		g_CommandManager.WaitForFence(FenceValue);

	g_ContextManager.FreeContext(this);

	return FenceValue;
}

void CommandContext::InitializeTexture(GpuResource& Dest, UINT NumSubresources, D3D12_SUBRESOURCE_DATA SubData[])
{
	UINT64 uploadBufferSize = GetRequiredIntermediateSize(Dest.GetResource(), 0, NumSubresources);

	CommandContext& InitContext = CommandContext::Begin();

	// copy data to the intermediate upload heap and then schedule a copy from the upload heap to the default texture
	DynAlloc mem = InitContext.ReserveUploadMemory(uploadBufferSize);
	UpdateSubresources(InitContext.m_CommandList, Dest.GetResource(), mem.Buffer.GetResource(), 0, 0, NumSubresources, SubData);
	InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ);

	// Execute the command list and wait for it to finish so we can release the upload buffer
	//? can't we not wait?
	InitContext.Finish(true);
}

void CommandContext::InitializeBuffer(GpuResource& Dest, const void* BufferData, size_t NumBytes, size_t Offset)
{
	auto& InitContext = CommandContext::Begin();

	auto mem = InitContext.ReserveUploadMemory(NumBytes);
	SIMDMemCopy(mem.DataPtr, BufferData, Math::DivideByMultiple(NumBytes, 16));

	// copy data to the intermediate uplod heap and then schedule a copy from the upload heap to the default texture
	InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
	InitContext.m_CommandList->CopyBufferRegion(Dest.GetResource(), Offset, mem.Buffer.GetResource(), 0, NumBytes);
	InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ, true);

	// Execute the command list and wait for it to finish so we can release the uplaod buffer
	InitContext.Finish(true);
}

void CommandContext::TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
	D3D12_RESOURCE_STATES OldState = Resource.m_UsageState;

	if (m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
	{
		ASSERT((OldState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == OldState);
		ASSERT((NewState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == NewState);
	}

	if (OldState != NewState)
	{
		ASSERT(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
		D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Transition.pResource = Resource.GetResource();
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = OldState;
		BarrierDesc.Transition.StateAfter = NewState;

		if (NewState == Resource.m_TransitioningState)
		{
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
			Resource.m_TransitioningState = (D3D12_RESOURCE_STATES)-1;
		}
		else
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		Resource.m_UsageState = NewState;
	}
	else if (NewState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		InsertUAVBarrier(Resource, FlushImmediate);

	if (FlushImmediate || m_NumBarriersToFlush == 16)
		FlushResourceBarriers();
}

void CommandContext::InsertUAVBarrier(GpuResource& Resource, bool FlushImmediate)
{
	ASSERT(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
	D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.UAV.pResource = Resource.GetResource();

	if (FlushImmediate)
		FlushResourceBarriers();
}

void CommandContext::PIXBeginEvent([[maybe_unused]] const wchar_t* label)
{
#ifndef RELEASE
	::PIXBeginEvent(m_CommandList, 0, label);
#endif
}

void CommandContext::PIXEndEvent(void)
{
#ifndef RELEASE
	::PIXEndEvent(m_CommandList);
#endif // !RELEASE
}

void CommandContext::PixSetMerker([[maybe_unused]] const wchar_t* label)
{
#ifndef RELEASE
	::PIXSetMarker(m_CommandList, 0, label);
#endif // !RELEASE
}

void CommandContext::BindDescriptorHeaps(void)
{
	UINT NonNullHeaps = 0;
	ID3D12DescriptorHeap* HeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	for (auto HeapIter : m_CurrentDescriptorHeaps)
	{
		if (HeapIter != nullptr)
			HeapsToBind[NonNullHeaps++] = HeapIter;
	}

	if (NonNullHeaps > 0)
		m_CommandList->SetDescriptorHeaps(NonNullHeaps, HeapsToBind);
}

CommandContext* ContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
{
	auto lg = std::lock_guard{ sm_ContextAllocationMutex };

	auto& AvailableContexts = sm_AvailableContexts[Type];

	CommandContext* ret = nullptr;
	if (AvailableContexts.empty())
	{
		ret = new CommandContext(Type);
		sm_ContextPool[Type].emplace_back(ret);
		ret->Initialize();
	}
	else
	{
		ret = AvailableContexts.front();
		AvailableContexts.pop();
		ret->Reset();
	}
	ASSERT(ret != nullptr);

	ASSERT(ret->m_Type == Type);

	return ret;
}

void ContextManager::FreeContext(CommandContext* UsedContext)
{
	ASSERT(UsedContext != nullptr);
	auto lg = std::lock_guard{ sm_ContextAllocationMutex };
	sm_AvailableContexts[UsedContext->m_Type].push(UsedContext);
}

void ContextManager::DestroyAllContexts()
{
	for (auto& pool : sm_ContextPool)
		pool.clear();
}

void GraphicsContext::ClearColor(ColorBuffer& Target)
{
	m_CommandList->ClearRenderTargetView(Target.GetRTV(), Target.GetClearColor().GetPtr(), 0, nullptr);
}

void GraphicsContext::ClearDepth(DepthBuffer& Target)
{
	m_CommandList->ClearDepthStencilView(Target.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, Target.GetClearDepth(), Target.GetClearStencil(), 0, nullptr);
}

void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV)
{
	m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, &DSV);
}

void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[])
{
	m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, nullptr);
}

void GraphicsContext::SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect)
{
	ASSERT(rect.left < rect.right&& rect.top < rect.bottom);
	m_CommandList->RSSetViewports(1, &vp);
	m_CommandList->RSSetScissorRects(1, &rect);
}

void GraphicsContext::SetViewport(const D3D12_VIEWPORT& vp)
{
	m_CommandList->RSSetViewports(1, &vp);
}

void GraphicsContext::SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth, FLOAT maxDepth)
{
	D3D12_VIEWPORT vp;
	vp.Width = w;
	vp.Height = h;
	vp.MinDepth = minDepth;
	vp.MaxDepth = maxDepth;
	vp.TopLeftX = x;
	vp.TopLeftY = y;
	m_CommandList->RSSetViewports(1, &vp);
}

void GraphicsContext::SetScissor(const D3D12_RECT& rect)
{
	ASSERT(rect.left < rect.right&& rect.top < rect.bottom);
	m_CommandList->RSSetScissorRects(1, &rect);
}
