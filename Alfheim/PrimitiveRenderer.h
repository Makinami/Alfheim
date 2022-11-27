#pragma once
#include "pch.h"

#include <span>
#include <array>

#include "GpuBuffer.h"
#include "DynamicUploadBuffer.h"
#include "MeshBufferPacker.h"
#include "Color.h"
#include "Math/Frustum.h"

namespace Math
{
	class Camera;
}

class PrimitiveRenderer
{
	struct InstanceData
	{
		DirectX::XMFLOAT4 Colour;
		DirectX::XMFLOAT4X4 WorldTransformation;
	};

	enum class PrimitiveType
	{
		kSphere,
		kCuboid,
		kPlane,
		kLine,

		kCount,
	};

public:
	void Initialize();
	void Shutdown() {};

	void QueueSphere(const Math::Vector3& Position, const float Radius, const Color Color = Color{})
	{
		QueuePrimitive(PrimitiveType::kSphere, DirectX::XMMatrixScaling(Radius, Radius, Radius) * DirectX::XMMatrixTranslationFromVector(Position), Color);
	}
	void QueueCuboid(const Math::Vector3& Min, const Math::Vector3& Max, const Color Color = Color{})
	{
		QueuePrimitive(PrimitiveType::kCuboid, DirectX::XMMatrixScalingFromVector(Max - Min) * DirectX::XMMatrixTranslationFromVector(Min), Color);
	}
	void QueuePlane(const Math::Vector3& Origin, const Math::Vector3& UAxis, const Math::Vector3& VAxis, const Color Color = Color{})
	{
		static const auto BaseInverse = Math::Matrix4(DirectX::XMMatrixInverse(nullptr, Math::Matrix4(Math::Vector4(0.f, 0.f, 0.f, 1.f), Math::Vector4(1, 0, 0, 1), Math::Vector4(0, 1, 0, 1), Math::Vector4(0, 0, 1, 1))));

		const auto Transformation = Math::Matrix4(Origin, Origin + UAxis, Origin + VAxis, Math::Vector3(Math::kZero)) * BaseInverse;
		QueuePrimitive(PrimitiveType::kPlane, Transformation, Color);
	}
	void QueueFrustum(const Math::Frustum& Frustum, const Color Color = Color{})
	{
		using enum Math::Frustum::CornerID;

		QueueLine(Frustum.GetFrustumCorner(kNearLowerLeft), Frustum.GetFrustumCorner(kNearUpperLeft), Color);
		QueueLine(Frustum.GetFrustumCorner(kNearLowerLeft), Frustum.GetFrustumCorner(kNearLowerRight), Color);
		QueueLine(Frustum.GetFrustumCorner(kNearUpperLeft), Frustum.GetFrustumCorner(kNearUpperRight), Color);
		QueueLine(Frustum.GetFrustumCorner(kNearLowerRight), Frustum.GetFrustumCorner(kNearUpperRight), Color);

		QueueLine(Frustum.GetFrustumCorner(kFarLowerLeft), Frustum.GetFrustumCorner(kFarUpperLeft), Color);
		QueueLine(Frustum.GetFrustumCorner(kFarLowerLeft), Frustum.GetFrustumCorner(kFarLowerRight), Color);
		QueueLine(Frustum.GetFrustumCorner(kFarUpperLeft), Frustum.GetFrustumCorner(kFarUpperRight), Color);
		QueueLine(Frustum.GetFrustumCorner(kFarLowerRight), Frustum.GetFrustumCorner(kFarUpperRight), Color);

		QueueLine(Frustum.GetFrustumCorner(kNearLowerLeft), Frustum.GetFrustumCorner(kFarLowerLeft), Color);
		QueueLine(Frustum.GetFrustumCorner(kNearLowerRight), Frustum.GetFrustumCorner(kFarLowerRight), Color);
		QueueLine(Frustum.GetFrustumCorner(kNearUpperLeft), Frustum.GetFrustumCorner(kFarUpperLeft), Color);
		QueueLine(Frustum.GetFrustumCorner(kNearUpperRight), Frustum.GetFrustumCorner(kFarUpperRight), Color);
	}
	void QueueLine(const Math::Vector3& From, const Math::Vector3 To, const Color Color = Color{})
	{
		const auto vector = To - From;
		const auto transformation = Math::AffineTransform::MakeTranslation(From) * Math::AffineTransform::MakeScale(vector);
		QueuePrimitive(PrimitiveType::kLine, transformation, Color);
	}

	void Render(GraphicsContext& gfxContext, const Math::Camera& camera);

	void Reset()
	{
		std::ranges::for_each(m_PrimitiveQueues, [](auto& data) { data.clear(); });
	}

private:
	void QueuePrimitive(PrimitiveType Type, const DirectX::XMFLOAT4X4& WorldMatrix, const DirectX::XMFLOAT4 Color)
	{
		m_PrimitiveQueues[static_cast<int>(Type)].emplace_back(Color, WorldMatrix);
	}
	void QueuePrimitive(PrimitiveType Type, DirectX::CXMMATRIX WorldMatrix, const Color Color)
	{
		DirectX::XMFLOAT4X4 Float4x4;
		XMStoreFloat4x4(&Float4x4, WorldMatrix);
		DirectX::XMFLOAT4 Float4;
		XMStoreFloat4(&Float4, Color);
		QueuePrimitive(Type, Float4x4, Float4);
	}

	RootSignature m_RootSig;
	GraphicsPSO m_WireframePSO;

	StructuredBuffer m_VertexBuffer;
	ByteAddressBuffer m_IndexBuffer;
	
	std::array<Foo, static_cast<size_t>(PrimitiveType::kCount)> m_PrimitivesIndices;

	std::array<std::vector<InstanceData>, static_cast<size_t>(PrimitiveType::kCount)> m_PrimitiveQueues;

	StructuredUploadBuffer<InstanceData> m_InstanceBuffer;
};

