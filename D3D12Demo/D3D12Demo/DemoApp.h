#pragma once

#include "Common/d3dApp.h"
#include "Common/UploadBuffer.h"
#include "Common/FrameResource.h"
#include "Common/GeometryGenerator.h"

#include <DirectXColors.h>
using namespace DirectX;

const int gNumFrameResources = 3;

struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

class DemoApp : public D3DApp
{
public:
	DemoApp(HINSTANCE hInstance);

	bool Init() override;

protected:
	void Update(const GameTimer& gt) override;
	void Draw(const GameTimer& gt) override;
	void OnResize() override;

	void BuildGeometry();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildPSO();
	void BuildFrameResource();
	void BuildRenderItems();

	void DrawRenderItems();

	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateGeometry(const GameTimer& gt);

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unique_ptr<MeshGeometry> mMeshGeo = nullptr;

	UINT vbByteSize;
	UINT ibByteSize;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	UINT mPassCbvOffset = 0;

	std::vector<Vertex> vertices;
	UINT totalVertexSize = 0;
};