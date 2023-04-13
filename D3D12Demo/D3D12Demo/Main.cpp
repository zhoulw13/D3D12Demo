

#include "Common/d3dApp.h"
#include "DemoApp.h"
#include "d3d12.h"



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	DemoApp app(hInstance);
	app.Init();

	app.Run();

	return 0;
}