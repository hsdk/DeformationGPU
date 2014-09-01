//--------------------------------------------------------------------------------------
// File: DXUT.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef DXUT_H
#define DXUT_H

#include <DXConfig.h>
			

#ifndef UNICODE
#error "DXUT requires a Unicode build. See the nearby comments for details"
//
// If you are using Microsoft Visual C++ .NET, under the General tab of the project 
// properties change the Character Set to 'Use Unicode Character Set'.  
//
// Windows XP and later are native Unicode so Unicode applications will perform better.  
// For Windows 98 and Windows Me support, consider using the Microsoft Layer for Unicode (MSLU).  
//
// To use MSLU, link against a set of libraries similar to this
//      /nod:kernel32.lib /nod:advapi32.lib /nod:user32.lib /nod:gdi32.lib /nod:shell32.lib /nod:comdlg32.lib /nod:version.lib /nod:mpr.lib /nod:rasapi32.lib /nod:winmm.lib /nod:winspool.lib /nod:vfw32.lib /nod:secur32.lib /nod:oleacc.lib /nod:oledlg.lib /nod:sensapi.lib UnicoWS.lib kernel32.lib advapi32.lib user32.lib gdi32.lib shell32.lib comdlg32.lib version.lib mpr.lib rasapi32.lib winmm.lib winspool.lib vfw32.lib secur32.lib oleacc.lib oledlg.lib sensapi.lib dxerr.lib dxguid.lib d3dx9d.lib d3d9.lib comctl32.lib
// and put the unicows.dll (available for download from msdn.microsoft.com) in the exe's folder.
// 
// For more details see the MSDN article titled:
// "MSLU: Develop Unicode Applications for Windows 9x Platforms with the Microsoft Layer for Unicode"
// at http://msdn.microsoft.com/msdnmag/issues/01/10/MSLU/default.aspx 
//
#endif

#ifndef STRICT
#define STRICT
#endif

//// If app hasn't choosen, set to work with Windows Vista and beyond
//#ifndef WINVER
//#define WINVER         0x0600
//#endif
//#ifndef _WIN32_WINDOWS
//#define _WIN32_WINDOWS 0x0600
//#endif
//
//// fix for windows 7 loading
//#ifdef FORCE_DIRECTX_11_0
//#undef _WIN32_WINNT
//#define _WIN32_WINNT 0x0601
//#else
//#undef _WIN32_WINNT
//#define _WIN32_WINNT 0x0602
//#endif
//#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/) && !defined(DXGI_1_2_FORMATS)
//#define DXGI_1_2_FORMATS
//#endif

// #define DXUT_AUTOLIB to automatically include the libs needed for DXUT 
#ifdef DXUT_AUTOLIB
#pragma comment( lib, "dxerr.lib" )
#pragma comment( lib, "dxguid.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
//#if defined(DEBUG) || defined(_DEBUG)
//#pragma comment( lib, "d3d11d.lib" )
//#else
#pragma comment( lib, "d3d11.lib" )
//#endif
#pragma comment( lib, "winmm.lib" )
#pragma comment( lib, "comctl32.lib" )
#endif

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds
#pragma warning( disable : 4481 )

// Enable extra D3D debugging in debug builds if using the debug DirectX runtime.  
// This makes D3D objects work well in the debugger watch window, but slows down 
// performance slightly.
#if defined(DEBUG) || defined(_DEBUG)
#ifndef D3D_DEBUG_INFO
#define D3D_DEBUG_INFO
#endif
#endif

// Standard Windows includes
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#include <initguid.h>
#include <assert.h>
//#include <wchar.h>
//#include <mmsystem.h>
#include <commctrl.h> // for InitCommonControls() 
#include <shellapi.h> // for ExtractIcon()
#include <new.h>      // for placement new
#include <shlobj.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>

// CRT's memory leak detection
#if defined(DEBUG) || defined(_DEBUG)
#include <crtdbg.h>
#endif

// Direct3D11 includes
#include <d3dcommon.h>
#include <dxgi.h>

//#ifndef FORCE_DIRECTX_11_0
////#if(NTDDI_VERSION >= NTDDI_WIN8)
//	#include <d3d11_1.h>
//	#ifndef FORCE_DIRECTX_11_0
//	#define DIRECTX_11_1_ENABLED
//	#endif
////#endif
//#endif
//
//#ifdef FORCE_DIRECTX_11_0
//	#include <d3d11.h>
//	typedef ID3D11Device		ID3D11Device1;
//	typedef ID3D11DeviceContext ID3D11DeviceContext1;
//	typedef IDXGISwapChain		IDXGISwapChain1;
//	typedef IDXGIFactory1		IDXGIFactory2;
//#endif

#include <dxgidebug.h>
#include <d3dCompiler.h>
//#include <d3dcsx.h>

#include <DirectXMath.h>
#include <DirectXColors.h>
#include <DirectXPackedVector.h>

	// WIC includes
	// VS 2010's stdint.h conflicts with intsafe.h
#pragma warning(push)
#pragma warning(disable : 4005)
#include <wincodec.h>
#pragma warning(pop)

	// XInput includes
#include <xinput.h>

	// HRESULT translation for Direct3D and other APIs 
#include "dxerr.h"

	// STL includes
#include <algorithm>
#include <memory>
#include <vector>

#include <wrl.h>

#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x)           { hr = (x); if( FAILED(hr) ) { DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#else
#ifndef V
#define V(x)           { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=nullptr; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p)=nullptr; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

#if defined(DEBUG) || defined(_DEBUG)

#ifndef DXUT_DEBUG_NAME_GI
#define DXUT_DEBUG_NAME_GI(x, y)           { DXUTSetDebugObjectNameDXGI(x,y, __FILE__, (DWORD)__LINE__); }
#endif

#ifndef DXUT_DEBUG_NAME
#define DXUT_DEBUG_NAME(x,y)              { DXUT_SetDebugName(x,y, __FILE__, (DWORD)__LINE__); }
#endif
#else
#ifndef DXUT_DEBUG_NAME_GI
#define DXUT_DEBUG_NAME_GI(x, y)           {  }
#endif

#ifndef DXUT_DEBUG_NAME
#define DXUT_DEBUG_NAME(x,y)              {  }
#endif
#endif

#ifndef D3DCOLOR_ARGB
#define D3DCOLOR_ARGB(a,r,g,b) \
    ((DWORD)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#endif

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------

struct DXUTD3D11DeviceSettings
{
    UINT AdapterOrdinal;
    D3D_DRIVER_TYPE DriverType;
    UINT Output;
#ifdef DIRECTX_11_1_ENABLED
	DXGI_SWAP_CHAIN_DESC1 sd;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC sdfs;
#else
    DXGI_SWAP_CHAIN_DESC sd;
#endif
    UINT32 CreateFlags;
    UINT32 SyncInterval;
    DWORD PresentFlags;
    bool AutoCreateDepthStencil; // DXUT will create the depth stencil resource and view if true
    DXGI_FORMAT AutoDepthStencilFormat;
    D3D_FEATURE_LEVEL DeviceFeatureLevel;
};

enum DXUTDeviceVersion
{    
    DXUT_D3D11_DEVICE
};

struct DXUTDeviceSettings
{    
    D3D_FEATURE_LEVEL MinimumFeatureLevel;    
    DXUTD3D11DeviceSettings d3d11; // only valid if ver == DXUT_D3D11_DEVICE
};


//--------------------------------------------------------------------------------------
// Error codes
//--------------------------------------------------------------------------------------
#define DXUTERR_NODIRECT3D              MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0901)
#define DXUTERR_NOCOMPATIBLEDEVICES     MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0902)
#define DXUTERR_MEDIANOTFOUND           MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0903)
#define DXUTERR_NONZEROREFCOUNT         MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0904)
#define DXUTERR_CREATINGDEVICE          MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0905)
#define DXUTERR_RESETTINGDEVICE         MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0906)
#define DXUTERR_CREATINGDEVICEOBJECTS   MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0907)
#define DXUTERR_RESETTINGDEVICEOBJECTS  MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0908)
#define DXUTERR_DEVICEREMOVED           MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x090A)
#define DXUTERR_NODIRECT3D11            MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x090)

//--------------------------------------------------------------------------------------
// Callback registration 
//--------------------------------------------------------------------------------------

// General callbacks
typedef void    (CALLBACK *LPDXUTCALLBACKFRAMEMOVE)(_In_ double fTime, _In_ float fElapsedTime, _In_opt_ void* pUserContext);
typedef void    (CALLBACK *LPDXUTCALLBACKKEYBOARD)(_In_ UINT nChar, _In_ bool bKeyDown, _In_ bool bAltDown, _In_opt_ void* pUserContext);
typedef void    (CALLBACK *LPDXUTCALLBACKMOUSE)(_In_ bool bLeftButtonDown, _In_ bool bRightButtonDown, _In_ bool bMiddleButtonDown,
	_In_ bool bSideButton1Down, _In_ bool bSideButton2Down, _In_ int nMouseWheelDelta,
	_In_ int xPos, _In_ int yPos, _In_opt_ void* pUserContext);
typedef LRESULT(CALLBACK *LPDXUTCALLBACKMSGPROC)(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam,
	_Out_ bool* pbNoFurtherProcessing, _In_opt_ void* pUserContext);
typedef void    (CALLBACK *LPDXUTCALLBACKTIMER)(_In_ UINT idEvent, _In_opt_ void* pUserContext);
typedef bool    (CALLBACK *LPDXUTCALLBACKMODIFYDEVICESETTINGS)(_In_ DXUTDeviceSettings* pDeviceSettings, _In_opt_ void* pUserContext);
typedef bool    (CALLBACK *LPDXUTCALLBACKDEVICEREMOVED)(_In_opt_ void* pUserContext);

class CD3D11EnumAdapterInfo;
class CD3D11EnumDeviceInfo;

// Direct3D 11 callbacks
typedef bool    (CALLBACK *LPDXUTCALLBACKISD3D11DEVICEACCEPTABLE)( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );

#ifdef DIRECTX_11_1_ENABLED
typedef HRESULT (CALLBACK *LPDXUTCALLBACKD3D11DEVICECREATED)( ID3D11Device1* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
typedef HRESULT (CALLBACK *LPDXUTCALLBACKD3D11SWAPCHAINRESIZED)( ID3D11Device1* pd3dDevice, IDXGISwapChain1 *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKD3D11FRAMERENDER)( ID3D11Device1* pd3dDevice, ID3D11DeviceContext1* pd3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext );
#else
typedef HRESULT (CALLBACK *LPDXUTCALLBACKD3D11DEVICECREATED)( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
typedef HRESULT (CALLBACK *LPDXUTCALLBACKD3D11SWAPCHAINRESIZED)( ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKD3D11FRAMERENDER)( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext );
#endif
typedef void    (CALLBACK *LPDXUTCALLBACKD3D11SWAPCHAINRELEASING)( void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKD3D11DEVICEDESTROYED)( void* pUserContext );

// General callbacks
void WINAPI DXUTSetCallbackFrameMove( LPDXUTCALLBACKFRAMEMOVE pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackKeyboard( LPDXUTCALLBACKKEYBOARD pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackMouse( LPDXUTCALLBACKMOUSE pCallback, bool bIncludeMouseMove = false, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackMsgProc( LPDXUTCALLBACKMSGPROC pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackDeviceChanging( LPDXUTCALLBACKMODIFYDEVICESETTINGS pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackDeviceRemoved( LPDXUTCALLBACKDEVICEREMOVED pCallback, void* pUserContext = NULL );

// Direct3D 11 callbacks
void WINAPI DXUTSetCallbackD3D11DeviceAcceptable( LPDXUTCALLBACKISD3D11DEVICEACCEPTABLE pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D11DeviceCreated( LPDXUTCALLBACKD3D11DEVICECREATED pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D11SwapChainResized( LPDXUTCALLBACKD3D11SWAPCHAINRESIZED pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D11FrameRender( LPDXUTCALLBACKD3D11FRAMERENDER pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D11SwapChainReleasing( LPDXUTCALLBACKD3D11SWAPCHAINRELEASING pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D11DeviceDestroyed( LPDXUTCALLBACKD3D11DEVICEDESTROYED pCallback, void* pUserContext = NULL );


//--------------------------------------------------------------------------------------
// Initialization
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTInit(_In_ bool bParseCommandLine = true,
						_In_ bool bShowMsgBoxOnError = true,
						_In_opt_ WCHAR* strExtraCommandLineParams = nullptr,
						_In_ bool bThreadSafeDXUT = false);

// Choose either DXUTCreateWindow or DXUTSetWindow.  If using DXUTSetWindow, consider using DXUTStaticWndProc
HRESULT WINAPI DXUTCreateWindow(_In_z_ const WCHAR* strWindowTitle = L"Direct3D Window",
								_In_opt_ HINSTANCE hInstance = nullptr, _In_opt_ HICON hIcon = nullptr, _In_opt_ HMENU hMenu = nullptr,
								_In_ int x = CW_USEDEFAULT, _In_ int y = CW_USEDEFAULT);
HRESULT WINAPI DXUTSetWindow(_In_ HWND hWndFocus, _In_ HWND hWndDeviceFullScreen, _In_ HWND hWndDeviceWindowed, _In_ bool bHandleMessages = true);
LRESULT CALLBACK DXUTStaticWndProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam);

// Choose either DXUTCreateDevice or DXUTSetD3D*Device or DXUTCreateD3DDeviceFromSettings

//HENRY TODO
HRESULT WINAPI DXUTCreateDevice(D3D_FEATURE_LEVEL reqFL,  bool bWindowed= true, int nSuggestedWidth =0, int nSuggestedHeight =0, int msaaCount = 1, int msaaQuality = 0 );
HRESULT WINAPI DXUTCreateDeviceFromSettings( DXUTDeviceSettings* pDeviceSettings, bool bPreserveInput = false, bool bClipWindowToSingleAdapter = true );
//HRESULT WINAPI DXUTCreateDevice(_In_ D3D_FEATURE_LEVEL reqFL, _In_ bool bWindowed = true, _In_ int nSuggestedWidth = 0, _In_  int nSuggestedHeight = 0);
//HRESULT WINAPI DXUTCreateDeviceFromSettings( _In_ DXUTDeviceSettings* pDeviceSettings, _In_ bool bClipWindowToSingleAdapter = true );
//HRESULT WINAPI DXUTSetD3D9Device( IDirect3DDevice9* pd3dDevice );
#ifdef DIRECTX_11_1_ENABLED
HRESULT WINAPI DXUTSetD3D11Device(_In_ ID3D11Device1* pd3dDevice, _In_ IDXGISwapChain1* pSwapChain);
#else
HRESULT WINAPI DXUTSetD3D11Device(_In_ ID3D11Device* pd3dDevice, _In_ IDXGISwapChain* pSwapChain);
#endif


// Choose either DXUTMainLoop or implement your own main loop 
HRESULT WINAPI DXUTMainLoop(_In_opt_ HACCEL hAccel = nullptr);

// If not using DXUTMainLoop consider using DXUTRender3DEnvironment
void WINAPI DXUTRender3DEnvironment();



//--------------------------------------------------------------------------------------
// Common Tasks 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTToggleFullScreen();
HRESULT WINAPI DXUTToggleREF();
HRESULT WINAPI DXUTToggleWARP();
void    WINAPI DXUTPause(_In_ bool bPauseTime, _In_ bool bPauseRendering);
void    WINAPI DXUTSetConstantFrameTime(_In_ bool bConstantFrameTime, _In_ float fTimePerFrame = 0.0333f);
void    WINAPI DXUTSetCursorSettings(_In_ bool bShowCursorWhenFullScreen = false, _In_ bool bClipCursorWhenFullScreen = false);
void    WINAPI DXUTSetHotkeyHandling(_In_ bool bAltEnterToToggleFullscreen = true, _In_ bool bEscapeToQuit = true, _In_ bool bPauseToToggleTimePause = true);
void    WINAPI DXUTSetMultimonSettings(_In_ bool bAutoChangeAdapter = true);
void    WINAPI DXUTSetShortcutKeySettings(_In_ bool bAllowWhenFullscreen = false, _In_ bool bAllowWhenWindowed = true); // Controls the Windows key, and accessibility shortcut keys
void    WINAPI DXUTSetWindowSettings(_In_ bool bCallDefWindowProc = true);
HRESULT WINAPI DXUTSetTimer(_In_ LPDXUTCALLBACKTIMER pCallbackTimer, _In_ float fTimeoutInSecs = 1.0f, _Out_opt_ UINT* pnIDEvent = nullptr, _In_opt_ void* pCallbackUserContext = nullptr);
HRESULT WINAPI DXUTKillTimer(_In_ UINT nIDEvent);
void    WINAPI DXUTResetFrameworkState();
void    WINAPI DXUTShutdown(_In_ int nExitCode = 0);
void    WINAPI DXUTSetIsInGammaCorrectMode(_In_ bool bGammaCorrect);
bool    WINAPI DXUTGetMSAASwapChainCreated();

//--------------------------------------------------------------------------------------
// State Retrieval  
//--------------------------------------------------------------------------------------

// Direct3D 11(.1)
#ifdef DIRECTX_11_1_ENABLED
IDXGIFactory2*           WINAPI DXUTGetDXGIFactory();				// Does not addref unlike typical Get* APIs
IDXGISwapChain1*         WINAPI DXUTGetDXGISwapChain();				// Does not addref unlike typical Get* APIs
ID3D11Device1*			 WINAPI DXUTGetD3D11Device();				// Does not addref unlike typical Get* APIs
ID3D11DeviceContext1*	 WINAPI DXUTGetD3D11DeviceContext();		// Does not addref unlike typical Get* APIs
ID3DUserDefinedAnnotation* WINAPI DXUTGetUserDefinedAnnotation();	// Does not addref unlike typical Get* APIs
#else
IDXGIFactory1*           WINAPI DXUTGetDXGIFactory(); // Does not addref unlike typical Get* APIs
IDXGISwapChain*          WINAPI DXUTGetDXGISwapChain(); // Does not addref unlike typical Get* APIs
ID3D11Device*			 WINAPI DXUTGetD3D11Device(); // Does not addref unlike typical Get* APIs
ID3D11DeviceContext*	 WINAPI DXUTGetD3D11DeviceContext(); // Does not addref unlike typical Get* APIs
#endif


const DXGI_SURFACE_DESC* WINAPI DXUTGetDXGIBackBufferSurfaceDesc();
bool                     WINAPI DXUTIsD3D11Available(); // If D3D11 APIs are availible
HRESULT                  WINAPI DXUTSetupD3D11Views(_In_ ID3D11DeviceContext* pd3dDeviceContext); // Supports immediate or deferred context
D3D_FEATURE_LEVEL	     WINAPI DXUTGetD3D11DeviceFeatureLevel();	// Returns the D3D11 devices current feature level
ID3D11RenderTargetView*  WINAPI DXUTGetD3D11RenderTargetView();		// Does not addref unlike typical Get* APIs
ID3D11DepthStencilView*  WINAPI DXUTGetD3D11DepthStencilView();		// Does not addref unlike typical Get* APIs
ID3D11ShaderResourceView* WINAPI DXUTGetD3D11DepthStencilViewSRV(); // Does not addref unlike typical Get* APIs
//bool                     WINAPI DXUTDoesAppSupportD3D11();
//bool                     WINAPI DXUTIsAppRenderingWithD3D11();


// General
DXUTDeviceSettings WINAPI DXUTGetDeviceSettings();
HINSTANCE WINAPI DXUTGetHINSTANCE();
HWND      WINAPI DXUTGetHWND();
HWND      WINAPI DXUTGetHWNDFocus();
HWND      WINAPI DXUTGetHWNDDeviceFullScreen();
HWND      WINAPI DXUTGetHWNDDeviceWindowed();
RECT      WINAPI DXUTGetWindowClientRect();
LONG      WINAPI DXUTGetWindowWidth();
LONG      WINAPI DXUTGetWindowHeight();
RECT      WINAPI DXUTGetWindowClientRectAtModeChange(); // Useful for returning to windowed mode with the same resolution as before toggle to full screen mode
RECT      WINAPI DXUTGetFullsceenClientRectAtModeChange(); // Useful for returning to full screen mode with the same resolution as before toggle to windowed mode
double    WINAPI DXUTGetTime();
float     WINAPI DXUTGetElapsedTime();
bool      WINAPI DXUTIsWindowed();
bool	  WINAPI DXUTIsInGammaCorrectMode();
float     WINAPI DXUTGetFPS();
LPCWSTR   WINAPI DXUTGetWindowTitle();
LPCWSTR   WINAPI DXUTGetFrameStats(_In_ bool bIncludeFPS = false);
LPCWSTR   WINAPI DXUTGetDeviceStats();

bool      WINAPI DXUTIsVsyncEnabled();
bool      WINAPI DXUTIsRenderingPaused();
bool      WINAPI DXUTIsTimePaused();
bool      WINAPI DXUTIsActive();
int       WINAPI DXUTGetExitCode();
bool      WINAPI DXUTGetShowMsgBoxOnError();
bool      WINAPI DXUTGetAutomation();  // Returns true if -automation parameter is used to launch the app
bool      WINAPI DXUTIsKeyDown(_In_ BYTE vKey); // Pass a virtual-key code, ex. VK_F1, 'A', VK_RETURN, VK_LSHIFT, etc
bool      WINAPI DXUTWasKeyPressed(_In_ BYTE vKey);  // Like DXUTIsKeyDown() but return true only if the key was just pressed
bool      WINAPI DXUTIsMouseButtonDown(_In_ BYTE vButton); // Pass a virtual-key code: VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2
HRESULT   WINAPI DXUTCreateState(); // Optional method to create DXUT's memory.  If its not called by the application it will be automatically called when needed
void      WINAPI DXUTDestroyState(); // Optional method to destroy DXUT's memory.  If its not called by the application it will be automatically called after the application exits WinMain 


// DEBUG DXGI

template<UINT TNameLength>
inline void DXUT_SetDebugName(_In_ ID3D11DeviceChild* resource, _In_z_ const char(&name)[TNameLength])
{
#if defined(_DEBUG) || defined(PROFILE)
	resource->SetPrivateData(WKPDID_D3DDebugObjectName, TNameLength - 1, name);
#else
	UNREFERENCED_PARAMETER(resource);
	UNREFERENCED_PARAMETER(name);
#endif
}

template<UINT TNameLength>
inline void DXUT_SetDebugName(_In_ ID3D11DeviceChild* resource, _In_z_ const char(&name)[TNameLength], const char* file, DWORD line)
{
#if defined(_DEBUG) || defined(PROFILE)
	char msg[256] = { 0 };
	sprintf_s(msg, 256, "%s %s: %i", name, file, line);
	resource->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(msg) - 1, msg);
	//resource->SetPrivateData(WKPDID_D3DDebugObjectName, TNameLength - 1, name);
#else
	UNREFERENCED_PARAMETER(resource);
	UNREFERENCED_PARAMETER(name);
#endif
}

template<UINT TNameLength>
inline void DXUT_SetDebugName(_In_ IDXGIObject* resource, _In_z_ const char(&name)[TNameLength])
{
#if defined(_DEBUG) || defined(PROFILE)
	resource->SetPrivateData(WKPDID_D3DDebugObjectName, TNameLength - 1, name);
#else
	UNREFERENCED_PARAMETER(resource);
	UNREFERENCED_PARAMETER(name);
#endif
}

template<UINT TNameLength>
inline void DXUT_SetDebugName(_In_ IDXGIObject* resource, _In_z_ const char(&name)[TNameLength], const char* file, DWORD line)
{
#if defined(_DEBUG) || defined(PROFILE)
	char msg[256] = { 0 };
	sprintf_s(msg, 256, "%s %s: %i", name, file, line);
	resource->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(msg) - 1, msg);
#else
	UNREFERENCED_PARAMETER(resource);
	UNREFERENCED_PARAMETER(name);
#endif
}

void WINAPI DXUTReportLiveObjectsDXGI(LPCWSTR msg = L"-----------------------------------------------------------\n");

#endif

//--------------------------------------------------------------------------------------
// DXUT core layer includes
//--------------------------------------------------------------------------------------
#include "DXUTmisc.h"
#include "DXUTDevice11.h"






