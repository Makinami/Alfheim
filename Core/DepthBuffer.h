#pragma once

#include "PixelBuffer.h"

class DepthBuffer : public PixelBuffer
{
public:
	DepthBuffer(float ClearDepth = 0.0f, uint8_t ClearStencil = 0)
		: m_ClearDepth(ClearDepth), m_ClearStencil(ClearStencil)
	{}

	// Create a depth buffer.  If an address is supplied, memory will not be allocated.
	void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format,
		D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

	auto& GetDSV() const { return m_hDSV[0]; }
	auto& GetDSV_DepthReadOnly() const { return m_hDSV[1]; }
	auto& GetDSV_StencilRealOnly() const { return m_hDSV[2]; }
	auto& GetDSV_ReadOnly() const { return m_hDSV[3]; }
	auto& GetDepthSRV() const { return m_hDepthSRV; }
	auto& GetStencilSRV() const { return m_hStencilSRV; }

	auto GetClearDepth() const { return m_ClearDepth; }
	auto GetClearStencil() const { return m_ClearStencil; }

private:
	void CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format);

	float m_ClearDepth;
	uint8_t m_ClearStencil;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hDSV[4] = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
	D3D12_CPU_DESCRIPTOR_HANDLE m_hDepthSRV = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
	D3D12_CPU_DESCRIPTOR_HANDLE m_hStencilSRV = { D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN };
};

