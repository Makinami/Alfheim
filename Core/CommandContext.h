#pragma once

#include "pch.h"
#include "CommandListManager.h"
#include "Color.h"
#include "PipelineState.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "DynamicUploadBuffer.h"
#include "DynamicDescriptorHeap.h"
#include "LinearAllocator.h"

class ColorBuffer;
class DepthBuffer;
//! class Texture;
class GraphicsContext;
class ComputeContext;

struct DWParam
{
	DWParam(FLOAT f) : Float(f) {}
	DWParam(UINT u) : Uint(u) {}
	DWParam(INT i) : Int(i) {}

	void operator=(FLOAT f) { Float = f; }
	void operator=(UINT u) { Uint = u; }
	void operator=(INT i) { Int = i; }

	union
	{
		FLOAT Float;
		UINT Uint;
		INT Int;
	};
};

#define VALID_COMPUTE_QUEUE_RESOURCE_STATES \
    ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS \
    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE \
    | D3D12_RESOURCE_STATE_COPY_DEST \
    | D3D12_RESOURCE_STATE_COPY_SOURCE )

class ContextManager
{
public:
	ContextManager(void) = default;

	CommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE Type);
	void FreeContext(CommandContext*);
	void DestroyAllContexts();

private:
	std::vector<std::unique_ptr<CommandContext>> sm_ContextPool[4]; //? sm_ so static?
	std::queue<CommandContext*> sm_AvailableContexts[4];
	std::mutex sm_ContextAllocationMutex;
};

struct NonCopyable
{
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};

class CommandContext : NonCopyable
{
	friend ContextManager;
private:
	CommandContext(D3D12_COMMAND_LIST_TYPE Type);

	void Reset(void);

public:
	~CommandContext(void);

	static void DestroyAllContexts(void);

	static CommandContext& Begin(const std::wstring ID = L"");

	uint64_t Finish(bool WaitForCompletion = false);

	void Initialize(void);

	GraphicsContext& GetGraphicsContext() {
		ASSERT(m_Type != D3D12_COMMAND_LIST_TYPE_COMPUTE, "Cannot convert async compute context to graphics");
		return reinterpret_cast<GraphicsContext&>(*this);
	}

	DynAlloc ReserveUploadMemory(size_t SizeInBytes)
	{
		return m_CpuLinearAllocator.Allocate(SizeInBytes);
	}

	static void InitializeTexture(GpuResource& Dest, UINT NumSubresources, D3D12_SUBRESOURCE_DATA SubData[]);
	static void InitializeBuffer(GpuResource& Dest, const void* Data, size_t NumBytes, size_t Offset = 0);

	void TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
	void InsertUAVBarrier(GpuResource& Resource, bool FlushImmediate = false);
	inline void FlushResourceBarriers(void);

	void InsertTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx);
	void ResolveTimeStamps(ID3D12Resource* pReadbackHeap, ID3D12QueryHeap* pQueryHeap, uint32_t NumQueries);
	void PIXBeginEvent(const wchar_t* label);
	void PIXEndEvent(void);
	void PixSetMerker(const wchar_t* label);

	void SetPipelineState(const PSO& PSO);
	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr);
	void SetDescriptorHeaps(UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[]);

protected:

	void BindDescriptorHeaps(void);

	CommandListManager* m_OwningManager = nullptr;
	ID3D12GraphicsCommandList* m_CommandList = nullptr;
	ID3D12CommandAllocator* m_CurrentAllocator = nullptr;

	ID3D12RootSignature* m_CurGraphicsRootSignature = nullptr;
	ID3D12PipelineState* m_CurPipelineState = nullptr;
	ID3D12RootSignature* m_CurComputeRootSignature = nullptr;

	DynamicDescriptorHeap m_DynamicViewDescriptorHeap;
	DynamicDescriptorHeap m_DynamicSamplerDescriptorHeap;

	D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[16];
	UINT m_NumBarriersToFlush = 0;

	ID3D12DescriptorHeap* m_CurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = { nullptr };

	LinearAllocator m_CpuLinearAllocator = { kCpuWritable };
	LinearAllocator m_GpuLinearAllocator = { kGpuExclusive };

	std::wstring m_ID;
	void SetID(const std::wstring& ID) { m_ID = ID; }

	D3D12_COMMAND_LIST_TYPE m_Type;
};

class GraphicsContext : public CommandContext
{
public:
	static GraphicsContext& Begin(const std::wstring& ID = L"")
	{
		return CommandContext::Begin(ID).GetGraphicsContext();
	}

	void ClearColor(ColorBuffer& Target);
	void ClearDepth(DepthBuffer& Target);

	void SetRootSignature(const RootSignature& RootSig);

	void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE mRTVs[]);
	void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE mRTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV);
	void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV) { SetRenderTargets(1, &RTV); }
	void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets(1, &RTV, DSV); }
	void SetDepthStencilTarget(D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets(0, nullptr, DSV); }

	void SetViewport(const D3D12_VIEWPORT& vp);
	void SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth = 0.0f, FLOAT maxDepth = 1.0f);
	void SetScissor(const D3D12_RECT& rect);
	void SetScissor(UINT left, UINT top, UINT right, UINT bottom);
	void SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect);
	void SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h);
	void SetStencilRef(UINT StencilRef);
	void SetBlendFactor(Color BlendFactor);
	void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology);

	void SetConstantArray(UINT RootIndex, UINT NumConstants, const void* pConstants);
	void SetConstant(UINT RootIndex, DWParam Val, UINT Offset = 0);
	void SetConstants(UINT RootIndex, DWParam X);
	void SetConstants(UINT RootIndex, DWParam X, DWParam Y);
	void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z);
	void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z, DWParam W);
	void SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData);
	void SetBufferSRV(UINT RootIndex, const GpuBuffer& SRV, UINT64 Offset = 0);
	void SetBufferSRV(UINT RootIndex, const DynamicUploadBuffer& SRV, UINT64 Offset = 0);

	void SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
	void SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);
	void SetDynamicSampler(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
	void SetDynamicSamplers(UINT RootIndex, UINT Offset, UINT Count, D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);

	void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView);
	void SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView);
	void SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[]);
	void SetDynamicVB(UINT Slot, size_t NumVertices, size_t VertexStride, const void* VBData);
	void SetDynamicIB(size_t IndexCount, const uint16_t* IBData);
	void SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData);

	void Draw(size_t VertexCount, size_t VertexStartOffset = 0);
	void DrawIndexed(size_t IndexCount, size_t StartIndexLocation = 0, size_t BaseVertexLocation = 0);
	void DrawInstanced(size_t VertexCountPerInstance, size_t InstanceCount,
		size_t StartVertexLocation = 0, size_t StartInstanceLocation = 0);
	void DrawIndexedInstanced(size_t IndexCountPerInstance, size_t InstanceCount, size_t StartIndexLocation,
		size_t BaseVertexLocation, size_t StartInstanceLocation);
};

inline void CommandContext::FlushResourceBarriers(void)
{
	if (m_NumBarriersToFlush > 0)
	{
		m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_ResourceBarrierBuffer);
		m_NumBarriersToFlush = 0;
	}
}

inline void CommandContext::SetPipelineState(const PSO& PSO)
{
	ID3D12PipelineState* PipelineState = PSO.GetPipelineStateObject();
	if (PipelineState == m_CurPipelineState)
		return;

	m_CommandList->SetPipelineState(PipelineState);
	m_CurPipelineState = PipelineState;
}

inline void CommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr)
{
	if (m_CurrentDescriptorHeaps[Type] != HeapPtr)
	{
		m_CurrentDescriptorHeaps[Type] = HeapPtr;
		BindDescriptorHeaps();
	}
}

inline void CommandContext::SetDescriptorHeaps(UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[])
{
	bool AnyChanged = false;

	for (UINT i = 0; i < HeapCount; ++i)
	{
		if (m_CurrentDescriptorHeaps[Type[i]] != HeapPtrs[i])
		{
			m_CurrentDescriptorHeaps[Type[i]] = HeapPtrs[i];
			AnyChanged = true;
		}
	}

	if (AnyChanged)
		BindDescriptorHeaps();
}

inline void GraphicsContext::SetRootSignature(const RootSignature& RootSig)
{
	if (RootSig.GetSignature() == m_CurGraphicsRootSignature)
		return;

	m_CommandList->SetGraphicsRootSignature(m_CurGraphicsRootSignature = RootSig.GetSignature());

	m_DynamicViewDescriptorHeap.ParseGraphicsRootSignature(RootSig);
	m_DynamicSamplerDescriptorHeap.ParseGraphicsRootSignature(RootSig);
}

inline void GraphicsContext::SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h)
{
	SetViewport((float)x, (float)y, (float)w, (float)h);
	SetScissor(x, y, x + w, y + h);
}

inline void GraphicsContext::SetStencilRef(UINT StencilRef)
{
	m_CommandList->OMSetStencilRef(StencilRef);
}

inline void GraphicsContext::SetBlendFactor(Color BlendFactor)
{
	m_CommandList->OMSetBlendFactor(BlendFactor.GetPtr());
}

inline void GraphicsContext::SetScissor(UINT left, UINT top, UINT right, UINT bottom)
{
	SetScissor(CD3DX12_RECT(left, top, right, bottom));
}

inline void GraphicsContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology)
{
	m_CommandList->IASetPrimitiveTopology(Topology);
}

inline void GraphicsContext::SetConstantArray(UINT RootIndex, UINT NumConstants, const void* pConstants)
{
	m_CommandList->SetGraphicsRoot32BitConstants(RootIndex, NumConstants, pConstants, 0);
}

inline void GraphicsContext::SetConstant(UINT RootIndex, DWParam Val, UINT Offset)
{
	m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Val.Uint, Offset);
}

inline void GraphicsContext::SetConstants(UINT RootEntry, DWParam X)
{
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, X.Uint, 0);
}

inline void GraphicsContext::SetConstants(UINT RootEntry, DWParam X, DWParam Y)
{
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, X.Uint, 0);
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, Y.Uint, 1);
}

inline void GraphicsContext::SetConstants(UINT RootEntry, DWParam X, DWParam Y, DWParam Z)
{
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, X.Uint, 0);
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, Y.Uint, 1);
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, Z.Uint, 2);
}

inline void GraphicsContext::SetConstants(UINT RootEntry, DWParam X, DWParam Y, DWParam Z, DWParam W)
{
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, X.Uint, 0);
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, Y.Uint, 1);
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, Z.Uint, 2);
	m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, W.Uint, 3);
}

inline void GraphicsContext::SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
	ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
	DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
	//SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
	memcpy(cb.DataPtr, BufferData, BufferSize);
	m_CommandList->SetGraphicsRootConstantBufferView(RootIndex, cb.GpuAddress);
}

inline void GraphicsContext::SetBufferSRV(UINT RootIndex, const GpuBuffer& SRV, UINT64 Offset)
{
	ASSERT((SRV.m_UsageState & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) != 0);
	m_CommandList->SetGraphicsRootShaderResourceView(RootIndex, SRV.GetGpuVirtualAddress() + Offset);
}

inline void GraphicsContext::SetBufferSRV(UINT RootIndex, const DynamicUploadBuffer& SRV, UINT64 Offset)
{
	m_CommandList->SetGraphicsRootShaderResourceView(RootIndex, SRV.GetGpuVirtualAddress() + Offset);
}

inline void GraphicsContext::SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
	SetDynamicDescriptors(RootIndex, Offset, 1, &Handle);
}

inline void GraphicsContext::SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
	m_DynamicViewDescriptorHeap.SetGraphicsDescriptorHandles(RootIndex, Offset, Count, Handles);
}

inline void GraphicsContext::SetDynamicSampler(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
	SetDynamicSamplers(RootIndex, Offset, 1, &Handle);
}

inline void GraphicsContext::SetDynamicSamplers(UINT RootIndex, UINT Offset, UINT Count, D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
	m_DynamicSamplerDescriptorHeap.SetGraphicsDescriptorHandles(RootIndex, Offset, Count, Handles);
}

inline void GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView)
{
	m_CommandList->IASetIndexBuffer(&IBView);
}

inline void GraphicsContext::SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView)
{
	SetVertexBuffers(Slot, 1, &VBView);
}

inline void GraphicsContext::SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[])
{
	m_CommandList->IASetVertexBuffers(StartSlot, Count, VBViews);
}

inline void GraphicsContext::SetDynamicVB(UINT Slot, size_t NumVertices, size_t VertexStride, const void* VBData)
{
	ASSERT(VBData != nullptr && Math::IsAligned(VBData, 16));

	size_t BufferSize = Math::AlignUp(NumVertices * VertexStride, 16);
	DynAlloc vb = m_CpuLinearAllocator.Allocate(BufferSize);

	SIMDMemCopy(vb.DataPtr, VBData, BufferSize >> 4);

	D3D12_VERTEX_BUFFER_VIEW VBView;
	VBView.BufferLocation = vb.GpuAddress;
	VBView.SizeInBytes = (UINT)BufferSize;
	VBView.StrideInBytes = (UINT)VertexStride;

	m_CommandList->IASetVertexBuffers(Slot, 1, &VBView);
}

inline void GraphicsContext::SetDynamicIB(size_t IndexCount, const uint16_t* IndexData)
{
	ASSERT(IndexData != nullptr && Math::IsAligned(IndexData, 16));

	const auto BufferSize = Math::AlignUp(IndexCount * sizeof(uint16_t), 16);
	const auto ib = m_CpuLinearAllocator.Allocate(BufferSize);

	SIMDMemCopy(ib.DataPtr, IndexData, BufferSize >> 4);

	const auto IBView = D3D12_INDEX_BUFFER_VIEW{
		.BufferLocation = ib.GpuAddress,
		.SizeInBytes = (UINT)(IndexCount * sizeof(uint16_t)),
		.Format = DXGI_FORMAT_R16_UINT
	};

	m_CommandList->IASetIndexBuffer(&IBView);
}

inline void GraphicsContext::SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
	ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
	DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
	SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
	m_CommandList->SetGraphicsRootShaderResourceView(RootIndex, cb.GpuAddress);
}

inline void GraphicsContext::Draw(size_t VertexCount, size_t VertexStartOffset)
{
	DrawInstanced(VertexCount, 1, VertexStartOffset, 0);
}

inline void GraphicsContext::DrawIndexed(size_t IndexCount, size_t StartIndexLocation, size_t BaseVertexLocation)
{
	DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
}

inline void GraphicsContext::DrawInstanced(size_t VertexCountPerInstance, size_t InstanceCount, size_t StartVertexLocation, size_t StartInstanceLocation)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
	m_DynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
	m_CommandList->DrawInstanced((UINT)VertexCountPerInstance, (UINT)InstanceCount, (UINT)StartVertexLocation, (UINT)StartInstanceLocation);
}

inline void GraphicsContext::DrawIndexedInstanced(size_t IndexCountPerInstance, size_t InstanceCount, size_t StartIndexLocation, size_t BaseVertexLocation, size_t StartInstanceLocation)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
	m_DynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
	m_CommandList->DrawIndexedInstanced((UINT)IndexCountPerInstance, (UINT)InstanceCount, (UINT)StartIndexLocation, (INT)BaseVertexLocation, (UINT)StartInstanceLocation);
}

inline void CommandContext::InsertTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx)
{
	m_CommandList->EndQuery(pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, QueryIdx);
}

inline void CommandContext::ResolveTimeStamps(ID3D12Resource* pReadbackHeap, ID3D12QueryHeap* pQueryHeap, uint32_t NumQueries)
{
	m_CommandList->ResolveQueryData(pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, NumQueries, pReadbackHeap, 0);
}
