#include "pch.h"
#include "GameCore.h"
#include "Camera.h"
#include "CameraController.h"
#include "BufferManager.h"

#include "PrimitiveRenderer.h"
#include "Water.h"

using namespace GameCore;

class Alfheim : public GameCore::IGameApp
{
public:
	Alfheim() {}

	virtual void Startup(void) override;
	virtual void Cleanup(void) override {};

	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:
	Math::Camera m_Camera;
	std::unique_ptr<CameraController> m_CameraController;

	PrimitiveRenderer m_PrimitiveRenderer;
	Water m_Water;
};

CREATE_APPLICATION( Alfheim )

void Alfheim::Startup(void)
{
	m_Camera.SetEyeAtUp({ 10.f, 10.f, 0.f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });
	m_CameraController = std::make_unique<CameraController>(m_Camera, Math::Vector3(Math::kYUnitVector));
	m_CameraController->SetMovementSpeed(50.f);

	//m_PrimitiveRenderer.Initialize();

	m_Water.Initialize();
	m_Water.SetWind({ 10.f, 10.f });
}

void Alfheim::Update([[maybe_unused]] float deltaT)
{
	m_CameraController->Update(deltaT);

	m_Water.Update(deltaT);
}

void Alfheim::RenderScene(void)
{
	auto& gfxContext = GraphicsContext::Begin(L"Main context");

	gfxContext.TransitionResource(Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	gfxContext.TransitionResource(Graphics::g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
	gfxContext.ClearColor(Graphics::g_SceneColorBuffer);
	gfxContext.ClearDepth(Graphics::g_SceneDepthBuffer);

	gfxContext.SetRenderTarget(Graphics::g_SceneColorBuffer.GetRTV(), Graphics::g_SceneDepthBuffer.GetDSV());

	m_Water.Render(gfxContext, m_Camera);

	gfxContext.Finish();
}
