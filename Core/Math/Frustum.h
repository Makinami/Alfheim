#pragma once

#include "BoundingPlane.h"
#include "BoundingSphere.h"
#include "BoundingBox.h"

namespace Math
{
	class Frustum
	{
	public:
		Frustum() = default;

		Frustum(const Matrix4& ProjectionMatrix);

		enum CornerID
		{
			kNearLowerLeft, kNearUpperLeft, kNearLowerRight, kNearUpperRight,
			kFarLowerLeft,  kFarUpperLeft,  kFarLowerRight,  kFarUpperRight
		};

		enum PlaneID
		{
			kNearPlane, kFarPlane, kLeftPlane, kRightPlane, kTopPlane, kBottomPlane
		};

		auto GetFrustumCorner(CornerID id) const { return m_FrustumCorners[id]; }
		auto GetFrustumPlane(PlaneID id) const { return m_FrustumPlanes[id]; }

        // Test whether the bounding sphere intersects the frustum. Intersection is defined as either being
        // fully contained in the frustum, or by intersecting one or more of the planes.
        bool IntersectSphere(BoundingSphere sphere) const;
        
        // We don't officially have BoundingBox class yet, but let's assume it's forthcoming. (There is a 
        // simple struct in the Model project.)
        bool IntersectBoundingBox(const AxisAlignedBox& aabb) const;

		friend Frustum  operator* (const OrthogonalTransform& xform, const Frustum& frustum);    // Fast
		friend Frustum  operator* (const AffineTransform& xform, const Frustum& frustum);        // Slow
		friend Frustum  operator* (const Matrix4& xform, const Frustum& frustum);                // Slowest (and most general)

	private:
		void ConstructPerspectiveFrustum(float HTan, float VTan, float NearClip, float FarClip);

		// Orthographic frustum constructor (for box-shaped frusta)
		void ConstructOrthographicFrustum(float Left, float Right, float Top, float Bottom, float NearClip, float FarClip);

		Vector3 m_FrustumCorners[8];       // the corners of the frustum
		BoundingPlane m_FrustumPlanes[6];  // the bounding planes
	};

    inline bool Frustum::IntersectSphere(BoundingSphere sphere) const
    {
        const auto radius = sphere.GetRadius();
        for (auto plane : m_FrustumPlanes)
        {
            if (plane.DistanceFromPoint(sphere.GetCenter()) + radius < 0.0f)
                return false;
        }
        return true;
    }

    inline bool Frustum::IntersectBoundingBox(const AxisAlignedBox& aabb) const
    {
        for (auto plane : m_FrustumPlanes)
        {
            auto farCorner = Select(aabb.GetMin(), aabb.GetMax(), plane.GetNormal() > Vector3(kZero));
            if (plane.DistanceFromPoint(farCorner) < 0.0f)
                return false;
        }
        return true;
    }

    inline Frustum operator* (const OrthogonalTransform& xform, const Frustum& frustum)
    {
        Frustum result;

        for (int i = 0; i < 8; ++i)
            result.m_FrustumCorners[i] = xform * frustum.m_FrustumCorners[i];

        for (int i = 0; i < 6; ++i)
            result.m_FrustumPlanes[i] = xform * frustum.m_FrustumPlanes[i];

        return result;
    }

    inline Frustum operator* (const AffineTransform& xform, const Frustum& frustum)
    {
        Frustum result;

        for (int i = 0; i < 8; ++i)
            result.m_FrustumCorners[i] = xform * frustum.m_FrustumCorners[i];

        Matrix4 XForm = Transpose(Invert(Matrix4(xform)));

        for (int i = 0; i < 6; ++i)
            result.m_FrustumPlanes[i] = BoundingPlane(XForm * Vector4(frustum.m_FrustumPlanes[i]));

        return result;
    }

    inline Frustum operator* (const Matrix4& mtx, const Frustum& frustum)
    {
        Frustum result;

        for (int i = 0; i < 8; ++i)
            result.m_FrustumCorners[i] = Vector3(mtx * frustum.m_FrustumCorners[i]);

        Matrix4 XForm = Transpose(Invert(mtx));

        for (int i = 0; i < 6; ++i)
            result.m_FrustumPlanes[i] = BoundingPlane(XForm * Vector4(frustum.m_FrustumPlanes[i]));

        return result;
    }
}