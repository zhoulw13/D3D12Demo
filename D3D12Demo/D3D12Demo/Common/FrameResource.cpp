#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT vertexCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, objectCount, true);

	DynamicVertex = std::make_unique<UploadBuffer<Vertex>>(device, vertexCount, false);
}

FrameResource::~FrameResource()
{

}