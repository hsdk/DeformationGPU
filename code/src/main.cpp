//   Copyright 2013 Henry Schäfer
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License  is  distributed on an 
//   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//	 either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

#include "stdafx.h"

#ifdef PROFILE
// nsight shader debugger does not understand precompiled shaders, use define to disable precompiled 
#define NSIGHT_MODE 
#endif

#include <DXUT.h>
#include <DXUTcamera.h>
#include <DXUTGui.h>
#include <DXUTSettingsDlg.h>
#include <SDKMisc.h>

#include "resource.h"
#include "App.h"

#include "scene/Scene.h"
#include "scene/ModelLoader.h"
#include "scene/ModelInstance.h"
#include "scene/DXSubDModel.h"

#include "dynamics/Physics.h"
#include "dynamics/AnimationGroup.h"
#include "dynamics/SkinningAnimation.h"
#include "dynamics/Car.h"
#include "dynamics/Character.h"

#include "rendering/RendererDebug.h"
#include "rendering/RendererTri.h"
#include "rendering/RendererSubD.h"
#include "rendering/PostPro.h"
#include "rendering/ShadowMapping.h"

#include <SDX/DXDebugStreamOutput.h>
#include <SDX/DXObjectOrientedBoundingBox.h>

#include "Pipeline.h"

#include "TileEdit.h"
#include "IntersectPatches.h"
#include "MemoryManager.h"
#include "TileOverlapUpdater.h"
#include "Voxelization.h"

#include <SDX/DXPerfEvent.h>
#include <SDX/DXShaderManager.h>
#include "utils/FrameProfiler.h"

//#define TW_NO_LIB_PRAGMA
#include "AntTweakBar.h"


#include <atlbase.h>
#include <atlconv.h>

#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>


using namespace DirectX;
using namespace Microsoft::WRL;


App g_app;

Scene*						g_scene = NULL;
Physics*					g_physics = NULL;

RendererBoundingGeometry	g_rendererBBoxes;
SkinningAnimation			g_skinning;
PostPro						g_postProPipeline;
ShadowMapping				g_shadow;


//#define FIRSTPERSON
#ifdef FIRSTPERSON
CFirstPersonCamera			g_Camera; 
#else
CModelViewerCamera			g_Camera; 
#endif
CModelViewerCamera			g_LightCamera; 


ID3D11Buffer*				g_CBEyeZdist;
ID3D11Buffer*				g_CBLight;

ID3D11RasterizerState*		g_pRasterizerStateSolid = NULL;
ID3D11RasterizerState*		g_pRasterizerStateWiref = NULL;

//ID3D11BlendState *g_pBlendStateEnabled;

ID3D11BlendState*			g_pBlendStateAlpha = NULL;
ID3D11BlendState*			g_pBlendStateDefault= NULL;
ID3D11DepthStencilState*	g_pDepthStencilStateDefault = NULL;
ID3D11DepthStencilState*    g_pDepthStencilStateNoDepth = NULL;


static float g_zFNDist = 100;

float						g_AnimationSpeed = 0.0f;
bool						g_bShowWire = false;

UINT						g_bShowGUI = 0;

bool						g_bUseWhiteBackground = true;
bool						g_bUseAALines = true;

XMINT2						g_MousePosition (0,0); 
static int					whichScreenShot = 0;

bool						g_withPostPro = false;
bool						g_bUsePhysicsCollisionDetection = true;		 // FALSE FOR FRANKIE!

bool						g_bShowIntersectingOBB = false;
bool						g_UseSkinning = true;
bool						g_bShowOBBs = false; 
bool						g_bShowTriModels = true;

bool						g_bUseAdaptiveVoxelization = true;
float						g_fAdaptiveVoxelizationScale = 10.f;		

//bool						g_bShowVoxelization = false;
bool						g_bVoxelizeAllObjects = false;			// no intersection, do whole object voxelization

bool						g_bTriggerPause = false;

bool						g_UseShadowMapping = true;
bool						g_ShowShadowDebug = false;
bool						g_ShowLightFrusta = false;
int							g_ShowCascadeIdx = 0;
bool						g_visualizeCascades = false;

bool						g_bShowControlMesh = false;

bool						g_showPhysics = false;

bool						g_UseFollowCam;				// controlled with F8
UINT						g_iCamFollowGroup = 0;		// switch followed group with left/right arrow keys

#define CAM_ARCBALL 0
#define	CAM_FOLLOW1 1
#define	CAM_FOLLOW2 2
uint32_t					g_CameraSelector = 2;

bool						g_UseSkydome = true;
ModelGroup*					g_SkydomeGroup = NULL;

bool						g_dumpFrames = false;
std::string					g_dumpDirectory;
UINT						g_dumpFrame = 0;
bool						g_dumpDebug = false;

bool						g_UseCar = true;
Car*						g_Car = NULL;

// custom frame capture -  fraps does not work with DX11.1
bool						g_DumpRecord = false;//false;	// false
bool						g_PlayRecord = false;// !g_DumpRecord; // false

Character*					g_Character = NULL;

bool						g_PickClosestCamera = true;
AnimationGroup*				g_AnimatedCharacter  = NULL;
ModelGroup*					g_FrogGroup = NULL;


bool						g_ShowTweakBar = true;

bool						g_UseMSAABackBuffer = true;

XMFLOAT3					g_LightDir(0.0f, 0.0f, -1.0f);
bool						g_UpdateLightDir = true;

std::string					g_CurrentCameraFile = std::string("camera.cam");
std::string					g_CurrentCharFile = std::string("character.char");


float g_far = 100.f;
float g_near = 1.f;

static bool controller_manager_screen_visible = true;
std::string controller_manager_text_string;

// voxelize collider obb or intersecting volume
#define VOXELIZE_COLLIDER_OBB

//float g_obbScaler = 1.01f;//25;


float g_ClearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
float g_ClearColorWhite[4] = { 1.0f, 1.0f, 1.0f, 0.0f };

DirectX::XMFLOAT4A m_lightDir = DirectX::XMFLOAT4A(0, -1, 0, 0);

D3D_DRIVER_TYPE DXDriverType = D3D_DRIVER_TYPE_HARDWARE;

// Global Variables:
HINSTANCE hInst;								// current instance


void ApplyGlobalSettings()
{
	g_app.g_bRunSimulation = false;
	g_app.g_fDisplacementScalar = 1.0f;
	g_app.g_displacementTileSize = 128;
	g_app.g_colorTileSize	     = 32;
	g_app.g_maxSubdivisions      = 5;

	g_app.g_memDebugDoPrealloc		= false; // prealloc for all mesh patches and disable mem management

	// with memory management
	g_app.g_memNumColorTiles	    = 3000;	 // preallocated buffer elements
	g_app.g_memNumDisplacementTiles = 3000;//2000;	 // preallocated buffer elements

	g_app.g_memMaxNumTilesPerObject = 85000;

	g_app.g_adaptiveVoxelizationScale	= 50;
	g_app.g_bShowVoxelization			= false;
	g_app.g_showAllocated				= false;
	g_app.g_fTessellationFactor			= 32.f;
	g_app.g_voxelDeformationMultiSampling = false;
	g_app.g_withVoxelJittering			= true;
	g_app.g_withVoxelOBBRotate			= false;
	g_app.g_profilePipelineStages		= false;
	g_app.g_useCompactedVisibilityOverlap = true;
	g_app.g_withOverlapUpdate = true;

	g_app.g_useCulling					= true;		// ALWAYS ENABLE!!!, use below to disable culling for ray casting		// culling doubles performance on gtx 480, TODO patch frustum culling
	g_app.g_useCullingForRayCast		= true;

	g_app.g_useDisplacementConstraints = false;  // activate for best quality - ! has to be active on app start 

	g_app.g_withPaintSculptTimings = false;
}


void RenderSkydome(ID3D11DeviceContext1* pd3dImmediateContext) 
{
	if (!g_UseSkydome || g_bShowWire) 
		return;

	if(!g_SkydomeGroup) return;

	pd3dImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateNoDepth, 1);
	
	g_app.UpdateCameraCBForModel(pd3dImmediateContext, XMMatrixIdentity());
	g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);
	for(auto model: g_SkydomeGroup->triModels) {
		g_renderTriMeshes.FrameRenderSkydome(pd3dImmediateContext, model);
	}

	pd3dImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateDefault, 1);
}

void RenderSceneObjects( ID3D11DeviceContext1* pd3dImmediateContext, bool showTriModels ) 
{

	g_frameProfiler.BeginQuery(pd3dImmediateContext, DXPerformanceQuery::RENDER);
	for (const auto* group : g_scene->GetModelGroups()) 	
	{	
		g_app.UpdateCameraCBForModel(pd3dImmediateContext, group->GetModelMatrix());
		g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);


		for(auto* instance : group->modelInstances)
		{
			if(instance->IsSubD())
			{
				g_rendererSubD.FrameRender(pd3dImmediateContext, instance);
			}
			else
			{
				if(showTriModels)
					g_renderTriMeshes.FrameRender(pd3dImmediateContext, instance);				
			}				
		}
	}
	g_frameProfiler.EndQuery(pd3dImmediateContext, DXPerformanceQuery::RENDER);
}



//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
	if (g_app.g_bTimingsEnabled) g_app.g_TimingLog.printTimings();	
}



void ReloadShadersDebug()
{
	static UINT reloadUpdateFrameCounter = 0;
	if(reloadUpdateFrameCounter % 100 == 0)
	{	
		reloadUpdateFrameCounter = 0;
		if(FAILED(g_shaderManager.CheckAndReload()))
		{
			std::cout << "shader errors" << std::endl;
			Sleep(100);
			return;
		}
	}
	reloadUpdateFrameCounter++;
}


void StoreCamera(std::string cameraFilename = std::string("camera.cam")) {
	std::ofstream outfile(cameraFilename.c_str(), std::ios::out | std::ios::binary);
	
	XMVECTOR qV;
	XMFLOAT4 q;
	g_Camera.GetViewQuat(qV);
	XMStoreFloat4(&q, qV);
	
	XMFLOAT3 eye = *g_Camera.GetEyePt();
	XMFLOAT3 lookat = *g_Camera.GetLookAtPt();
	//
	outfile.write(reinterpret_cast<char*>(&eye), sizeof(XMFLOAT3));
	outfile.write(reinterpret_cast<char*>(&lookat), sizeof(XMFLOAT3));
	outfile.write(reinterpret_cast<char*>(&q), sizeof(XMFLOAT4));
	outfile.write(reinterpret_cast<char*>(&g_LightDir), sizeof(XMFLOAT3));


	outfile.close();
}

void LoadCamera(std::string cameraFilename = std::string("camera.cam")) {
	std::ifstream infile(cameraFilename.c_str(), std::ios::in | std::ios::binary);

	if (infile.is_open())
	{
		XMFLOAT3 eye;
		XMFLOAT3 lookat;
		XMFLOAT4 q;

		infile.read(reinterpret_cast<char*>(&eye), sizeof(XMFLOAT3));
		infile.read(reinterpret_cast<char*>(&lookat), sizeof(XMFLOAT3));
		infile.read(reinterpret_cast<char*>(&q), sizeof(XMFLOAT4));
		infile.read(reinterpret_cast<char*>(&g_LightDir), sizeof(XMFLOAT3));

		g_Camera.SetViewParams(&eye, &lookat);
		XMVECTOR qV = XMLoadFloat4(&q);
		g_Camera.SetViewQuat(qV);

		infile.close();
	}
}

void StoreChar(std::string cameraFilename = std::string("character.char")) {
	if (!g_Character) return;
	std::ofstream outfile(cameraFilename.c_str(), std::ios::out | std::ios::binary);

	btTransform t = g_Character->getCurrentCharTrafo();
	outfile.write(reinterpret_cast<char*>(&t), sizeof(btTransform));

	outfile.close();
}

void LoadChar(std::string cameraFilename = std::string("character.char")) {
	if (!g_Character) return;
	std::ifstream infile(cameraFilename.c_str(), std::ios::in | std::ios::binary);

	btTransform t;
	infile.read(reinterpret_cast<char*>(&t), sizeof(btTransform));
	g_Character->setCurrentCharTrafo(t);

	infile.close();
}

void GameControls(float fElapsedTime)
{
	if(DXUTGetHWND() != GetForegroundWindow())
		return;
	if(g_Car && g_app.g_bRunSimulation) {	
		if (g_Car->IsReplaying()) {
			g_Car->Replay(fElapsedTime, g_physics);
		} else {
			g_Car->BeginEvent(fElapsedTime);

			g_Car->StopAccelerate();
			g_Car->StopBreak();

			if (GetAsyncKeyState('A'))
				g_Car->SteerRight(fElapsedTime);				

			if (GetAsyncKeyState('D'))
				g_Car->SteerLeft(fElapsedTime); ;					

			if (GetAsyncKeyState('W'))
				g_Car->StartAccelerate();

			if (GetAsyncKeyState('S'))
				g_Car->StartAccelerate(-0.5f);

			if (GetAsyncKeyState(VK_SHIFT))
				g_Car->StartBreak();

			if (GetAsyncKeyState('E'))
				g_Car->Reset(g_physics);
			g_Car->EndEvent();


		}
	} else if (g_Character && g_app.g_bRunSimulation) {
		if (!g_PlayRecord) {
			g_Character->ResetControls();
			if (GetAsyncKeyState('A'))
				g_Character->Right();				

			if (GetAsyncKeyState('D'))
				g_Character->Left(); ;					

			if (GetAsyncKeyState('W'))
				g_Character->Forward();

			if (GetAsyncKeyState('S'))
				g_Character->Backward();

			//if (GetAsyncKeyState(VK_SHIFT))
			//	g_Character->Jump();

			if (GetAsyncKeyState('E')) {
				ModelInstance* mi = g_AnimatedCharacter->modelInstances[0];
				//SetIsColliderForInstanceGroup(mi, !mi->IsCollider());
			}


			g_Character->CaptureEvent();
			g_Character->move(fElapsedTime);
		} else {
			g_Character->Replay(fElapsedTime);
		}

		// -X = +X
		// +Y = -Z
		//g_AnimatedCharacter->SetModelMatrix(XMMatrixTranslation(-30,26,2.5));
		//if (g_AnimatedCharacter) g_AnimatedCharacter->SetModelMatrix(g_Character->getTrafo());
	}
}



void ApplyAdaptiveSubdivision(ID3D11DeviceContext1* pd3dImmediateContext, const XMMATRIX& mView, const XMMATRIX& mProj)
{
	static UINT g_frameNr = 0;
	static UINT g_subdUpdateFrame =20;	

	if(g_frameNr>0) return; 
	// todo: static meshes need only an a single update 
	// but feature adaptive subd update should be run 
	// on animated models each frame 

	g_frameNr++;
	for(auto group : g_scene->GetModelGroups())
	{	
		for(auto mesh: group->osdModels)
		{			
			PERF_EVENT_SCOPED(perf,L"OSD Subdivision Kernel");			
			// todo maybe precompute tess factors to make them match in shadow and standard rendering
			// todo maybe refine only up to specific subd level (<MAX_SUBDIVS) depending on model distance

			if (g_app.g_bTimingsEnabled) 
			{
				g_app.WaitForGPU();

				g_app.g_TimingLog.m_dGPUSubdivTotal -= GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);

				for (UINT i = 0; i < g_app.g_TimingLog.m_uNumRuns+1; i++) //needs to be an odd number
				{
					mesh->GetMesh()->Refine();  // run gpu subdiv			
				}

				g_app.WaitForGPU();
				g_app.g_TimingLog.m_dGPUSubdivTotal += GetTimeMS()/(double)(g_app.g_TimingLog.m_uNumRuns+1);
				g_app.g_TimingLog.m_uGPUSubdivCount++;
			}
			else 
			{					
				mesh->GetMesh()->Refine();  // run gpu subdiv				
				//mesh->GetMesh()->Synchronize(); // wait for update
			}

			// todo apply global render settings to opensubdiv render
		}
	}
	g_frameNr = (++g_frameNr % g_subdUpdateFrame);
}



//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
	assert( pDeviceSettings->d3d11.DeviceFeatureLevel >= D3D_FEATURE_LEVEL_11_0 );

	// For the first device created if its a REF device, optionally display a warning dialog box
	static bool s_bFirstTime = true;
	if( s_bFirstTime )
	{
		s_bFirstTime = false;

		// Set driver type based on cmd line / program default
		pDeviceSettings->d3d11.DriverType = DXDriverType;

		// Disable vsync
		pDeviceSettings->d3d11.SyncInterval = 0;

		if( ( pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
		{
			
			DXUTDisplaySwitchingToREFWarning(  );
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo,
									   UINT Output,
									   const CD3D11EnumDeviceInfo *DeviceInfo,
									   DXGI_FORMAT BackBufferFormat,
									   bool bWindowed,
									   void* pUserContext )
{
	return true;
}



// TODO clear also memory assignment
void ClearDisplacementTiles(ID3D11DeviceContext1* pd3dImmediateContext)
{	
	if(g_memoryManager.GetDisplacementDataUAV())
	{
		float clearVals[4] = {0,0,0,0};
		pd3dImmediateContext->ClearUnorderedAccessViewFloat(g_memoryManager.GetDisplacementDataUAV(),clearVals);

		if(g_app.g_useDisplacementConstraints)
			pd3dImmediateContext->ClearUnorderedAccessViewFloat(g_memoryManager.GetDisplacementConstraintsUAV(),clearVals);
	}
}

void UpdateCamPosZNearFarCB(ID3D11DeviceContext1* pd3dImmediateContext)
{
	float fNear = g_Camera.GetNearClip();
	float fFar	= g_Camera.GetFarClip();
	g_zFNDist = fNear - fFar;

	D3D11_MAPPED_SUBRESOURCE MappedResource;	
	pd3dImmediateContext->Map( g_CBEyeZdist, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_CAMEYE_DIST* pCBEyeZdist = ( CB_CAMEYE_DIST* )MappedResource.pData;		
	pCBEyeZdist->eye = *g_Camera.GetEyePt();//XMFLOAT3A(g_Camera.GetEyePt()->x, g_Camera.GetEyePt()->y,g_Camera.GetEyePt()->z);
	pCBEyeZdist->zfzn = g_zFNDist;
	pCBEyeZdist->znear =g_near;
	pCBEyeZdist->zfar = fFar;
	pd3dImmediateContext->Unmap( g_CBEyeZdist, 0 );

	pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
	pd3dImmediateContext->VSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );	
	pd3dImmediateContext->DSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
	pd3dImmediateContext->HSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
	pd3dImmediateContext->GSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
	pd3dImmediateContext->PSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device1* pd3dDevice, 
									  const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
									  void* pUserContext )
{
	HRESULT hr;

	static bool bFirstOnCreateDevice = true;

	// Warn the user that in order to support DX11, a non-hardware device has been created, continue or quit?
	if ( DXUTGetDeviceSettings().d3d11.DriverType != D3D_DRIVER_TYPE_HARDWARE && bFirstOnCreateDevice )
	{
		if ( MessageBox( 0, L"No Direct3D 11 hardware detected. "\
			L"In order to continue, a non-hardware device has been created, "\
			L"it will be very slow, continue?", L"Warning", MB_ICONEXCLAMATION | MB_YESNO ) != IDYES )
			return E_FAIL;
	}

	bFirstOnCreateDevice = false;
	

	D3D11_RASTERIZER_DESC RasterDesc;
	ZeroMemory( &RasterDesc, sizeof(D3D11_RASTERIZER_DESC) );
	RasterDesc.DepthClipEnable = TRUE;
	RasterDesc.AntialiasedLineEnable = false;
	RasterDesc.CullMode = D3D11_CULL_BACK;			// TODO ENABLE
	RasterDesc.FillMode = D3D11_FILL_SOLID;
	
	V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateSolid ) );
	DXUT_SetDebugName(g_pRasterizerStateSolid, "g_pRasterizerStateSolid");
	RasterDesc.AntialiasedLineEnable = g_bUseAALines;
	RasterDesc.FillMode = D3D11_FILL_WIREFRAME;
	V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateWiref ) );
	DXUT_SetDebugName(g_pRasterizerStateWiref, "g_pRasterizerStateWiref");
	
	D3D11_BLEND_DESC BlendDesc;
	ZeroMemory(&BlendDesc, sizeof(D3D11_BLEND_DESC));
	BlendDesc.RenderTarget[0].BlendEnable = TRUE;
	BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	BlendDesc.RenderTarget[0].BlendOpAlpha =  D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	V_RETURN(pd3dDevice->CreateBlendState(&BlendDesc, &g_pBlendStateAlpha));
	DXUT_SetDebugName(g_pBlendStateAlpha,"g_pBlendStateAlpha");
	

	D3D11_DEPTH_STENCIL_DESC descDS;
	ZeroMemory(&descDS, sizeof(D3D11_DEPTH_STENCIL_DESC));
	descDS.DepthEnable = FALSE;
	descDS.StencilEnable = FALSE;
	V_RETURN(pd3dDevice->CreateDepthStencilState(&descDS, &g_pDepthStencilStateNoDepth));
	
	ID3D11DeviceContext1* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

	XMFLOAT3 vecEye( 0.f, 0.f, 50.f);
	XMFLOAT3 vecAt( 0.f, 0.f, 0.f);

	g_Camera.SetViewParams( &vecEye, &vecAt );

	//vecEye = XMFLOAT3( 0.f, 200, 800.f);
	XMFLOAT3 vecLightEye = XMFLOAT3( 200.f, -200, 400.f);
	XMFLOAT3 vecLightAt = XMFLOAT3(0,0,0);
	//XMVECTOR lightDir = 
	g_LightCamera.SetViewParams(&vecLightEye, &vecLightAt);
	g_LightCamera.SetProjParams( XM_PIDIV4, 1, 20.f, 1000.0f );
	g_LightCamera.FrameMove( 0 );
#ifdef FIRSTPERSON


	g_Camera.SetRotateButtons(TRUE, FALSE, TRUE);	
	g_Camera.SetScalers( 0.001f, 100.0f );
	g_Camera.SetDrag( false );
	//g_Camera.SetEnableYAxisMovement( true );
	//g_Camera.SetEnableXAxisMovement( true );
	//XMFLOAT3 vMin = XMFLOAT3( -1000.0f, -1000.0f, -1000.0f );
	//XMFLOAT3 vMax = XMFLOAT3( 1000.0f, 1000.0f, 1000.0f );
	//g_Camera.SetClipToBoundary( TRUE, vMin, vMax );
	g_Camera.FrameMove( 0 );
#else	
	g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );
#endif

	//// Setup constant buffers
	//V_RETURN(DXUTCreateBuffer(pd3dDevice,D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_PER_FRAME), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, g_CBCamera));
	//DXUT_DEBUG_NAME(g_CBCamera, "g_pCamCB");

	V_RETURN(DXCreateBuffer(pd3dDevice,D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_CAMEYE_DIST), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, g_CBEyeZdist));
	DXUT_SetDebugName(g_CBEyeZdist, "g_CBEyeZdist");


	CB_LIGHT light;

	XMVECTOR vLightEye    = XMLoadFloat3(&vecLightEye);
	XMVECTOR vLightLookAt = XMLoadFloat3(&vecLightAt);
	XMVECTOR vLightDir = XMVector3Normalize(XMVectorSubtract(vLightEye,vLightLookAt));	
	XMVectorSetW(vLightDir, 0);

	XMFLOAT4 lightDirWS = XMFLOAT4(0,0,1,0);
	XMStoreFloat4(&lightDirWS, vLightDir);
	lightDirWS.w = 0;


	const XMMATRIX& camView = g_Camera.GetViewMatrix();
	
	XMStoreFloat4(&light.lightDirES, XMVector3Normalize(XMVector4Transform(vLightDir, camView)));
	light.lightDirWS = lightDirWS;
	V_RETURN(DXCreateBuffer(pd3dDevice,D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_LIGHT), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, g_CBLight, &light));
	DXUT_SetDebugName(g_CBLight, "g_CBLight");

	pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &g_CBLight );
	pd3dImmediateContext->VSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &g_CBLight );	
	pd3dImmediateContext->DSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &g_CBLight );
	pd3dImmediateContext->HSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &g_CBLight );
	pd3dImmediateContext->GSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &g_CBLight );
	pd3dImmediateContext->PSSetConstantBuffers( CB_LOC::LIGHT_PROPERTIES, 1, &g_CBLight );



	//V_RETURN(DXUTGetD3D11DeviceContext()->QueryInterface( __uuidof(g_perf), reinterpret_cast<void**>(&g_perf) ));

	// init components
	OutputDebugStringW(L"App::Create()\n");
	V_RETURN(g_app.Create(pd3dDevice));

	OutputDebugStringW(L"Create FrameProfiler\n");
	V_RETURN(g_frameProfiler.Create(pd3dDevice));

	OutputDebugStringW(L"Create Voxelizer\n");
	V_RETURN(g_voxelization.Create(pd3dDevice));

	OutputDebugStringW(L"Create AABB Renderer\n");
	V_RETURN(g_rendererBBoxes.Create(pd3dDevice));

	OutputDebugStringW(L"Create Triangle Mesh Renderer\n");
	V_RETURN(g_renderTriMeshes.Create(pd3dDevice));

	OutputDebugStringW(L"Create OSD Renderer\n");
	V_RETURN(g_rendererSubD.Create(pd3dDevice));

	OutputDebugStringW(L"Create Skinning\n");
	V_RETURN(g_skinning.Create(pd3dDevice));

	OutputDebugStringW(L"Create PostPro Pipeline\n");
	V_RETURN(g_postProPipeline.Create(pd3dDevice));

	OutputDebugStringW(L"Create Shadow\n");
	V_RETURN(g_shadow.Create(pd3dDevice));

	OutputDebugStringW(L"Create Memory Manager\n");
	V_RETURN(g_memoryManager.Create(pd3dDevice));

	OutputDebugStringW(L"Create IntersectGPU\n");
	V_RETURN(g_intersectGPU.Create(pd3dDevice));

	OutputDebugStringW(L"Create TileOverlapUpdater\n");
	V_RETURN(g_overlapUpdater.Create(pd3dDevice));
	
//#ifdef NSIGHT_MODE
//	g_shaderManager.SetPrecompiledShaders(false); // for nsight debugging, compile all shaders after this point without using shaders from disk
//#endif
	OutputDebugStringW(L"Create Deformation \n");
	V_RETURN(g_deformation.Create(pd3dDevice));
//#ifdef NSIGHT_MODE
//	g_shaderManager.SetPrecompiledShaders(true); // renable precompile shader usage
//#endif


	g_app.g_osdSubdivider =  new OpenSubdiv::OsdD3D11ComputeController(DXUTGetD3D11DeviceContext());
	
	int numDisplMipMaps = 0;
	int numColorMipMaps = 0;
	
	bool useHalf = false;
	g_memoryManager.InitTileDisplacementMemory(DXUTGetD3D11Device(), g_app.g_memNumDisplacementTiles, g_app.g_displacementTileSize, numDisplMipMaps, 0.0f, true, useHalf, g_app.g_useDisplacementConstraints);
	g_memoryManager.InitTileColorMemory(DXUTGetD3D11Device(), g_app.g_memNumColorTiles, g_app.g_colorTileSize, numColorMipMaps, XMFLOAT3A(0.5, 0.5, 0.5), true);

	
	g_physics = new Physics();
	g_physics->init();
	
	g_scene = new Scene();
	g_scene->SetPhysics(g_physics);
		

		
g_app.g_envMapSRV = g_textureManager.AddTexture("../../media/textures/env_sky.dds");


#define TERRAIN

#ifdef TERRAIN	
	g_far = 1000;
	g_near = 5.f;
	g_LightCamera.SetViewParams(&vecLightEye, &vecLightAt);
	g_LightCamera.SetProjParams( XM_PIDIV4, 1, 20.f, 1000.0f );
	g_LightCamera.FrameMove( 0);

	ModelLoader::Load("media/models/valley/valley.dae", g_scene);
	//ModelLoader::Load("media/models/valley/valley2.dae", g_scene);
	
	//ModelLoader::Load("../../media/models/terrain7.dae", g_scene);
	//ModelLoader::Load("../../media/models/sintel/tundra_level.dae", g_scene, 0, 0);	
	if (g_UseSkydome)
	{
		// Skydome
		g_SkydomeGroup = new ModelGroup();
		ModelLoader::Load("media/models/skydome.dae", g_scene, NULL, g_SkydomeGroup);
	}

	if (g_UseCar) 
	{
		g_Car = new Car();
		g_Car->Create(g_physics, g_scene,
			"media/Models/hummer/Hummer_chassis.dae",  "media/models/hummer/Hummer_wheel.dae",
			g_DumpRecord,
			g_PlayRecord,
			XMFLOAT3(-60,-80,-5));
	}

	g_app.g_envMapSRV = g_textureManager.AddTexture("media/textures/env_snow.dds");

#endif

	if (g_bShowIntersectingOBB) { // || g_showOBBs) {
		g_withPostPro = false;	
	}


	g_physics->GetDynamicsWorld()->setGravity(btVector3(0, 0, -10));


	if(g_physics)	g_physics->stepSimulation(0.02f , 1); // run simulation at least once
	if(g_physics && g_Car) g_Car->Update(g_physics);
	if(g_physics && g_Character) g_Character->move(0.02f);

	for(auto instance : g_scene->GetModelGroups()) instance->UpdateModelMatrix();	// move objects to initial position


	if(g_Car == NULL)
		g_UseCar = false;


	// apply feature adaptive subdivision once
	ApplyAdaptiveSubdivision(pd3dImmediateContext, XMMatrixIdentity(), XMMatrixIdentity());

	// transfer existing displacement maps to analytic
	//TransferExistingDisplacement(pd3dImmediateContext);

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
	TwWindowSize(0, 0); 
	TwTerminate();
	
	// cleanup	
	SAFE_RELEASE(g_pRasterizerStateSolid);
	SAFE_RELEASE(g_pRasterizerStateWiref);

	SAFE_RELEASE(g_CBLight);
	SAFE_RELEASE(g_CBEyeZdist);

	SAFE_RELEASE(g_pBlendStateDefault		);
	SAFE_RELEASE(g_pBlendStateAlpha			);
	SAFE_RELEASE(g_pDepthStencilStateDefault);
	SAFE_RELEASE(g_pDepthStencilStateNoDepth);
	
	g_app.Destroy();
	g_frameProfiler.Destroy();
	g_rendererBBoxes.Destroy();
	g_renderTriMeshes.Destroy();
	
	g_skinning.Destroy();
	g_voxelization.Destroy();
	g_textureManager.Destroy();
	g_shaderManager.Destroy();
	g_Dso.Destroy();
	g_postProPipeline.Destroy();
	g_shadow.Destroy();
	g_rendererSubD.Destroy();
	g_memoryManager.Destroy();
	g_deformation.Destroy();
	g_intersectGPU.Destroy();
	g_overlapUpdater.Destroy();


	delete g_scene;
	delete g_physics;

	SAFE_DELETE(g_SkydomeGroup);
	SAFE_DELETE(g_Car);
	SAFE_DELETE(g_Character);

}

//------------------------------------------------------------ --------------------------
// Render 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device1* pd3dDevice,  ID3D11DeviceContext1* pd3dImmediateContext,  double fTime,  float fElapsedTime,  void* pUserContext )
{
	HRESULT hr = S_OK;

#ifdef _DEBUG
	ReloadShadersDebug();
#endif
	g_frameProfiler.FrameStart(pd3dImmediateContext);

	if (g_UpdateLightDir) {
		XMFLOAT3 eye = *g_LightCamera.GetEyePt();
		XMFLOAT3 lookAt = *g_LightCamera.GetLookAtPt();

		float distance;
		XMStoreFloat(&distance, XMVector3Length(XMVectorSubtract(XMLoadFloat3(&lookAt), XMLoadFloat3(&eye))));
		XMStoreFloat3(&lookAt, XMVectorScale(XMLoadFloat3(&g_LightDir), distance));
		g_LightCamera.SetViewParams(&eye, &lookAt);
		g_LightCamera.SetProjParams(XM_PIDIV4, 1, 20.f, 1000.0f);
		g_LightCamera.FrameMove(0);
	}

	// skinning update	
//#ifdef FRANKIE
	double currentTime = fTime;
	static bool paused = true;
	if(g_AnimatedCharacter)
	{
			

	static double startPauseTime = 0;
	static double accumulatedPauseDuration = 0;
	
	if (g_bTriggerPause) {
		if (paused) 
		{
			accumulatedPauseDuration += currentTime - startPauseTime;
		}
		else 
		{
			startPauseTime = currentTime;
		}
		paused = !paused;
		g_bTriggerPause = false;
	}

	if (paused) 
		currentTime = startPauseTime;		

	currentTime -= accumulatedPauseDuration;
	}
//#endif

	if ((g_PlayRecord || g_DumpRecord) && (g_Car || g_Character)) {
#ifdef TERRAIN
		//fElapsedTime = 1.0f/(40.f);
		fElapsedTime = 1.0f/(500.f);
#endif
	}

	if(g_UseSkinning && g_app.g_bRunSimulation && g_AnimatedCharacter)	
	{	
		if ((g_PlayRecord || g_DumpRecord)) {
			static int frame = -1;
			if (!paused)
				frame++;
			if (frame < 0) frame = 0;
 			currentTime = frame * fElapsedTime;
		}



		for(auto group : g_scene->GetAnimatedModels())
		{ 
			g_skinning.ApplySkinning(pd3dImmediateContext, group, (float) currentTime);
		}
	}
	
	if(g_app.g_bRunSimulation)
	{
		GameControls(fElapsedTime); // keyboard control character or car
	}

	// update pysics and model trafos
	if(g_physics && g_app.g_bRunSimulation)
	{	
		if(g_physics && g_Car)	
			g_Car->Update(g_physics, fElapsedTime);

		g_physics->stepSimulation(fElapsedTime, 10);
		
		for(auto group : g_scene->GetModelGroups() ) 
			group->UpdateModelMatrix();
	}

	g_scene->UpdateAABB(false); // do not update each frame


	// CAM_FOLLOW1
	if(g_CameraSelector == CAM_FOLLOW1 && g_scene->GetModelGroups().size() > 0)
	{		
		const std::vector<XMFLOAT3>& scp = g_scene->GetStadiumCamPositions();
		int N = (int)scp.size();

		
		//XMFLOAT3 vecEye = XMFLOAT3( target.x-50, target.y, target.z+30);
		
		XMFLOAT3 vecEye = scp[g_iCamFollowGroup];//MFLOAT3( target.x-40, target.y, target.z+70);
		vecEye = XMFLOAT3(vecEye.x-100.f, vecEye.y, vecEye.z);

		XMFLOAT3 target = XMFLOAT3(0,0,0);

		if (g_UseCar && g_Car != NULL) {
			target = g_Car->getPosition();
			if (g_PickClosestCamera) 
			{
				// Iterate over all cameras and pick the closest one
				float minDistance = 1000000.0f;
				int currentCameraIndex = -1;
				for (int i = 0; i < N; ++i) 
				{
					XMFLOAT3 currentCameraPosition = scp[i];
					float distance;
					XMStoreFloat(&distance, XMVector3Length(XMVectorSubtract(XMLoadFloat3(&currentCameraPosition), XMLoadFloat3(&target))));
					if(distance < minDistance) 
					{
						minDistance = distance;
						currentCameraIndex = i;
						vecEye = currentCameraPosition;
					}
				}
			}
		
		} else if (g_Character != NULL) {
			btVector3 t = g_Character->getTransform().getOrigin();
			target = XMFLOAT3A(t.x(), t.y(), t.z());
			vecEye = *g_Camera.GetEyePt();
		}

		XMStoreFloat3(&vecEye, XMVectorLerp(XMLoadFloat3(&target), XMLoadFloat3(&vecEye), 0.5f)); // CAR INTRO 0.125f here, others 0.5f or 1.0f
		if (g_withPostPro) 
		{
			float focusLength = 80.0f; // 180.0f
			if (g_UseFollowCam && !g_scene->GetModelGroups().empty())
				XMStoreFloat(&focusLength, XMVector3Length(XMVectorSubtract(XMLoadFloat3(&vecEye), XMLoadFloat3(&target))));
			g_postProPipeline.SetBokehFocus(focusLength);
			//fprintf(stderr, "%f\n", focusLength);
		}
		g_Camera.SetViewParams( &vecEye, &target );	
	}

	// CAM FOLLOW2
	if (g_CameraSelector == CAM_FOLLOW2) {
	
		if (g_Car != NULL) {
			XMFLOAT3 vecEye;
			XMFLOAT3 target;
			XMFLOAT3 p = g_Car->getPosition();
			XMFLOAT3 d = g_Car->getDirection();
			XMVECTOR pV = XMLoadFloat3(&p);
			XMVECTOR dV = XMLoadFloat3(&d);
			
			// Eye and target offsets Z+ is up, Y is Z
			//XMVECTOR eyeOffset = XMVectorSet(0,-16,4,0);
			XMVECTOR heightOffsetTarget = XMVectorSet(0,0,-1.5,0);
			XMVECTOR heightOffsetEye = XMVectorSet(0,0,8.5,0);

			XMStoreFloat3(&target, XMVectorAdd(heightOffsetTarget, XMVectorAdd(pV, XMVectorScale(dV, -2.0f))));
			XMStoreFloat3(&vecEye, XMVectorAdd(heightOffsetEye, XMVectorAdd(pV, XMVectorScale(dV, -14.0f))));

			if (g_withPostPro) {
				float focusLength = 80.0f; // 180.0f
				if (g_UseFollowCam && !g_scene->GetModelGroups().empty())
					XMStoreFloat(&focusLength, XMVector3Length(XMVectorSubtract(XMLoadFloat3(&vecEye), XMLoadFloat3(&target))));
				g_postProPipeline.SetBokehFocus(focusLength);				
			}
			
			g_Camera.SetViewParams( &vecEye, &target );	

		}
	}



	XMMATRIX mView  = g_Camera.GetViewMatrix();	
	const XMMATRIX& mProj = g_Camera.GetProjMatrix();

	
	g_app.SetProjectionMatrix(mProj);
	g_app.SetViewMatrix(mView);
	
	UpdateCamPosZNearFarCB(pd3dImmediateContext);	
	g_app.UpdateCameraCBForModel(pd3dImmediateContext, XMMatrixIdentity());	// set camera constant buffer data
	g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);			// attach constant buffer to shader stages
		
	g_frameProfiler.BeginQuery(pd3dImmediateContext,DXPerformanceQuery::SUBD_KERNEL);
	ApplyAdaptiveSubdivision(pd3dImmediateContext, mView, mProj);
	g_frameProfiler.EndQuery(pd3dImmediateContext,DXPerformanceQuery::SUBD_KERNEL);


	// Clear the render target & depth stencil
	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	pd3dImmediateContext->ClearRenderTargetView( pRTV, g_bUseWhiteBackground ? g_ClearColorWhite : g_ClearColorBlack );				
	pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0 );
	pd3dImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateDefault, 1);
	
	// needed for subd voxelization
	g_app.UpdateGenShadowCBCustom(pd3dImmediateContext, mView, mProj);
	g_app.SetGenShadowConstantBuffersHSDSGS(pd3dImmediateContext);

	ID3D11RasterizerState* rsState;
	pd3dImmediateContext->RSGetState(&rsState);
	
	// Voxel Deformation:
	{
		g_voxelization.SetUseAdaptiveVoxelization(g_bUseAdaptiveVoxelization);

		g_frameProfiler.BeginQuery(pd3dImmediateContext,DXPerformanceQuery::DEFORMATION);
		if (!g_physics || g_app.g_bRunSimulation || g_app.g_bShowVoxelization)
		{
			g_deformationPipeline.DetectDeformableCollisionPairs(g_physics, g_bUsePhysicsCollisionDetection);
			g_deformationPipeline.CheckAndApplyDeformation(pd3dDevice, pd3dImmediateContext);
		}
		
		g_frameProfiler.EndQuery(pd3dImmediateContext,DXPerformanceQuery::DEFORMATION);

		if (g_bShowIntersectingOBB)
		{
			g_app.UpdateCameraCBForModel(pd3dImmediateContext, XMMatrixIdentity());
			g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);

			for (auto& deformationPair : g_deformationPipeline.GetCollisionPairs())
			{
				for (auto& penetratorMap : deformationPair.second)
				{
					ModelInstance* penetrator = penetratorMap.first;
					DXObjectOrientedBoundingBox isctOBB = penetratorMap.second;
					g_rendererBBoxes.RenderOBB(&isctOBB);
				}

			}
		}
		
	}
	
	pd3dImmediateContext->RSSetState(rsState);

	//std::cout << "num voxelized last run " << numVoxelizedLastRun << std::endl;
	//g_perf->EndEvent();

	//g_perf->BeginEvent(L"compute overlap");
	// update tile overlap for recently deformed meshes
	g_frameProfiler.BeginQuery(pd3dImmediateContext,DXPerformanceQuery::OVERLAP);
	for(auto deformableGroup : g_scene->GetModelGroups())
	{
		if(!deformableGroup->HasDeformables()) continue;

		for(auto deformable : deformableGroup->modelInstances)
		{
			if(!deformable->IsDeformable()) continue;
			if(deformable->IsSubD())
			{
				if(deformable->GetOSDMesh()->GetRequiresOverlapUpdate() && g_app.g_withOverlapUpdate)
				{
					g_overlapUpdater.UpdateOverlapDisplacement(pd3dImmediateContext, deformable);
				}
			}
		}
	}
	g_frameProfiler.EndQuery(pd3dImmediateContext,DXPerformanceQuery::OVERLAP);
	//g_perf->EndEvent();

		
	// shadow mapping
	g_app.SetGenShadowConstantBuffersHSDSGS(pd3dImmediateContext); // update light
	g_frameProfiler.BeginQuery(pd3dImmediateContext, DXPerformanceQuery::SHADOW);
	if(g_UseShadowMapping)	
	{		
		
		PERF_EVENT_SCOPED(sm,L"Shadow Map Rendering");

		
		ID3D11RenderTargetView* pMainRTV = NULL;
		ID3D11DepthStencilView* pMainDSV = NULL;
		pd3dImmediateContext->OMGetRenderTargets(1, &pMainRTV, &pMainDSV);
		

		{
			V(g_shadow.RenderShadowMap(pd3dImmediateContext, &g_Camera, &g_LightCamera, g_LightDir, g_scene, &g_renderTriMeshes, &g_rendererSubD));
		}


		// restore viewport
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = float(DXUTGetDXGIBackBufferSurfaceDesc()->Width);
		viewport.Height = float(DXUTGetDXGIBackBufferSurfaceDesc()->Height);
		viewport.MinDepth = D3D11_MIN_DEPTH;
		viewport.MaxDepth = D3D11_MAX_DEPTH;
		pd3dImmediateContext->RSSetViewports(1, &viewport);



		// reset states
		pd3dImmediateContext->OMSetRenderTargets(1, &pMainRTV, pMainDSV);
		SAFE_RELEASE(pMainRTV);
		SAFE_RELEASE(pMainDSV);

		g_app.SetProjectionMatrix(mProj);
		g_app.SetViewMatrix(mView);
		

	}
	g_frameProfiler.EndQuery(pd3dImmediateContext, DXPerformanceQuery::SHADOW);
	pd3dImmediateContext->RSSetState(rsState);
	//pd3dImmediateContext->RSSetState(g_pRasterizerStateSolid);
	SAFE_RELEASE(rsState);

	//g_rendererAdaptive.SetShowWireFrame(false); 
	//pd3dImmediateContext->RSSetState(g_pRasterizerStateSolid);

	if (g_bShowWire)	
	{	
		g_rendererSubD.SetWireframeOverlay(true);
		g_renderTriMeshes.SetWireframeOverlay(true);
		
	} else {
		g_rendererSubD.SetWireframeOverlay(false);
		g_renderTriMeshes.SetWireframeOverlay(false);		
	}
	

	//bool stereoButNotSBS = g_Stereo && !g_SideBySideStereo;

	// postpro
	if(g_withPostPro)// && !stereoButNotSBS) // disable it here when not using sideBySide but stereo.
	{
		pd3dImmediateContext->OMSetBlendState(g_pBlendStateDefault,0, 0xffffffff);

		//g_rendererAdaptive.SetWriteNormalAndDepth(true);
		g_renderTriMeshes.SetUseSSAO(true);		
		g_rendererSubD.SetWithSSAO(true);

		g_postProPipeline.BeginOffscreenRendering(pd3dImmediateContext);
	}

	if(g_UseShadowMapping)
	{
		g_renderTriMeshes.SetUseShadows(true);		
		g_rendererSubD.SetUseShadows(true);

		// reset shadow resource and sampler
		ID3D11ShaderResourceView* ppSRV[] = { g_shadow.GetShadowSRV()};
		ID3D11SamplerState* ppSampler[] = {g_shadow.GetShadowSampler()};

		pd3dImmediateContext->PSSetShaderResources(12, 1, ppSRV);
		pd3dImmediateContext->PSSetSamplers(6, 1, ppSampler);
		g_shadow.UpdateShadowCB(pd3dImmediateContext, g_app.GetViewMatrix(), g_app.GetProjectionMatrix());
	}
	else
	{
		g_shadow.UpdateShadowCB(pd3dImmediateContext, g_app.GetViewMatrix(), g_app.GetProjectionMatrix());
		g_renderTriMeshes.SetUseShadows(false);		
		g_rendererSubD.SetUseShadows(false);
	}

	
	// RENDER SCENE
	{	
		PERF_EVENT_SCOPED(perf,L"Render scene objects");
		RenderSkydome(pd3dImmediateContext);
		RenderSceneObjects(pd3dImmediateContext, g_bShowTriModels);
	}
		

	if(g_UseShadowMapping)
	{		
		g_renderTriMeshes.SetUseShadows(false);		
		g_rendererSubD.SetUseShadows(false);
	}

	//g_perf->EndEvent();
	// postpro
	if(g_withPostPro)// && !(g_Stereo && !stereoButNotSBS)) // disable it here when not using SBS but stereo.
	{		
		
		PERF_EVENT_SCOPED(perf, L"Postprocessing Pipeline");
		//g_perf->BeginEvent(L"postpro");
		pd3dImmediateContext->PSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
		//g_rendererAdaptive.SetWriteNormalAndDepth(false);
		g_renderTriMeshes.SetUseSSAO(false);
		g_rendererSubD.SetWithSSAO(false);

		if(g_UseShadowMapping)
			g_shadow.RunDepthReduction(pd3dImmediateContext, &g_Camera, g_postProPipeline.GetDepthStencilView(), g_postProPipeline.GetDepthStencilViewSRV(), g_postProPipeline.GetWidth(), g_postProPipeline.GetHeight());

		g_postProPipeline.EndOffscreenRendering(pd3dImmediateContext);

		g_frameProfiler.BeginQuery(pd3dImmediateContext,DXPerformanceQuery::POSTPRO);
		g_postProPipeline.ApplySSAO(pd3dImmediateContext);		
		g_frameProfiler.EndQuery(pd3dImmediateContext,DXPerformanceQuery::POSTPRO);
		//g_perf->EndEvent();
	}
	else
	{		
		// depth reduction on main depth buffer
		if(g_UseShadowMapping)
			g_shadow.RunDepthReduction(pd3dImmediateContext, &g_Camera, DXUTGetD3D11DepthStencilView(), DXUTGetD3D11DepthStencilViewSRV(), DXUTGetDXGIBackBufferSurfaceDesc()->Width, DXUTGetDXGIBackBufferSurfaceDesc()->Height);
	}


	// Render Control Level Control Cage
	if(g_bShowControlMesh)
	{		
		PERF_EVENT_SCOPED(perf, L"Render control cages");
		for(auto group : g_scene->GetModelGroups())
		{
			g_app.UpdateCameraCBForModel(pd3dImmediateContext, group->GetModelMatrix());
			g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);
			for(auto instance : group->modelInstances)
			{
				if(!instance->IsSubD()) continue;
				g_rendererBBoxes.RenderControlCage(pd3dImmediateContext, instance);
			}
		}
	}

	// Show OBBs
	if (g_bShowOBBs)
	{		
		PERF_EVENT_SCOPED(perf, L"Render oriented bounding boxes");
		g_app.UpdateCameraCBForModel(pd3dImmediateContext, XMMatrixIdentity());
		g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);

		for(auto group : g_scene->GetModelGroups())
		{
			for( auto instance : group->modelInstances)
			{
				g_rendererBBoxes.RenderOBB(&instance->GetOBBWorld());
			}
		}
	}
	

	if(g_showPhysics)
	{		
		PERF_EVENT_SCOPED(perf, L"Render physics");
		g_app.UpdateCameraCBForModel(pd3dImmediateContext, XMMatrixIdentity());
		g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);
		g_rendererBBoxes.RenderPhysicsAABB(g_scene);

		if(g_Car)
		{
			for(int i = 0; i < 4; ++i)
				g_rendererBBoxes.RenderPhysicsAABB(g_Car->GetCarWheels(i));			
		}
	}

	// debug voxelize all objects
	if(g_bVoxelizeAllObjects)
	{			
		PERF_EVENT_SCOPED(perf, L"Voxelize all objects");
		for(auto group : g_scene->GetModelGroups())
		{
			for(auto model : group->modelInstances)
			{
				//if(model->IsDeformable()) continue;
				//if(model->IsDeformable() && !model->IsCollider()) continue;

				// voxelize collider
				g_voxelization.StartVoxelizeSolid(pd3dImmediateContext, model, model->GetOBBWorld(), false);

				g_app.UpdateCameraCBCustom(pd3dImmediateContext, model->GetVoxelGridDefinition().m_MatrixWorldToVoxelProj, XMMatrixIdentity());
				g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);

				if(model->IsSubD())
				{
					g_rendererSubD.Voxelize(pd3dImmediateContext, model, false);
				}
				else
				{
					g_renderTriMeshes.Voxelize(pd3dImmediateContext, model);
				}

				g_voxelization.EndVoxelizeSolid(pd3dImmediateContext);
			}
		}
	}

	// render voxelization
	if(g_app.g_bShowVoxelization )
	{		
		pd3dImmediateContext->RSSetState(g_pRasterizerStateSolid);
		g_app.UpdateCameraCBForModel(pd3dImmediateContext, XMMatrixIdentity());
		g_app.SetCameraConstantBuffersAllStages(pd3dImmediateContext);

		for(auto group: g_scene->GetModelGroups())
		{
			for(auto instance : group->modelInstances)
			{
				if (!instance->GetVoxelGridDefinition().m_OOBB.IsValid()) continue;
								
				g_voxelization.RenderVoxelizationViaRaycasting(pd3dImmediateContext, instance, g_Camera.GetEyePt());
			}			
		}

		pd3dImmediateContext->OMSetBlendState(g_pBlendStateDefault,0, 0xffffffff);
		pd3dImmediateContext->OMSetDepthStencilState(g_pDepthStencilStateDefault, 1);
	}



	if(g_UseShadowMapping && g_ShowShadowDebug)
	{				
		ID3D11ShaderResourceView* ppSRV[] = { g_shadow.GetShadowSRV()};
		ID3D11SamplerState* ppSampler[]   = {g_shadow.GetShadowSampler()};

		g_shadow.DebugRenderCascade(pd3dImmediateContext, DXUTGetD3D11RenderTargetView());

		ID3D11RenderTargetView* pMainRTV = DXUTGetD3D11RenderTargetView();		
		pd3dImmediateContext->OMSetRenderTargets(1, &pMainRTV, DXUTGetD3D11DepthStencilView() );
		
	}

	ID3D11ShaderResourceView* nv[] = { NULL};
	ID3D11SamplerState* ns[] = {NULL};
	pd3dImmediateContext->PSSetShaderResources(12, 1, nv);
	pd3dImmediateContext->PSSetSamplers(6, 1, ns);


	if (g_dumpFrames ) {
		char buf[1024];
		sprintf(buf, "%s/deformation.%05d.png", g_dumpDirectory.c_str(), g_dumpFrame);
		if (g_dumpDebug) fprintf(stderr, "%s\n", buf);			
		DXUTSnapD3D11Screenshot(string2wstring(std::string(buf)).c_str());
		g_dumpFrame++;
	}

	g_frameProfiler.BeginQuery(pd3dImmediateContext, DXPerformanceQuery::GUI);
	// Render the HUD
	if (g_bShowGUI == 1) {		
		PERF_EVENT_SCOPED(perf, L"GUI DXUT");
		DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
		RenderText();
		DXUT_EndPerfEvent();
	} else if (g_bShowGUI == 2) {
		//TODO
		//RenderFPSTime();
	}

	if (g_ShowTweakBar)
	{
		PERF_EVENT_SCOPED(perf, L"GUI Anttweakbar");		
		TwDraw();
	}
	g_frameProfiler.EndQuery(pd3dImmediateContext, DXPerformanceQuery::GUI);
	g_frameProfiler.FrameEnd(pd3dImmediateContext);

	if(g_app.g_bTimingsEnabled)
	{
		g_app.g_TimingLog.printTimings();
		g_app.g_TimingLog.resetTimings();
	}

	if(g_app.g_useCompactedVisibilityOverlap)
	{	
		for(auto groups : g_scene->GetModelGroups())
		{
			//if(!g->_containsDeformables) continue;

			for (auto instance : groups->modelInstances)
			{
				if(instance->IsDeformable())
					g_overlapUpdater.ClearIntersectAllBuffer(pd3dImmediateContext, instance);
			}		
		}
	}
	
	//g_app.g_doEdit = false;
}



//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device1* pd3dDevice,
										  IDXGISwapChain1* pSwapChain,
										  const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
										  void* pUserContext )
{
	// Setup the camera's projection parameters    
	float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;	
	
	g_Camera.SetProjParams( XM_PIDIV4, fAspectRatio, g_near, g_far );

#ifdef FIRSTPERSON

#else
	g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
#endif
	float fNear = g_Camera.GetNearClip();
	float fFar	= g_Camera.GetFarClip();

	std::cout << "fnear, far " << fNear << ", " << fFar << std::endl;
#ifdef FROG_SHOWROOM
	g_Camera.SetProjParams( XM_PI/3.0f, fAspectRatio, fNear, g_far );
#else
	g_Camera.SetProjParams( XM_PIDIV4, fAspectRatio, fNear, fFar);
#endif
	//g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
	//g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

	//////////////////////////////////////////////////////////////////////////////////////////
	// set camera eye and zdist, @maddi eye is not used? when are the clip planes changed, besides load camera? 
	// can we move zdist update to init and load camera
	g_zFNDist = g_Camera.GetFarClip() - g_Camera.GetNearClip();

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	ID3D11DeviceContext1* pd3dImmediateContext =  DXUTGetD3D11DeviceContext();
	pd3dImmediateContext->Map( g_CBEyeZdist, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_CAMEYE_DIST* pCBEyeZdist = ( CB_CAMEYE_DIST* )MappedResource.pData;		
	//pCBEyeZdist->eye = XMFLOAT3A(g_Camera.GetEyePt()->x, g_Camera.GetEyePt()->y,g_Camera.GetEyePt()->z);
	pCBEyeZdist->eye = *g_Camera.GetEyePt();//XMFLOAT3A(g_Camera.GetEyePt()->x, g_Camera.GetEyePt()->y,g_Camera.GetEyePt()->z);
	pCBEyeZdist->zfzn = g_zFNDist;
	pCBEyeZdist->znear =fNear;
	pCBEyeZdist->zfar = fFar;
	pd3dImmediateContext->Unmap( g_CBEyeZdist, 0 );



	pd3dImmediateContext->CSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
	pd3dImmediateContext->VSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );	
	pd3dImmediateContext->DSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
	pd3dImmediateContext->HSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
	pd3dImmediateContext->GSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
	pd3dImmediateContext->PSSetConstantBuffers( CB_LOC::CAM_EYE_AND_ZDIST, 1, &g_CBEyeZdist );
	//////////////////////////////////////////////////////////////////////////////////////////


	g_postProPipeline.Resize(DXUTGetD3D11Device(), pBackBufferSurfaceDesc);
	TwWindowSize(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);
	// notify other components
	//V_RETURN(g_renderStd.resize(pBackBufferSurfaceDesc));
	


	return S_OK;
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{	
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	// Update the camera's position based on user input 
	g_Camera.FrameMove( fElapsedTime );
}



//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------

void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{	
	UINT max_level = 0;

	bool shift = GetAsyncKeyState(VK_SHIFT) != 0; 

	if( bKeyDown ) {
		switch( nChar )
		{
		
		case VK_UP:
			if (!shift) {
				LoadCamera(g_CurrentCameraFile);
			} else {
				LoadChar(g_CurrentCharFile);
			}
		break;

		case VK_DOWN:
			if (!shift) {
				StoreCamera(g_CurrentCameraFile);
			} else {
				StoreChar(g_CurrentCharFile);
			}
		break;

		case VK_F1:			// WIREFRAME
			g_bShowWire = !g_bShowWire; 
			break;
			
		case VK_F4:
			g_withPostPro = !g_withPostPro;
			break;
		case VK_F5:
			g_ShowTweakBar = !g_ShowTweakBar;
			break;
		case VK_F6:
			g_UseShadowMapping = !g_UseShadowMapping;
			break;
		case VK_F7:
			g_ShowShadowDebug = !g_ShowShadowDebug;
			break;
		case VK_F8:
			g_UseFollowCam = !g_UseFollowCam;
			if (g_UseFollowCam) g_CameraSelector = CAM_FOLLOW1;
			break;

		case VK_F9:
			{			
			HRESULT hr = DXUTSnapD3D11Screenshot(L"../../screenshot.png");
			V(hr);
			}
			break;
		case VK_F10:
			
				if (!g_dumpFrames) {
					SYSTEMTIME time;
					GetSystemTime(&time);
					char buf[1024];
					sprintf(buf, "dump_%02d-%02d-%04d_%02d-%02d-%02d", time.wMonth, time.wDay, time.wYear, time.wHour, time.wMinute, time.wSecond);
					g_dumpDirectory = std::string(buf);
					CreateDirectory(string2wstring(g_dumpDirectory).c_str(),  NULL);
				}
				g_dumpFrames = !g_dumpFrames;
				g_dumpFrame = 0;			
			break;
		case VK_F11:
			g_bShowTriModels = !g_bShowTriModels;
			break;

#ifdef OLD_FOLLOW_CAM
		case VK_LEFT:
			if(g_iCamFollowGroup==0) g_iCamFollowGroup = (UINT)(g_scene->GetModelGroups().size() -1);
			else
			{
				g_iCamFollowGroup = (g_iCamFollowGroup-1)%g_scene->GetModelGroups().size();
			}
			break;
		
		case VK_RIGHT:
			if(g_iCamFollowGroup==g_scene->GetModelGroups().size() -1) g_iCamFollowGroup = 0;
			else
					g_iCamFollowGroup = (g_iCamFollowGroup+1)%g_scene->GetModelGroups().size();			
			break;
#else
		case VK_LEFT:
			if(g_iCamFollowGroup==0) g_iCamFollowGroup = (UINT)(g_scene->GetStadiumCamPositions().size() -1);
			else
			{
				g_iCamFollowGroup = (g_iCamFollowGroup-1)%g_scene->GetStadiumCamPositions().size();
			}
			break;
		
		case VK_RIGHT:
			if(g_iCamFollowGroup==g_scene->GetStadiumCamPositions().size() -1) g_iCamFollowGroup = 0;
			else
					g_iCamFollowGroup = (g_iCamFollowGroup+1)%g_scene->GetStadiumCamPositions().size();			
			break;
#endif

		case '\t':	
			g_bShowGUI = (g_bShowGUI+1)%3;
			break;
		case 'N':
			{
				g_app.g_editMode = (g_app.g_editMode+1)%4; //0 none, 1 sculting, 2 painting
			}
			break;

		case 'V':
			g_app.g_bShowVoxelization = !g_app.g_bShowVoxelization;
			break;

		case 0x4B:		// k , j = 0x4a
			g_ShowCascadeIdx = (g_ShowCascadeIdx+1)%(NUM_CASCADES+1);
			break;

		case VK_SPACE:
			g_app.g_bRunSimulation = !g_app.g_bRunSimulation;	
			g_bTriggerPause = true;
			break;
		case UINT('R'):	
			std::cout << "Reload Shaders" << std::endl;
			if(FAILED(g_shaderManager.CheckAndReload()))
				std::cout << "shader errors" << std::endl;
			break;
		case UINT('T'):	
			std::cout << "Reset" << std::endl;	
			g_app.g_bTimingsEnabled = !g_app.g_bTimingsEnabled;
			break;

		case UINT('P'):
			std::cout << "screenshot " << std::endl;
			DXUTSnapD3D11Screenshot(string2wstring(std::string("test.png")).c_str());
			break;
		case UINT('M'):			
			g_app.g_showAllocated = !g_app.g_showAllocated;			
			break;
		case UINT('J'):
			g_visualizeCascades = !g_visualizeCascades;
			g_shadow.SetShowCascades(g_visualizeCascades);
			break;

		case UINT('I'):
			g_app.g_showIntersections  =  !g_app.g_showIntersections;			
			break;			
		case UINT('H'):
			{
				std::cout << "clear displacement" << std::endl;
				ClearDisplacementTiles(DXUTGetD3D11DeviceContext());
			}
			break;
		}
	}
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
HRESULT InitGUI()
{
	// ANT TWEAKBAR
	
		TwInit(TW_DIRECT3D11, DXUTGetD3D11Device());

		TwBar* mainBar = TwNewBar("Main");
		TwDefine("Main position='0 0' size='200 500'");

		TwAddVarRW(mainBar, "lightDir", TW_TYPE_DIR3F, &g_LightDir, "label = 'lightdir' group='Scene'");

		// scene settings
		TwAddVarCB(mainBar, "fps", TW_TYPE_FLOAT, (TwSetVarCallback)[](const void *value, void* clientData){},
			(TwGetVarCallback)[](void *value, void*){*static_cast<float*>(value) = DXUTGetFPS(); }, NULL, "label = 'fps' group='Scene'");
		TwAddVarRW(mainBar, "tess factor", TW_TYPE_FLOAT, (float*)&(g_app.g_fTessellationFactor), "min=1 max=64 step=0.2 group='Scene'");

		//g_app.g_adaptiveTessellation
		TwAddVarCB(mainBar, "adaptive tess", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_adaptiveTessellation = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_adaptiveTessellation; }, NULL, "label = 'adaptive tessellation' group='Scene'");

		TwAddVarRW(mainBar, "displacement scaler", TW_TYPE_FLOAT, (float*)&(g_app.g_fDisplacementScalar), "min=0 max=5 step=0.1 group='Scene'");

		typedef enum { SUMMER, FALL, WINTER, SPRING } Seasons;
		//Seasons season = WINTER;
		TwType cameraType = TwDefineEnum("CameraType", NULL, 0);

		TwAddVarRW(mainBar, "camera type", cameraType, &g_CameraSelector, " enum='0 {ArcBall}, 1 {Follow Car Closest}, 2 {Follow Car}' group='Scene'");

		TwAddVarCB(mainBar, "simulation", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_bRunSimulation = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_bRunSimulation; }, NULL, "label = 'run physics'");

		// deformation
		TwAddVarRW(mainBar, "voxelscaler", TW_TYPE_FLOAT, (float*)&(g_app.g_adaptiveVoxelizationScale), "min=1 max=100 step=0.5 label='voxel scaler' group='Deformation'");
		TwAddVarCB(mainBar, "constraints", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_useDisplacementConstraints = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_useDisplacementConstraints; }, NULL, "label = 'solve constraints' group='Deformation'");
		TwAddVarCB(mainBar, "VoxelMultisampling", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_voxelDeformationMultiSampling = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_voxelDeformationMultiSampling; }, NULL, "label = 'voxel multisample' group='Deformation'");
		TwAddVarCB(mainBar, "VoxelJittering", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_withVoxelJittering = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_withVoxelJittering; }, NULL, "label = 'voxel jittering' group='Deformation'");
		TwAddVarCB(mainBar, "VoxelRotate", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_withVoxelOBBRotate = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_withVoxelOBBRotate; }, NULL, "label = 'voxel obb rotate' group='Deformation'");
		TwAddVarCB(mainBar, "CompactedOverlap", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_useCompactedVisibilityOverlap = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_useCompactedVisibilityOverlap; }, NULL, "label = 'compacted overlap' group='Deformation'");
		TwAddVarCB(mainBar, "CulledRaycast", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_useCullingForRayCast = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_useCullingForRayCast; }, NULL, "label = 'culling raycast' group='Deformation'");
		TwAddVarCB(mainBar, "OverlapUpdate", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_withOverlapUpdate = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_withOverlapUpdate; }, NULL, "label = 'update overlap' group='Deformation'");


		// debug vis
		TwAddVarCB(mainBar, "ShowCage", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_bShowControlMesh = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_bShowControlMesh; }, NULL, "label = 'control cage' group='Vis'");

		TwAddVarCB(mainBar, "OBBs", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_bShowIntersectingOBB = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_bShowIntersectingOBB; }, NULL, "label = 'isct OBBs' group='Vis'");

		TwAddVarCB(mainBar, "visphysics", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_showPhysics = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_showPhysics; }, NULL, "label = 'physics' group='Vis'");

		TwAddVarCB(mainBar, "visvoxel", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_bShowVoxelization = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_bShowVoxelization; }, NULL, "label = 'voxel' group='Vis'");
		TwAddVarCB(mainBar, "showisct", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_showIntersections = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_showIntersections; }, NULL, "label = 'intersections' group='Vis'");
		TwAddVarCB(mainBar, "showallocs", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_app.g_showAllocated = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_app.g_showAllocated; }, NULL, "label = 'allocs' group='Vis'");





		// shadows		
		TwAddVarCB(mainBar, "ShadowsSwitch", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_UseShadowMapping = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_UseShadowMapping; }, NULL, "label = 'Enable' group='Shadows'");
		TwAddVarCB(mainBar, "ShowCascades", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_visualizeCascades = *static_cast<const bool *>(value); g_shadow.SetShowCascades(g_visualizeCascades); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_visualizeCascades; }, NULL, "label = 'Show Cascades' group='Shadows'");

		TwAddVarCB(mainBar, "SinglePass", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_shadow.GetUseSinglePassShadowRef() = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_shadow.GetUseSinglePassShadowRef(); }, NULL, "label = 'Single Pass' group='Shadows'");
		TwAddVarCB(mainBar, "AutoCascades", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_shadow.GetUseAutomaticCascadesRef() = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_shadow.GetUseAutomaticCascadesRef(); }, NULL, "label = 'Auto Depth Range' group='Shadows'");

		TwAddVarRO(mainBar, "DepthMin", TW_TYPE_FLOAT, &g_shadow.GetDepthMinMaxRef().x, "label = 'minDepth' group='Shadows'");
		TwAddVarRO(mainBar, "DepthMax", TW_TYPE_FLOAT, &g_shadow.GetDepthMinMaxRef().y, "label = 'maxDepth' group='Shadows'");

		TwAddVarCB(mainBar, "BlurCS", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_shadow.GetUseBlurCSRef() = *static_cast<const bool *>(value); },
			(TwGetVarCallback)[](void *value, void*){ *static_cast<bool*>(value) = g_shadow.GetUseBlurCSRef();	}, NULL, "label = 'Blur CS' group='Shadows'");

		TwAddVarCB(mainBar, "BlurSharedCS", TW_TYPE_BOOLCPP, (TwSetVarCallback)[](const void *value, void* clientData){g_shadow.GetUseBlurSharedMemCSRef() = *static_cast<const bool *>(value);	},
			(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) = g_shadow.GetUseBlurSharedMemCSRef();	}, NULL, "label = 'Blur SharedMem CS' group='Shadows'");

		//TwAddButton(shadowbar, "BlurCS", (TwButtonCallback)(void*)[]{g_shadow.SetUseBlurCS(!g_shadow.GetUseBlurCS());}, NULL, "label='Blur CS'");
		//TwAddButton(shadowbar, "BlurSharedCS", (TwButtonCallback)(void*)[]{g_shadow.SetUseBlurSharedMemCS(!g_shadow.GetUseBlurSharedMemCS());}, NULL, "label='Blur Shared Mem CS'");

		g_frameProfiler.InitGUI();

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle windows messages
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
						 void* pUserContext )
{
	// AntTweakbar
	if (g_ShowTweakBar) {
		if(TwEventWin(hWnd, uMsg, wParam, lParam)) {
			return 0;
		}
	}

	g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);
		
	bool update = false;
	if ((uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK))
	{
		// start follow cam on right mouse button
		g_UseFollowCam = true;
		update = true;

	}
	else if ((uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK))
	{
		// stop follow cam on mouse action	
		g_UseFollowCam = false;
		g_CameraSelector = CAM_ARCBALL;
		update = true;
	}

	if (update && g_UseFollowCam) g_CameraSelector = CAM_FOLLOW1;
	

	return 0;

}

int __cdecl main() {
	wWinMain(GetModuleHandle(0), 0, NULL, TRUE);
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	HRESULT hr;
	V_RETURN(DXUTSetMediaSearchPath(L"..\\media"));

	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	// break on specific alloc
	//_CrtSetBreakAlloc(925515);
#endif

	

	CoInitializeEx(NULL, COINIT_MULTITHREADED);	

	// DXUT callbacks	
	DXUTSetCallbackDeviceChanging(	ModifyDeviceSettings	);
	DXUTSetCallbackMsgProc(			MsgProc					);
	DXUTSetCallbackFrameMove(		OnFrameMove				);
	DXUTSetCallbackKeyboard( OnKeyboard );

	DXUTSetCallbackD3D11DeviceAcceptable(	IsD3D11DeviceAcceptable		);
	DXUTSetCallbackD3D11DeviceCreated(		OnD3D11CreateDevice			);
	DXUTSetCallbackD3D11SwapChainResized(	OnD3D11ResizedSwapChain		);
	DXUTSetCallbackD3D11FrameRender(		OnD3D11FrameRender			);
	DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain	);
	DXUTSetCallbackD3D11DeviceDestroyed(	OnD3D11DestroyDevice		);
	
	ApplyGlobalSettings();		

	DXUTInit( true, true);
	DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
	DXUTCreateWindow( L"DX11 Deformation" );
	

	if (g_UseMSAABackBuffer) {
		DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1280, 720, 8, 31 );  // create with msaa
	} else {
		DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1280, 720 );
	}

	InitGUI();

	//DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 800, 600 );
	DXUTMainLoop(); // Enter into the DXUT render loop

	return DXUTGetExitCode();
}
