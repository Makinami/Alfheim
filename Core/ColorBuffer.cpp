#include "pch.h"
#include "ColorBuffer.h"
#include "GraphicsCore.h"

using namespace Graphics;

void ColorBuffer::Init(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
{
	NumMips = (NumMips == 0 ? ComputeNumMips(Width, Height) : NumMips);
	D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
	D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 1, NumMips, Format, Flags);

	ResourceDesc.SampleDesc.Count = m_FragmentCount;
	ResourceDesc.SampleDesc.Quality = 0;

	D3D12_CLEAR_VALUE ClearValue = {};
	ClearValue.Format = Format;
	ClearValue.Color[0] = m_ClearColor.R();
	ClearValue.Color[1] = m_ClearColor.G();
	ClearValue.Color[2] = m_ClearColor.B();
	ClearValue.Color[3] = m_ClearColor.A();

	CreateTextureResource(Graphics::g_Device, Name, ResourceDesc, ClearValue, VidMemPtr);
	CreateDerivedViews(Graphics::g_Device, Format, 1, NumMips);
}

void ColorBuffer::Init3D(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t Depth,
	DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
{
	D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
	D3D12_RESOURCE_DESC ResourceDesc = DescribeTex3D(Width, Height, Depth, Format, Flags);

	ResourceDesc.SampleDesc.Count = m_FragmentCount;
	ResourceDesc.SampleDesc.Quality = 0;

	D3D12_CLEAR_VALUE ClearValue = {};
	ClearValue.Format = Format;
	ClearValue.Color[0] = m_ClearColor.R();
	ClearValue.Color[1] = m_ClearColor.G();
	ClearValue.Color[2] = m_ClearColor.B();
	ClearValue.Color[3] = m_ClearColor.A();

	CreateTextureResource(Graphics::g_Device, Name, ResourceDesc, ClearValue, VidMemPtr);
	CreateDerivedViews(Graphics::g_Device, ResourceDesc);
}

void ColorBuffer::InitArray(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t ArrayCount, DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
{
	D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
	D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, ArrayCount, 1, Format, Flags);

	D3D12_CLEAR_VALUE ClearValue = {};
	ClearValue.Format = Format;
	ClearValue.Color[0] = m_ClearColor.R();
	ClearValue.Color[1] = m_ClearColor.G();
	ClearValue.Color[2] = m_ClearColor.B();
	ClearValue.Color[3] = m_ClearColor.A();

	CreateTextureResource(Graphics::g_Device, Name, ResourceDesc, ClearValue, VidMemPtr);
	CreateDerivedViews(Graphics::g_Device, Format, ArrayCount, 1);
}

void ColorBuffer::InitFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource)
{
	AssociateWithResource(Graphics::g_Device, Name, BaseResource, D3D12_RESOURCE_STATE_PRESENT);

	//m_UAVHandle[0] = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//Graphics::g_Device->CreateUnorderedAccessView(m_pResource.Get(), nullptr, nullptr, m_UAVHandle[0]);

	m_RTVHandle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	Graphics::g_Device->CreateRenderTargetView(m_pResource.Get(), nullptr, m_RTVHandle);
}

void ColorBuffer::CreateDerivedViews(ID3D12Device* Device, const D3D12_RESOURCE_DESC& desc)
{
	ASSERT(desc.DepthOrArraySize == 1 || desc.MipLevels == 1, "We don't support auto-mips on texture arrays");

	m_NumMipMaps = desc.MipLevels - 1;

	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

	RTVDesc.Format = desc.Format;
	UAVDesc.Format = GetUAVFormat(desc.Format);
	SRVDesc.Format = desc.Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	switch (desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (desc.DepthOrArraySize > 1)
		{
			RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			RTVDesc.Texture2DArray.MipSlice = 0;
			RTVDesc.Texture2DArray.FirstArraySlice = 0;
			RTVDesc.Texture2DArray.ArraySize = (UINT)desc.DepthOrArraySize;

			UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			UAVDesc.Texture2DArray.MipSlice = 0;
			UAVDesc.Texture2DArray.FirstArraySlice = 0;
			UAVDesc.Texture2DArray.ArraySize = (UINT)desc.DepthOrArraySize;

			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			SRVDesc.Texture2DArray.MipLevels = desc.MipLevels;
			SRVDesc.Texture2DArray.MostDetailedMip = 0;
			SRVDesc.Texture2DArray.FirstArraySlice = 0;
			SRVDesc.Texture2DArray.ArraySize = (UINT)desc.DepthOrArraySize;
		}
		else if (m_FragmentCount > 1)
		{
			RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			RTVDesc.Texture2D.MipSlice = 0;

			UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			UAVDesc.Texture2D.MipSlice = 0;

			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MipLevels = desc.MipLevels;
			SRVDesc.Texture2D.MostDetailedMip = 0;
		}
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		RTVDesc.Texture3D.MipSlice = 0;
		RTVDesc.Texture3D.FirstWSlice = 0;
		RTVDesc.Texture3D.WSize = desc.DepthOrArraySize;

		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		UAVDesc.Texture3D.MipSlice = 0;
		UAVDesc.Texture3D.FirstWSlice = 0;
		UAVDesc.Texture3D.WSize = desc.DepthOrArraySize;

		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		SRVDesc.Texture3D.MostDetailedMip = 0;
		SRVDesc.Texture3D.MipLevels = desc.MipLevels;
		break;
	default:
		ERROR("Unsupported resource dimension {}", desc.Dimension);
	}

	if (m_SRVHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
	{
		m_RTVHandle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_SRVHandle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	ID3D12Resource* Resource = m_pResource.Get();

	// Create the render targer view
	Device->CreateRenderTargetView(Resource, &RTVDesc, m_RTVHandle);

	// Create the shder resource view
	Device->CreateShaderResourceView(Resource, &SRVDesc, m_SRVHandle);

	if (m_FragmentCount > 1)
		return;

	// Create the UAVs for each mip level (RWTexture2D)
	for (uint32_t i = 0; i < desc.MipLevels; ++i)
	{
		if (m_UAVHandle[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_UAVHandle[i] = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		Device->CreateUnorderedAccessView(Resource, NULL, &UAVDesc, m_UAVHandle[i]);

		switch (desc.Dimension)
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			UAVDesc.Texture2D.MipSlice++;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			UAVDesc.Texture3D.MipSlice++;
			break;
		}
	}
}

void ColorBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips)
{
	ASSERT(ArraySize == 1 || NumMips == 1, "We don't support auto-mips on texture arrays");

	m_NumMipMaps = NumMips - 1;

	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

	RTVDesc.Format = Format;
	UAVDesc.Format = GetUAVFormat(Format);
	SRVDesc.Format = Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (ArraySize > 1)
	{
		RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		RTVDesc.Texture2DArray.MipSlice = 0;
		RTVDesc.Texture2DArray.FirstArraySlice = 0;
		RTVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;

		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		UAVDesc.Texture2DArray.MipSlice = 0;
		UAVDesc.Texture2DArray.FirstArraySlice = 0;
		UAVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;

		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		SRVDesc.Texture2DArray.MipLevels = NumMips;
		SRVDesc.Texture2DArray.MostDetailedMip = 0;
		SRVDesc.Texture2DArray.FirstArraySlice = 0;
		SRVDesc.Texture2DArray.ArraySize = (UINT)ArraySize;
	}
	else if (m_FragmentCount > 1)
	{
		RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	}
	else
	{
		RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		RTVDesc.Texture2D.MipSlice = 0;

		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		UAVDesc.Texture2D.MipSlice = 0;
		
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = NumMips;
		SRVDesc.Texture2D.MostDetailedMip = 0;
	}

	if (m_SRVHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
	{
		m_RTVHandle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_SRVHandle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	ID3D12Resource* Resource = m_pResource.Get();

	// Create the render targer view
	Device->CreateRenderTargetView(Resource, &RTVDesc, m_RTVHandle);

	// Create the shder resource view
	Device->CreateShaderResourceView(Resource, &SRVDesc, m_SRVHandle);

	if (m_FragmentCount > 1)
		return;

	// Create the UAVs for each mip level (RWTexture2D)
	for (uint32_t i = 0; i < NumMips; ++i)
	{
		if (m_UAVHandle[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_UAVHandle[i] = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		Device->CreateUnorderedAccessView(Resource, NULL, &UAVDesc, m_UAVHandle[i]);

		UAVDesc.Texture2D.MipSlice++;
	}
}
