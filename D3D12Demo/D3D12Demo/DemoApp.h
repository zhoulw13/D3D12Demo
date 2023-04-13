#pragma once

#include "Common/d3dApp.h"

class DemoApp : public D3DApp
{
public:
	DemoApp(HINSTANCE hInstance);

	bool Init() override;

protected:
	void Update() override;
	void Draw() override;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;


	UINT vbByteSize;
	UINT ibByteSize;

	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;

	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;
};