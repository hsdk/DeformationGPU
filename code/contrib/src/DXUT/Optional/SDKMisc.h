//--------------------------------------------------------------------------------------
// File: SDKMisc.h
//
// Various helper functionality that is shared between SDK samples
//
// Copyright (c) Microsoft Corporation. All rights reserved
//--------------------------------------------------------------------------------------
#pragma once
#ifndef SDKMISC_H
#define SDKMISC_H

//-----------------------------------------------------------------------------
// Resource cache for textures, fonts, meshs, and effects.  
// Use DXUTGetGlobalResourceCache() to access the global cache
//-----------------------------------------------------------------------------

enum DXUTCACHE_SOURCELOCATION
{
    DXUTCACHE_LOCATION_FILE,
    DXUTCACHE_LOCATION_RESOURCE
};


//--------------------------------------------------------------------------------------
// Manages the insertion point when drawing text
//--------------------------------------------------------------------------------------
class CDXUTDialogResourceManager;
class CDXUTTextHelper
{
public:
#ifdef DIRECTX_11_1_ENABLED
			CDXUTTextHelper( ID3D11Device1* pd3d11Device, ID3D11DeviceContext1* pd3dDeviceContext, CDXUTDialogResourceManager* pManager, int nLineHeight );
#else
			CDXUTTextHelper( ID3D11Device* pd3d11Device, ID3D11DeviceContext* pd3dDeviceContext, CDXUTDialogResourceManager* pManager, int nLineHeight );
#endif
            ~CDXUTTextHelper();

    void    SetInsertionPos( int x, int y )
    {
        m_pt.x = x; m_pt.y = y;
    }
    void    SetForegroundColor( DirectX::XMFLOAT4 clr )
    {
        //XMStoreFloat4(&m_clr,clr);
		m_clr = clr;
    }

    void    Begin();
    HRESULT DrawFormattedTextLine( const WCHAR* strMsg, ... );
    HRESULT DrawTextLine( const WCHAR* strMsg );
    HRESULT DrawFormattedTextLine( RECT& rc, DWORD dwFlags, const WCHAR* strMsg, ... );
    HRESULT DrawTextLine( RECT& rc, DWORD dwFlags, const WCHAR* strMsg );
    void    End();

protected:	
    DirectX::XMFLOAT4 m_clr;
    POINT m_pt;
    int m_nLineHeight;

	// D3D11 font
#ifdef DIRECTX_11_1_ENABLED
	ID3D11Device1* m_pd3d11Device;
	ID3D11DeviceContext1* m_pd3d11DeviceContext;
#else
	ID3D11Device* m_pd3d11Device;
	ID3D11DeviceContext* m_pd3d11DeviceContext;
#endif
	CDXUTDialogResourceManager* m_pManager;
};


////--------------------------------------------------------------------------------------
//// Manages a persistent list of lines and draws them using ID3DXLine
////--------------------------------------------------------------------------------------
//class CDXUTLineManager
//{
//public:
//            CDXUTLineManager();
//            ~CDXUTLineManager();
//
//    HRESULT OnCreatedDevice( IDirect3DDevice9* pd3dDevice );
//    HRESULT OnResetDevice();
//    HRESULT OnRender();
//    HRESULT OnLostDevice();
//    HRESULT OnDeletedDevice();
//
//    HRESULT AddLine( int* pnLineID, D3DXVECTOR2* pVertexList, DWORD dwVertexListCount, D3DCOLOR Color, float fWidth,
//                     float fScaleRatio, bool bAntiAlias );
//    HRESULT AddRect( int* pnLineID, RECT rc, D3DCOLOR Color, float fWidth, float fScaleRatio, bool bAntiAlias );
//    HRESULT RemoveLine( int nLineID );
//    HRESULT RemoveAllLines();
//
//protected:
//    struct LINE_NODE
//    {
//        int nLineID;
//        D3DCOLOR Color;
//        float fWidth;
//        bool bAntiAlias;
//        float fScaleRatio;
//        D3DXVECTOR2* pVertexList;
//        DWORD dwVertexListCount;
//    };
//
//    CGrowableArray <LINE_NODE*> m_LinesList;
//    IDirect3DDevice9* m_pd3dDevice;
//    ID3DXLine* m_pD3DXLine;
//};


//--------------------------------------------------------------------------------------
// Shared code for samples to ask user if they want to use a REF device or quit
//--------------------------------------------------------------------------------------
void WINAPI DXUTDisplaySwitchingToREFWarning();

//--------------------------------------------------------------------------------------
// Tries to finds a media file by searching in common locations
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTFindDXSDKMediaFileCch( __in_ecount(cchDest) WCHAR* strDestPath,
                                         int cchDest, 
                                         __in LPCWSTR strFilename );
HRESULT WINAPI DXUTSetMediaSearchPath( LPCWSTR strPath );
LPCWSTR WINAPI DXUTGetMediaSearchPath();


//--------------------------------------------------------------------------------------
// Returns a view matrix for rendering to a face of a cubemap.
//--------------------------------------------------------------------------------------
DirectX::XMMATRIX WINAPI DXUTGetCubeMapViewMatrix( DWORD dwFace );


//--------------------------------------------------------------------------------------
// Helper function to compile an hlsl shader from file, 
// its binary compiled code is returned
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, 
										 LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines = NULL );






//--------------------------------------------------------------------------------------
// Simple helper stack class
//--------------------------------------------------------------------------------------
template <class T> class CDXUTStack
{
private:
    UINT m_MemorySize;
    UINT m_NumElements;
    T* m_pData;

    bool    EnsureStackSize( UINT64 iElements )
    {
        if( m_MemorySize > iElements )
            return true;

        T* pTemp = new T[ ( size_t )( iElements * 2 + 256 ) ];
        if( !pTemp )
            return false;

        if( m_NumElements )
        {
            CopyMemory( pTemp, m_pData, ( size_t )( m_NumElements * sizeof( T ) ) );
        }

        if( m_pData ) delete []m_pData;
        m_pData = pTemp;
        return true;
    }

public:
            CDXUTStack()
            {
                m_pData = NULL; m_NumElements = 0; m_MemorySize = 0;
            }
            ~CDXUTStack()
            {
                if( m_pData ) delete []m_pData;
            }

    UINT    GetCount()
    {
        return m_NumElements;
    }
    T       GetAt( UINT i )
    {
        return m_pData[i];
    }
    T       GetTop()
    {
        if( m_NumElements < 1 )
            return NULL;

        return m_pData[ m_NumElements - 1 ];
    }

    T       GetRelative( INT i )
    {
        INT64 iVal = m_NumElements - 1 + i;
        if( iVal < 0 )
            return NULL;
        return m_pData[ iVal ];
    }

    bool    Push( T pElem )
    {
        if( !EnsureStackSize( m_NumElements + 1 ) )
            return false;

        m_pData[m_NumElements] = pElem;
        m_NumElements++;

        return true;
    }

    T       Pop()
    {
        if( m_NumElements < 1 )
            return NULL;

        m_NumElements --;
        return m_pData[m_NumElements];
    }
};


#endif
