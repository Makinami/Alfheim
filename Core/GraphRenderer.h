#pragma once

#include "GraphicsCore.h"

namespace GraphRenderer
{
	void Initialize();
	void Shutdown();

	enum class GraphType { Global, Profile };
	typedef uint32_t GraphHandle;

	bool ToggleGraph(GraphHandle graphID, GraphType Type);
	GraphHandle InitGraph(GraphType Type);
	Color GetGraphColor(GraphHandle GraphID, GraphType Type);
	void Update(DirectX::XMFLOAT2 InputNode, GraphHandle GraphID, GraphType Type);
	void RenderGraphs(GraphicsContext& Context, GraphType Type);

	void SetSelectedIndex(uint32_t selectedIndex);
};

