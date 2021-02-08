#pragma once

#include <wrl.h>
#include <concepts>
#include <span>

class DynamicUploadBuffer
{
public:
	DynamicUploadBuffer() = default;
	virtual ~DynamicUploadBuffer() { Destroy(); }

	void Create(const std::wstring& name, size_t ByteSize);
	void Destroy();

	// Map a CPU-visible pointer to the buffer memory. You probably don't want to leace a lot of
	// memory (100s od MB) mapped this way, so you have the option of unmapping it.
	void* Map(void);
	void Unmap(void);

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(uint32_t NumVertices, uint32_t Stride, uint32_t Offset = 0) const;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView(uint32_t NumIndices, bool _32bit, uint32_t Offset = 0) const;
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const { return m_GpuVirtualAddress; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuPointer(uint32_t Offset = 0) const
	{
		return m_GpuVirtualAddress + Offset;
	}

private:
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
	}

	[[nodiscard]] constexpr T& operator[](const size_t Offset) const noexcept {
		ASSERT(Offset < m_Span.size(), "Out-of-bound access to structured upload buffer.");
		return m_Span[Offset];
	}

private:
	std::span<T> m_Span;
};