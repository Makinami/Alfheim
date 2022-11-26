#include "pch.h"

#include <random>

#include "GameCore.h"
#include "Camera.h"
#include "CameraController.h"
#include "BufferManager.h"

#include "PrimitiveRenderer.h"
#include "GltfRenderer.h"
#include "Model.h"
#include "Terrain.h"

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
	GltfRenderer m_Gltf;
	Model m_Model;
	std::vector<Math::Matrix4> m_Transformations;
	std::vector<SimpleLight> m_SimpleLights;
	
	Terrain m_Terrain;
};

CREATE_APPLICATION( Alfheim )

void Alfheim::Startup(void)
{
	m_Camera.SetEyeAtUp({ 5.f, 0.f, 0.f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });
	m_Camera.Update();
	m_CameraController = std::make_unique<CameraController>(m_Camera, Math::Vector3(Math::kYUnitVector));
	m_CameraController->SetMovementSpeed(50.f);

	m_Gltf.Initialize();

	m_Model = ModelReader().Load("./Stone.glb");

	std::mt19937 gen(42);
	std::uniform_real_distribution<float> dis(-10.0, 10.0);
	for (int i = 0; i < 100; ++i) {
		m_Transformations.emplace_back(Math::OrthogonalTransform::MakeTranslation({dis(gen), dis(gen), dis(gen)}));
	}

	m_SimpleLights.insert(m_SimpleLights.end(), {
		SimpleLight{ XMFLOAT3{ 1.f, 1.f, 1.f }, 20.f, XMFLOAT3{}, 40.f, XMFLOAT3{ 10.f, 10.f, 10.f }, 0 },
		SimpleLight{ XMFLOAT3{ 0.5f, 0.5f, 1.f }, 20.f, XMFLOAT3{}, 40.f, XMFLOAT3{ -10.f, 10.f, 10.f }, 0 },
		SimpleLight{ XMFLOAT3{ 1.f, 0.5f, 0.5f }, 20.f, XMFLOAT3{}, 40.f, XMFLOAT3{ 10.f, -10.f, 10.f }, 0 },
		SimpleLight{ XMFLOAT3{ 0.1f, 0.1f, 0.1f }, 20.f, XMFLOAT3{}, 40.f, XMFLOAT3{ 10.f, -10.f, -10.f }, 0 }
	});
	
	m_PrimitiveRenderer.Initialize();
	m_Terrain.Initialize();
}

void Alfheim::Update([[maybe_unused]] float deltaT)
{
	m_CameraController->Update(deltaT);

	m_Terrain.Update(deltaT);
}

void Alfheim::RenderScene(void)
{
	auto& gfxContext = GraphicsContext::Begin(L"Main context");

	gfxContext.TransitionResource(Graphics::g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	gfxContext.TransitionResource(Graphics::g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
	gfxContext.ClearColor(Graphics::g_SceneColorBuffer);
	gfxContext.ClearDepth(Graphics::g_SceneDepthBuffer);

	gfxContext.SetRenderTarget(Graphics::g_SceneColorBuffer.GetRTV(), Graphics::g_SceneDepthBuffer.GetDSV());

	m_Terrain.Render(gfxContext, m_Camera);

	m_PrimitiveRenderer.Render(gfxContext, m_Camera);

	gfxContext.Finish();
}
