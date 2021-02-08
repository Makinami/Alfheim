#pragma once

#include "pch.h"
#include "GpuResource.h"
#include "Utility.h"

class Texture : public GpuResource
{
	friend class CommandContext;

public:
	Texture() = default;
	Texture(D3D12_CPU_DESCRIPTOR_HANDLE Handle) : m_hCpuDescriptorHandle(Handle) {}

	void Create(size_t Pitch, size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitData);
	void Create(size_t Width, size_t Height, DXGI_FORMAT Format, const void* InitData)
	{
		Create(Width, Width, Height, Format, InitData);
	}

	virtual void Destroy() override
	{
		GpuResource::Destroy();
		//? This leaks descriptor handles. We should really give it back to be reused
		m_hCpuDescriptorHandle.ptr = 0;
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() const { return m_hCpuDescriptorHandle; }

	bool operator!() { return m_hCpuDescriptorHandle.ptr == 0; }

protected:
	D3D12_CPU_DESCRIPTOR_HANDLE m_hCpuDescriptorHandle = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
};