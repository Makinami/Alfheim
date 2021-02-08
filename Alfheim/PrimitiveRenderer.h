#pragma once
#include "pch.h"

#include <span>
#include <array>

#include "GpuBuffer.h"
#include "DynamicUploadBuffer.h"
#include "MeshBufferPacker.h"

namespace Math
{
	class Camera;
}


class PrimitiveRenderer
{
	struct InstanceData
	{
		DirectX::XMFLOAT4X4 WorldTransformation;
	};

	enum class PrimitiveType
	{
		kSphere,
		kCuboid,
		kPlane
	};
	static constexpr int sm_PrimitiveTypesCount = 3;

public:
	void Initialize();
	void Shutdown() {};

	void QueueSphere(const Math::Vector3& Position, const float Radius)
	{
		QueuePrimitive(PrimitiveType::kSphere, DirectX::XMMatrixScaling(Radius, Radius, Radius) * DirectX::XMMatrixTranslationFromVector(Position));
	}
	void QueueCuboid(const Math::Vector3& Min, const Math::Vector3& Max)
	{
		QueuePrimitive(PrimitiveType::kCuboid, DirectX::XMMatrixTranslationFromVector(Min) * DirectX::XMMatrixScalingFromVector(Max - Min));
	}
	void QueuePlane(const Math::Vector3& Origin, const Math::Vector3& UAxis, const Math::Vector3& VAxis)
	{
		static const auto BaseInverse = Math::Matrix4(DirectX::XMMatrixInverse(nullptr, Math::Matrix4(Math::Vector4(0.f, 0.f, 0.f, 1.f), Math::Vector4(1, 0, 0, 1), Math::Vector4(0, 1, 0, 1), Math::Vector4(0, 0, 1, 1))));

		const auto Transformation = Math::Matrix4(Origin, Origin + UAxis, Origin + VAxis, Math::Vector3(Math::kZero)) * BaseInverse;
		QueuePrimitive(PrimitiveType::kPlane, Transformation);
	}
	void Render(GraphicsContext& gfxContext, const Math::Camera& camera);

private:
	void QueuePrimitive(PrimitiveType Type, const DirectX::XMFLOAT4X4& WorldMatrix)
	{
		m_PrimitiveQueues[static_cast<int>(Type)].emplace_back(WorldMatrix);
	}
	void QueuePrimitive(PrimitiveType Type, DirectX::CXMMATRIX WorldMatrix)
	{
		DirectX::XMFLOAT4X4 Float4x4;
		XMStoreFloat4x4(&Float4x4, WorldMatrix);
		QueuePrimitive(Type, Float4x4);
	}


	RootSignature m_RootSig;
	GraphicsPSO m_WireframePSO;
	GraphicsPSO m_SurfacePSO;
	GraphicsPSO m_LinePSO;

	StructuredBuffer m_VertexBuffer;
	ByteAddressBuffer m_IndexBuffer;
	
	std::array<Foo, sm_PrimitiveTypesCount> m_Foos;

	std::array<std::vector<InstanceData>, sm_PrimitiveTypesCount> m_PrimitiveQueues;

	StructuredUploadBuffer<InstanceData> m_InstanceBuffer;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
};

