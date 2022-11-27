#pragma once

#include "VectorMath.h"
#include "Math/Frustum.h"

namespace Math
{
	class BaseCamera
	{
	public:
		// Cal this function once per frame and after you've changed any state. This
		// regenerates all matrices. Calling it more or less than once per frame will break
		// temporal effects and cause unpredictable results.
		void Update();

		// Public function for contraolling where the camera is and its orientation
		void SetEyeAtUp(Vector3 eye, Vector3 at, Vector3 up);
		void SetLookDirection(Vector3 forward, Vector3 up);
		void SetRotation(Quaternion basisRotation);
		void SetPosition(Vector3 worldPos);
		void SetTransform(const AffineTransform& xform);
		void SetTransform(const OrthogonalTransform& xform);

		const auto GetRotation() const { return m_CameraToWorld.GetRotation(); }
		const auto GetRightVec() const { return m_Basis.GetX(); }
		const auto GetUpVec() const { return m_Basis.GetY(); }
		const auto GetForwardVec() const { return -m_Basis.GetZ(); }
		const auto GetPosition() const { return m_CameraToWorld.GetTranslation(); }

		const auto& GetViewMatrix() const { return m_ViewMatrix; }
		const auto& GetProjMatrix() const { return m_ProjMatrix; }
		const auto& GetViewProjMatrix() const { return m_ViewProjMatrix; }
		const auto& GetReprojectionMatrix() const { return m_ReprojectMatrix; }
		const auto& GetViewSpaceFrustum() const { return m_FrustumVS; }
		const auto& GetWorldSpaceFrustum() const { return m_FrustumWS; }

	protected:
		BaseCamera() : m_CameraToWorld(kIdentity), m_Basis(kIdentity) {}

		void SetProjMatrix(const Matrix4& ProjMat) { m_ProjMatrix = ProjMat; }

		OrthogonalTransform m_CameraToWorld;

		// Redundant data cached for faster lookups.
		Matrix3 m_Basis;

		// Transform homogeneous coordinates from world space to view space. In this case, view space is defined as +X is
		// to the right, +Y is up, and -Z is forward. This has to match what the procetion matrix expects, but you might
		// also need to know what the convention is if you work in view space in a shader.
		Matrix4 m_ViewMatrix;        // i.e. "World-to-View" matrix

		// The projection matrix transforms view space to clip space. Once division by W has occurred, the finals coordinates
		// can be transformed by the viewport matrix to screen space. The projection matrix is determined by the screem aspect
		// and camera field of view. A projection matrix can also be orthographics. In that case, field of view would be defined
		// in linear units, not angles.
		Matrix4 m_ProjMatrix;        // i.e. "View-to-Projection" matrix
	
		// A concatenation of the view and projection matrices.
		Matrix4 m_ViewProjMatrix;    // i.e. "World-to-Projections" matrix

		// The view-projection matrix from the previous frame
		Matrix4 m_PreviousViewProjMatrix;

		// Projects a clip-space coordinate to the previous frame (useful for temporal effects).
		Matrix4 m_ReprojectMatrix;

		Frustum m_FrustumVS;         // View-space view frustum
		Frustum m_FrustumWS;         // World-space view frustum
	};

	class Camera : public BaseCamera
	{
	public:
		Camera();

		void SetPerspectiveMatrix(float verticalFovRadians, float aspectHeightOverWidth, float nearZClip, float farZClip);
		void SetFOV(float verticalFovInRadians) { m_VerticalFOV = verticalFovInRadians; UpdateProjMatrix(); }
		void SetAspectRatio(float heightOverWidth) { m_AspectRatio = heightOverWidth; UpdateProjMatrix(); }
		void SetZRange(float nearZ, float farZ) { m_NearClip = nearZ; m_FarClip = farZ; UpdateProjMatrix(); }
		void ReverseZ(bool enable) { m_ReverseZ = enable; UpdateProjMatrix(); }

	private:
		void UpdateProjMatrix(void);

		float m_VerticalFOV;
		float m_AspectRatio;
		float m_NearClip;
		float m_FarClip;
		bool m_ReverseZ;
	};

	inline void BaseCamera::SetEyeAtUp(Vector3 eye, Vector3 at, Vector3 up)
	{
		SetLookDirection(at - eye, up);
		SetPosition(eye);
	}

	inline void BaseCamera::SetPosition(Vector3 worldPos)
	{
		m_CameraToWorld.SetTranslation(worldPos);
	}

	inline void BaseCamera::SetTransform(const AffineTransform& xform)
	{
		// By using these functions, we rederive an orthogonal transform.
		SetLookDirection(-xform.GetZ(), xform.GetY());
		SetPosition(xform.GetTranslation());
	}

	inline void BaseCamera::SetRotation(Quaternion basisRotation)
	{
		m_CameraToWorld.SetRotation(Normalize(basisRotation));
		m_Basis = Matrix3(m_CameraToWorld.GetRotation());
	}

	inline Camera::Camera() : m_ReverseZ(true)
	{
		SetPerspectiveMatrix(XM_PIDIV4, 9.0f / 16.0f, 1.0f, 1000.0f);
	}

	inline void Camera::SetPerspectiveMatrix(float verticalFovRadians, float aspectHeightOverWidth, float nearZClip, float farZClip)
	{
		m_VerticalFOV = verticalFovRadians;
		m_AspectRatio = aspectHeightOverWidth;
		m_NearClip = nearZClip;
		m_FarClip = farZClip;

		UpdateProjMatrix();

		m_PreviousViewProjMatrix = m_ViewProjMatrix;
	}
}


