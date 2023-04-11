

#include "Common/d3dApp.h"
#include "d3d12.h"



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	D3DApp app(hInstance);
	app.Init();

	return 0;
}