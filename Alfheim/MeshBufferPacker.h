#pragma once

#include "GpuBuffer.h"

struct Foo
{
	int IndexCount;
	int StartIndexLocation;
	int BaseVertexLocation;
};

template <typename VertexDataType, typename IndexDataType = uint32_t,
	typename = std::enable_if_t<std::is_same_v<IndexDataType, uint32_t> || std::is_same_v<IndexDataType, uint16_t>>>
class MeshBufferPacker
{
public:
	template <typename VDT, typename IDT, typename = std::enable_if_t<std::is_integral_v<IDT> || std::is_convertible_v<VDT, VertexDataType>>>
	auto Push(const std::vector<VDT>& Vertices, const std::vector<IDT>& Indices) -> Foo
	{
		auto foo = Foo{
			.IndexCount = static_cast<int>(Indices.size()),
			.StartIndexLocation = static_cast<int>(m_Indices.size())
		};

		m_Indices.insert(m_Indices.end(), Indices.begin(), Indices.end());
		std::for_each(m_Indices.end() - Indices.size(), m_Indices.end(), [base = m_Vertices.size()](auto& index) {
			index += static_cast<int>(base);
		});

		m_Vertices.insert(m_Vertices.end(), Vertices.begin(), Vertices.end());

		return foo;
	}
	
	template <typename VDT, typename IDT, typename = std::enable_if_t<std::is_integral_v<IDT> || std::is_convertible_v<VDT, VertexDataType>>>
	auto Push(const std::pair<std::vector<VDT>, std::vector<IDT>>& MeshData)
	{
		return Push(MeshData.first, MeshData.second);
	}

	auto Finalize(const std::wstring name)
	{
		StructuredBuffer VertexBuffer;
		ByteAddressBuffer IndexBuffer;

		Finalize(name, VertexBuffer, IndexBuffer);

		return  std::tuple{ std::move(VertexBuffer), std::move(IndexBuffer) };
	}
	
	void Finalize(const std::wstring name, GpuBuffer& VertexBuffer, GpuBuffer& IndexBuffer)
	{
		VertexBuffer.Create(name + L" vertex buffer", std::size(m_Vertices), sizeof(m_Vertices[0]), m_Vertices.data());
		IndexBuffer.Create(name + L" index buffer", std::size(m_Indices), sizeof(m_Indices[0]), m_Indices.data());

		Reset();
	}

	void Reset() { m_Vertices.clear(); m_Indices.clear(); }

private:
	std::vector<VertexDataType> m_Vertices;
	std::vector<IndexDataType> m_Indices;
};

