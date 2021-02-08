#include "pch.h"
#include "GpuBuffer.h"

#include "GraphicsCore.h"

using namespace Graphics;

void GpuBuffer::Create([[maybe_unused]] const std::wstring& name, size_t NumElements, size_t ElementSize, const void* initialData)
{
	ASSERT(NumElements <= std::numeric_limits<UINT>::max(), "Buffer of over UINT elements not supported by DirectX");
	ASSERT(ElementSize <= std::numeric_limits<UINT>::max(), "Buffer elements of over UINT in size not supported by DirectX");

	Destroy();

	m_ElementCount = NumElements;
	m_ElementSize = ElementSize;
	m_BufferSize = NumElements * ElementSize;

	const auto ResourceDesc = DescribeBuffer();

	const auto HeapProps = D3D12_HEAP_PROPERTIES{
		.Type = D3D12_HEAP_TYPE_DEFAULT,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask = 1,
		.VisibleNodeMask = 1
	};

	ASSERT_SUCCEEDED(g_Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
		&ResourceDesc, m_UsageState, nullptr, IID_PPV_ARGS(&m_pResource)));

	m_GpuVirtualAddress = m_pResource->GetGPUVirtualAddress();

	if (initialData)
		CommandContext::InitializeBuffer(*this, initialData, m_BufferSize);

#ifndef RELEASE
	m_pResource->SetName(name.c_str());
#endif

	CreateDerivedViews();
}

auto GpuBuffer::CreateConstantBufferView(size_t Offset, size_t Size) const -> D3D12_CPU_DESCRIPTOR_HANDLE
{
	ASSERT(Offset + Size <= m_BufferSize);

	Size = Math::AlignUp(Size, 16);

	auto CBVDesc = D3D12_CONSTANT_BUFFER_VIEW_DESC{
		.BufferLocation = m_GpuVirtualAddress + Offset,
		.SizeInBytes = (UINT)Size
	};

	auto hCBV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateConstantBufferView(&CBVDesc, hCBV);
	return hCBV;
}

auto GpuBuffer::DescribeBuffer(void) const -> D3D12_RESOURCE_DESC
{
	ASSERT(m_BufferSize != 0);

	return D3D12_RESOURCE_DESC{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = 0,
		.Width = m_BufferSize,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = {1, 0},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = m_ResourceFlags
	};
}

void ByteAddressBuffer::CreateDerivedViews(void)
{
	const auto SRVDesc = D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format = DXGI_FORMAT_R32_TYPELESS,
		.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer = {
			.NumElements = static_cast<UINT>(m_BufferSize / 4),
			.Flags = D3D12_BUFFER_SRV_FLAG_RAW
		}
	};

	if (m_SRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_SRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(m_pResource.Get(), &SRVDesc, m_SRV);

	const auto UAVDesc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
		.Format = DXGI_FORMAT_R32_TYPELESS,
		.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
		.Buffer = {
			.NumElements = static_cast<UINT>(m_BufferSize / 4),
			.Flags = D3D12_BUFFER_UAV_FLAG_RAW,
			}
	};

	if (m_UAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_UAV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateUnorderedAccessView(m_pResource.Get(), nullptr, &UAVDesc, m_UAV);
}

void StructuredBuffer::CreateDerivedViews(void)
{
	const auto SRVDesc = D3D12_SHADER_RESOURCE_VIEW_DESC{
		.Format = DXGI_FORMAT_UNKNOWN,
		.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Buffer = {
			.NumElements = (UINT)m_ElementCount,
			.StructureByteStride = (UINT)m_ElementSize,
			.Flags = D3D12_BUFFER_SRV_FLAG_NONE
		}
	};

	if (m_SRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_SRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateShaderResourceView(m_pResource.Get(), &SRVDesc, m_SRV);

	const auto UAVDesc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
		.Format = DXGI_FORMAT_UNKNOWN,
		.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
		.Buffer = {
			.NumElements = (UINT)m_ElementCount,
			.StructureByteStride = (UINT)m_ElementSize,
			.Flags = D3D12_BUFFER_UAV_FLAG_NONE,
		}
	};

	m_CounterBuffer.Create(L"StructuredBuffer::Counter", 1, 4);

	if (m_UAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_UAV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_Device->CreateUnorderedAccessView(m_pResource.Get(), m_CounterBuffer.GetResource(), &UAVDesc, m_UAV);
}

auto StructuredBuffer::GetCounterSRV(CommandContext& Context) -> const D3D12_CPU_DESCRIPTOR_HANDLE&
{
	Context.TransitionResource(m_CounterBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
	return m_CounterBuffer.GetSRV();
}

auto StructuredBuffer::GetCounterUAV(CommandContext& Context) -> const D3D12_CPU_DESCRIPTOR_HANDLE&
{
	Context.TransitionResource(m_CounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	return m_CounterBuffer.GetUAV();
}
