#pragma once

#include "pch.h"
#include "GpuResource.h"

class CommandContext;

class GpuBuffer : public GpuResource
{
public:
	virtual ~GpuBuffer() { Destroy(); }

	GpuBuffer(GpuBuffer&&) = default;
	GpuBuffer& operator=(GpuBuffer&&) = default;

	// Create a buffer. If inital data is provided, it will be copied into the buffer using the default command context.
	void Create(const std::wstring& name, size_t NumElements, size_t ElementSize, const void* initialData = nullptr);

	auto& GetUAV(void) const noexcept { return m_UAV; }
	auto& GetSRV(void) const noexcept { return m_SRV; }

	auto RootConstantBufferView(void) const noexcept { return m_GpuVirtualAddress; }

	auto CreateConstantBufferView(size_t Offset, size_t Size) const ->D3D12_CPU_DESCRIPTOR_HANDLE;

	auto VertexBufferView(size_t Offset, size_t Size, size_t Stride) const -> D3D12_VERTEX_BUFFER_VIEW;
	auto VertexBufferView(size_t BaseVertexIndex = 0) const
	{
		const size_t Offset = BaseVertexIndex * m_ElementSize;
		return VertexBufferView(Offset, m_BufferSize - Offset, m_ElementSize);
	}

	auto IndexBufferView(size_t Offset, size_t Size, bool b32Bit = false) const -> D3D12_INDEX_BUFFER_VIEW;
	auto IndexBufferView(size_t StartIndex = 0) const
	{
		const auto Offset = StartIndex * m_ElementSize;
		return IndexBufferView(Offset, m_BufferSize - Offset, m_ElementSize == 4);
	}

	auto GetBufferSize() const noexcept { return m_BufferSize; }
	auto GetElementCount() const noexcept { return m_ElementCount; }
	auto GetElementSize() const noexcept { return m_ElementSize; }

protected:
	GpuBuffer(void) = default;

	auto DescribeBuffer(void) const -> D3D12_RESOURCE_DESC;
	virtual void CreateDerivedViews(void) = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE m_UAV = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
	D3D12_CPU_DESCRIPTOR_HANDLE m_SRV = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };

	size_t m_BufferSize = 0;
	size_t m_ElementCount = 0;
	size_t m_ElementSize = 0;
	D3D12_RESOURCE_FLAGS m_ResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
};

inline auto GpuBuffer::VertexBufferView(size_t Offset, size_t Size, size_t Stride) const -> D3D12_VERTEX_BUFFER_VIEW
{
	return {
		.BufferLocation = m_GpuVirtualAddress + Offset,
		.SizeInBytes = (UINT)Size,
		.StrideInBytes = (UINT)Stride
	};
}

inline auto GpuBuffer::IndexBufferView(size_t Offset, size_t Size, bool b32Bit) const -> D3D12_INDEX_BUFFER_VIEW
{
	return {
		.BufferLocation = m_GpuVirtualAddress + Offset,
		.SizeInBytes = (UINT)Size,
		.Format = b32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT
	};
}

class ByteAddressBuffer : public GpuBuffer
{
public:
	virtual void CreateDerivedViews(void) override;
};

class InderectArgsBuffer : public ByteAddressBuffer
{
public:
	InderectArgsBuffer() = default;
};

class StructuredBuffer : public GpuBuffer
{
public:
	void Destroy(void) override
	{
		m_CounterBuffer.Destroy();
		GpuBuffer::Destroy();
	}

	void CreateDerivedViews(void) override;

	auto& GetCounterBuffer(void) noexcept { return m_CounterBuffer; }

	auto GetCounterSRV(CommandContext& Context) -> const D3D12_CPU_DESCRIPTOR_HANDLE&;
	auto GetCounterUAV(CommandContext& Context) -> const D3D12_CPU_DESCRIPTOR_HANDLE&;

private:
	ByteAddressBuffer m_CounterBuffer;
};