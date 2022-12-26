#pragma once

#include "Transform.h"

namespace Math
{
	class AxisAlignedBox
	{
	public:
		AxisAlignedBox() : m_min{ FLT_MAX, FLT_MAX, FLT_MAX }, m_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX } {}
		AxisAlignedBox(EZeroTag) : m_min{ FLT_MAX, FLT_MAX, FLT_MAX }, m_max{ -FLT_MAX, -FLT_MAX, -FLT_MAX } {}
		AxisAlignedBox(Vector3 min, Vector3 max) : m_min(min), m_max(max) {}

		void AddPoint(Vector3 point)
		{
			m_min = Min(point, m_min);
			m_max = Max(point, m_max);
		}

		void AddBoundingBox(const AxisAlignedBox& box)
		{
			AddPoint(box.m_min);
			AddPoint(box.m_max);
		}

		AxisAlignedBox Union(const AxisAlignedBox& box)
		{
			return AxisAlignedBox(Min(m_min, box.m_min), Max(m_max, box.m_max));
		}

		Vector3 GetMin() const { return m_min; }
		Vector3 GetMax() const { return m_max; }
		Vector3 GetCenter() const { return (m_min + m_max) * 0.5f; }
		
	private:
		Vector3 m_min;
		Vector3 m_max;
	};

	class OrientedBox
	{
	public:
		OrientedBox() = default;
		OrientedBox(const AxisAlignedBox& box)
		{
			m_repr.SetBasis(Matrix3::MakeScale(box.GetMax() - box.GetMin()));
			m_repr.SetTranslation(box.GetMin());
		}

		friend OrientedBox operator* (const AffineTransform& xform, const OrientedBox& obb)
		{
			return xform * obb.m_repr;
		}

		Vector3 GetDimensions() const { return m_repr.GetX() + m_repr.GetY() + m_repr.GetZ(); }
		Vector3 GetCenter() const { return m_repr.GetTranslation() + GetDimensions() * 0.5f; }

	private:
		OrientedBox(const AffineTransform& xform) : m_repr(xform) {}

		AffineTransform m_repr;
	};

	INLINE OrientedBox operator* (const UniformTransform& xform, const OrientedBox& obb)
	{
		return AffineTransform(xform) * obb;
	}
}