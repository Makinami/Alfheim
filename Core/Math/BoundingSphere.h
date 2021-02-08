#pragma once

#include "../VectorMath.h"

namespace Math
{
	class BoundingSphere
	{
	public:
		BoundingSphere() = default;
		BoundingSphere(Vector3 center, Scalar radius) : m_repr(center, radius) {}
		explicit BoundingSphere(Vector4 sphere) : m_repr{sphere} {}

		auto GetCenter(void) const { return Vector3{ m_repr }; }
		auto GetRadius(void) const { return m_repr.GetW(); }

	private:
		Vector4 m_repr;
	};
}