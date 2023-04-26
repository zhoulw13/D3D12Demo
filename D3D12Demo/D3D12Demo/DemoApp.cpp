#include "DemoApp.h"


DemoApp::DemoApp(HINSTANCE hInstance)
	:D3DApp(hInstance)
{

}

bool DemoApp::Init()
{
	if (!D3DApp::Init())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildRootSignature();
	BuildGeometry();
	BuildRenderItems();
	BuildFrameResource();
	//BuildConstantBuffers();
	BuildPSO();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void DemoApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);

}

void DemoApp::Update(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	//XMMATRIX world = XMLoadFloat4x4(&mWorld);
	//XMMATRIX proj = XMLoadFloat4x4(&mProj);
	//XMMATRIX worldViewProj = world * view * proj;

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, 0, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
	UpdateGeometry(gt);
}

void DemoApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO.Get()));

	// Indicate a state transition on the resource usage.
	D3D12_RESOURCE_BARRIER Barriers = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ResourceBarrier(1, &Barriers);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	D3D12_CPU_DESCRIPTOR_HANDLE bbv = CurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = DepthStencilView();
	mCommandList->OMSetRenderTargets(1, &bbv, true, &dsv);


	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	//ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	//mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	//auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	//passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	//mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	DrawRenderItems();

	// Indicate a state transition on the resource usage.
	Barriers = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	mCommandList->ResourceBarrier(1, &Barriers);

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	//FlushCommandQueue();

	mCurrFrameResource->Fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}


void DemoApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX worldViewProj = XMLoadFloat4x4(&e->World);

			// Update the constant buffer with the latest worldViewProj matrix.
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void DemoApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMVECTOR viewDeterminant = XMMatrixDeterminant(view);
	XMVECTOR projDeterminant = XMMatrixDeterminant(proj);
	XMVECTOR viewProjDeterminant = XMMatrixDeterminant(viewProj);

	XMMATRIX invView = XMMatrixInverse(&viewDeterminant, view);
	XMMATRIX invProj = XMMatrixInverse(&projDeterminant, proj);
	XMMATRIX invViewProj = XMMatrixInverse(&viewProjDeterminant, viewProj);

	PassConstants mMainPassCB;

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	mCurrFrameResource->PassCB->CopyData(0, mMainPassCB);
}

void DemoApp::UpdateGeometry(const GameTimer& gt)
{
	//mMeshGeo->VertexBufferGPU = 
	auto CurrVertexVB = mCurrFrameResource->DynamicVertex.get();
	for (int i = 0; i < vertices.size(); i++)
	{
		Vertex v;
		v.Pos = vertices[i].Pos;
		v.Pos.x += std::sin(gt.TotalTime());
		v.Color = vertices[i].Color;
		
		CurrVertexVB->CopyData(i, v);
	}
	mMeshGeo->VertexBufferGPU = CurrVertexVB->Resource();
}

void DemoApp::BuildRootSignature()
{
	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	//// Create a single descriptor table of CBVs.
	//CD3DX12_DESCRIPTOR_RANGE cbvTable;
	//cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	//CD3DX12_DESCRIPTOR_RANGE cbvTable2;
	//cbvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	//slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);
	//slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable2);

	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));

}

void DemoApp::BuildGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 2.0f, 0.5f, 3);
	//GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 10, 10);
	GeometryGenerator::MeshData sphere = geoGen.CreateGeosphere(0.5f, 1);

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = 0;
	boxSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = (UINT)box.Indices32.size();
	sphereSubmesh.BaseVertexLocation = (UINT)box.Vertices.size();

	totalVertexSize = box.Vertices.size() + sphere.Vertices.size();

	//std::vector<Vertex> vertices(totalVertexSize);
	vertices.resize(totalVertexSize);
	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); i++, k++)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(Colors::Yellow);
	}

	for (size_t i = 0; i < sphere.Vertices.size(); i++, k++)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(Colors::Green);
	}

	std::vector<uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));

	mMeshGeo = std::make_unique<MeshGeometry>();
	mMeshGeo->Name = "example";

	vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	//mMeshGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, mMeshGeo->VertexBufferUploader);
	mMeshGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, mMeshGeo->IndexBufferUploader);

	mMeshGeo->VertexByteStride = sizeof(Vertex);
	mMeshGeo->VertexBufferByteSize = vbByteSize;
	mMeshGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mMeshGeo->IndexBufferByteSize = ibByteSize;

	mMeshGeo->DrawArgs["box"] = boxSubmesh;
	mMeshGeo->DrawArgs["sphere"] = sphereSubmesh;
}

void DemoApp::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->ObjCBIndex = 0;
	boxRitem->IndexCount = mMeshGeo->DrawArgs["box"].IndexCount;
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->StartIndexLocation = mMeshGeo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = mMeshGeo->DrawArgs["box"].BaseVertexLocation;
	boxRitem->World = MathHelper::Identity4x4();
	boxRitem->Geo = mMeshGeo.get();

	XMMATRIX sphereWorld = XMMatrixTranslation(-1.0f, 1.5f, 0.0f);
	auto sphereRitem = std::make_unique<RenderItem>();
	sphereRitem->ObjCBIndex = 1;
	sphereRitem->IndexCount = mMeshGeo->DrawArgs["sphere"].IndexCount;
	sphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphereRitem->StartIndexLocation = mMeshGeo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem->BaseVertexLocation = mMeshGeo->DrawArgs["sphere"].BaseVertexLocation;
	sphereRitem->Geo = mMeshGeo.get();
	XMStoreFloat4x4(&sphereRitem->World, sphereWorld);

	mAllRitems.push_back(std::move(boxRitem));
	mAllRitems.push_back(std::move(sphereRitem));
}

void DemoApp::BuildFrameResource()
{
	for (int i = 0; i < gNumFrameResources; i++)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, mAllRitems.size(), totalVertexSize));
	}
}

void DemoApp::BuildConstantBuffers()
{
	UINT objCount = (UINT)mAllRitems.size();

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = (objCount + 1) * gNumFrameResources;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;

	mPassCbvOffset = objCount * gNumFrameResources;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	for (int frameIdx = 0; frameIdx < gNumFrameResources; frameIdx++)
	{
		auto objectCB = mFrameResources[frameIdx]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; i++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			int heapIndex = frameIdx * objCount + i;
			cbAddress += i * objCBByteSize;

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	for (int frameIdx = 0; frameIdx < gNumFrameResources; frameIdx++)
	{
		auto passCB = mFrameResources[frameIdx]->PassCB->Resource();


		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		int heapIndex = mPassCbvOffset + frameIdx;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}

}

void DemoApp::BuildPSO()
{
	mInputLayout =
	{
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();

	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};

	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};

	D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

void DemoApp::DrawRenderItems()
{
	auto objCount = (UINT)mAllRitems.size();

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	for (UINT i = 0; i < objCount; i++)
	{
		auto Ritem = mAllRitems[i].get();
		D3D12_VERTEX_BUFFER_VIEW vbv = Ritem->Geo->VertexBufferView();
		D3D12_INDEX_BUFFER_VIEW ibv = Ritem->Geo->IndexBufferView();

		mCommandList->IASetVertexBuffers(0, 1, &vbv);
		mCommandList->IASetIndexBuffer(&ibv);
		mCommandList->IASetPrimitiveTopology(Ritem->PrimitiveType);

		//UINT cbvIndex = mCurrFrameResourceIndex * objCount + (UINT)Ritem->ObjCBIndex;
		//auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		//cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);
		//mCommandList->SetGraphicsRootDescriptorTable(0, cbvHandle);
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress();
		objCBAddress += Ritem->ObjCBIndex * objCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		mCommandList->DrawIndexedInstanced(Ritem->IndexCount, 1, Ritem->StartIndexLocation, Ritem->BaseVertexLocation, 0);
	}
}