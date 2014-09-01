// SELECT DIRECTX VERSION HERE
#define FORCE_DIRECTX_11_0

// If app hasn't choosen, set to work with Windows Vista and beyond
#ifndef WINVER
#define WINVER         0x0600
#endif
#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0600
#endif

// fix for windows 7 loading
#ifdef FORCE_DIRECTX_11_0
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#else
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/) && !defined(DXGI_1_2_FORMATS)
#define DXGI_1_2_FORMATS
#endif

#ifndef FORCE_DIRECTX_11_0
//#if(NTDDI_VERSION >= NTDDI_WIN8)
#include <d3d11_1.h>
#ifndef FORCE_DIRECTX_11_0
#define DIRECTX_11_1_ENABLED
#endif
//#endif
#endif

#ifdef FORCE_DIRECTX_11_0
#include <d3d11.h>
typedef ID3D11Device		ID3D11Device1;
typedef ID3D11DeviceContext ID3D11DeviceContext1;
typedef IDXGISwapChain		IDXGISwapChain1;
typedef IDXGIFactory1		IDXGIFactory2;
#endif