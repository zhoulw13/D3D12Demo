#pragma once

#include "Common/d3dApp.h"
#include "Common/UploadBuffer.h"

#include <DirectXColors.h>
using namespace DirectX;

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};


class DemoApp : public D3DApp
{
public:
	DemoApp(HINSTANCE hInstance);

	bool Init() override;

protected:
	void Update() override;
	void Draw() override;
	void OnResize() override;

	void BuildGeometry();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildPSO();

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unique_ptr<MeshGeometry> mMeshGeo = nullptr;

	UINT vbByteSize;
	UINT ibByteSize;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

};