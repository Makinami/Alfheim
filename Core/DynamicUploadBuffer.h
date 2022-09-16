#pragma once

#include <wrl.h>
#include <concepts>
#include <span>

#include "GraphicsCore.h"

class DynamicUploadBuffer
{
public:
	DynamicUploadBuffer() = default;
	virtual ~DynamicUploadBuffer() { Destroy(); }

	void Create(const std::wstring& name, size_t ByteSize);
	void Destroy();

	// Map a CPU-visible pointer to the buffer memory. You probably don't want to leace a lot of
	// memory (100s of MB) mapped this way, so you have the option of unmapping it.
	void* Map(void);
	void Unmap(void);

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(uint32_t NumVertices, uint32_t Stride, uint32_t Offset = 0) const;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView(uint32_t NumIndices, bool _32bit, uint32_t Offset = 0) const;
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const { return m_GpuVirtualAddress; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuPointer(uint32_t Offset = 0) const
	{
		return m_GpuVirtualAddress + Offset;
	}

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
	D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
	void* m_CpuVirtualAddress;
};

template <typename T>
class StructuredUploadBuffer : public DynamicUploadBuffer
{
public:
	void Create(const std::wstring& name, size_t ElementCount)
	{
		DynamicUploadBuffer::Create(name, ElementCount * sizeof(T));

		m_Span = std::span<T>(static_cast<T*>(Map()), ElementCount);

		const auto SRVDesc = D3D12_SHADER_RESOURCE_VIEW_DESC{
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Buffer = {
				.NumElements = (UINT)ElementCount,
				.StructureByteStride = sizeof(T),
				.Flags = D3D12_BUFFER_SRV_FLAG_NONE
			}
		};

		if (m_SRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_SRV = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		Graphics::g_Device->CreateShaderResourceView(m_pResource.Get(), &SRVDesc, m_SRV);
	}

	[[nodiscard]] constexpr T& operator[](const size_t Offset) const noexcept {
		ASSERT(Offset < m_Span.size(), "Out-of-bound access to structured upload buffer.");
		return m_Span[Offset];
	}

	auto& GetSRV(void) const noexcept { return m_SRV; }

private:
	std::span<T> m_Span;

	D3D12_CPU_DESCRIPTOR_HANDLE m_SRV = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
};