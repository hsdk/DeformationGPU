//--------------------------------------------------------------------------------------
// File: SDKmisc.cpp
//
// Various helper functionality that is shared between SDK samples
//
// Copyright (c) Microsoft Corporation. All rights reserved
//--------------------------------------------------------------------------------------
#include <DXUT.h>
#include "SDKmisc.h"
#include "DXUTres.h"
#undef min // use __min instead
#undef max // use __max instead

#include "DXUTGui.h"
#include <fstream>



using namespace DirectX;

//--------------------------------------------------------------------------------------
// Global/Static Members
//--------------------------------------------------------------------------------------
//CDXUTResourceCache& WINAPI DXUTGetGlobalResourceCache()
//{
//    // Using an accessor function gives control of the construction order
//    static CDXUTResourceCache cache;
//    return cache;
//}


//--------------------------------------------------------------------------------------
// Internal functions forward declarations
//--------------------------------------------------------------------------------------
bool DXUTFindMediaSearchTypicalDirs( __in_ecount(cchSearch) WCHAR* strSearchPath, 
                                     int cchSearch, 
                                     __in LPCWSTR strLeaf, 
                                     __in WCHAR* strExePath,
                                     __in WCHAR* strExeName );
bool DXUTFindMediaSearchParentDirs( __in_ecount(cchSearch) WCHAR* strSearchPath, 
                                    int cchSearch, 
                                    __in WCHAR* strStartAt, 
                                    __in WCHAR* strLeafName );
INT_PTR CALLBACK DisplaySwitchToREFWarningProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );


//--------------------------------------------------------------------------------------
// Shared code for samples to ask user if they want to use a REF device or quit
//--------------------------------------------------------------------------------------
void WINAPI DXUTDisplaySwitchingToREFWarning( )
{
    if( DXUTGetShowMsgBoxOnError() )
    {
        DWORD dwSkipWarning = 0, dwRead = 0, dwWritten = 0;
        HANDLE hFile = NULL;

        // Read previous user settings
        WCHAR strPath[MAX_PATH];
        SHGetFolderPath( DXUTGetHWND(), CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, strPath );
        wcscat_s( strPath, MAX_PATH, L"\\DXUT\\SkipRefWarning.dat" );
        if( ( hFile = CreateFile( strPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0,
                                  NULL ) ) != INVALID_HANDLE_VALUE )
        {
            ReadFile( hFile, &dwSkipWarning, sizeof( DWORD ), &dwRead, NULL );
            CloseHandle( hFile );
        }

        if( dwSkipWarning == 0 )
        {
            // Compact code to create a custom dialog box without using a template in a resource file.
            // If this dialog were in a .rc file, this would be a lot simpler but every sample calling this function would
            // need a copy of the dialog in its own .rc file. Also MessageBox API could be used here instead, but 
            // the MessageBox API is simpler to call but it can't provide a "Don't show again" checkbox
            typedef struct
            {
                DLGITEMTEMPLATE a;
                WORD b;
                WORD c;
                WORD d;
                WORD e;
                WORD f;
            } DXUT_DLG_ITEM;
            typedef struct
            {
                DLGTEMPLATE a;
                WORD b;
                WORD c;
                WCHAR   d[2];
                WORD e;
                WCHAR   f[16];
                DXUT_DLG_ITEM i1;
                DXUT_DLG_ITEM i2;
                DXUT_DLG_ITEM i3;
                DXUT_DLG_ITEM i4;
                DXUT_DLG_ITEM i5;
            } DXUT_DLG_DATA;

            DXUT_DLG_DATA dtp =
            {
                {WS_CAPTION | WS_POPUP | WS_VISIBLE | WS_SYSMENU | DS_ABSALIGN | DS_3DLOOK | DS_SETFONT |
                    DS_MODALFRAME | DS_CENTER,0,5,0,0,269,82},0,0,L" ",8,L"MS Shell Dlg 2",
                { {WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,0,7,7,24,24,0x100},0xFFFF,0x0082,0,0,0}, // icon
                { {WS_CHILD | WS_VISIBLE,0,40,7,230,25,0x101},0xFFFF,0x0082,0,0,0}, // static text
                { {WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,0,80,39,50,14,IDYES},0xFFFF,0x0080,0,0,0}, // Yes button
                { {WS_CHILD | WS_VISIBLE | WS_TABSTOP,0,133,39,50,14,IDNO},0xFFFF,0x0080,0,0,0}, // No button
                { {WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX,0,7,59,70,16,IDIGNORE},0xFFFF,0x0080,0,0,0}, // checkbox
            };

            LPARAM lParam;
            //if( ver == DXUT_D3D9_DEVICE )
            //    lParam = 9;
            //else
                lParam = 11;
            int nResult = ( int )DialogBoxIndirectParam( DXUTGetHINSTANCE(), ( DLGTEMPLATE* )&dtp, DXUTGetHWND(),
                                                         DisplaySwitchToREFWarningProc, lParam );

            if( ( nResult & 0x80 ) == 0x80 ) // "Don't show again" checkbox was checked
            {
                // Save user settings
                dwSkipWarning = 1;
                SHGetFolderPath( DXUTGetHWND(), CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, strPath );
                wcscat_s( strPath, MAX_PATH, L"\\DXUT" );
                CreateDirectory( strPath, NULL );
                wcscat_s( strPath, MAX_PATH, L"\\SkipRefWarning.dat" );
                if( ( hFile = CreateFile( strPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0,
                                          NULL ) ) != INVALID_HANDLE_VALUE )
                {
                    WriteFile( hFile, &dwSkipWarning, sizeof( DWORD ), &dwWritten, NULL );
                    CloseHandle( hFile );
                }
            }

            // User choose not to continue
            if( ( nResult & 0x0F ) == IDNO )
                DXUTShutdown( 1 );
        }
    }
}


//--------------------------------------------------------------------------------------
// MsgProc for DXUTDisplaySwitchingToREFWarning() dialog box
//--------------------------------------------------------------------------------------
INT_PTR CALLBACK DisplaySwitchToREFWarningProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch( message )
    {
        case WM_INITDIALOG:
            // Easier to set text here than in the DLGITEMTEMPLATE
            SetWindowText( hDlg, DXUTGetWindowTitle() );
            SendMessage( GetDlgItem( hDlg, 0x100 ), STM_SETIMAGE, IMAGE_ICON, ( LPARAM )LoadIcon( 0, IDI_QUESTION ) );
            WCHAR sz[512];
            swprintf_s( sz, 512,
                             L"This program needs to use the Direct3D %d reference device.  This device implements the entire Direct3D %d feature set, but runs very slowly.  Do you wish to continue?", lParam, lParam );
            SetDlgItemText( hDlg, 0x101, sz );
            SetDlgItemText( hDlg, IDYES, L"&Yes" );
            SetDlgItemText( hDlg, IDNO, L"&No" );
            SetDlgItemText( hDlg, IDIGNORE, L"&Don't show again" );
            break;

        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDIGNORE:
                    CheckDlgButton( hDlg, IDIGNORE, ( IsDlgButtonChecked( hDlg,
                                                                          IDIGNORE ) == BST_CHECKED ) ? BST_UNCHECKED :
                                    BST_CHECKED );
                    EnableWindow( GetDlgItem( hDlg, IDNO ), ( IsDlgButtonChecked( hDlg, IDIGNORE ) != BST_CHECKED ) );
                    break;
                case IDNO:
                    EndDialog( hDlg, ( IsDlgButtonChecked( hDlg, IDIGNORE ) == BST_CHECKED ) ? IDNO | 0x80 : IDNO |
                               0x00 ); return TRUE;
                case IDCANCEL:
                case IDYES:
                    EndDialog( hDlg, ( IsDlgButtonChecked( hDlg, IDIGNORE ) == BST_CHECKED ) ? IDYES | 0x80 : IDYES |
                               0x00 ); return TRUE;
            }
            break;
    }
    return FALSE;
}


//--------------------------------------------------------------------------------------
// Returns pointer to static media search buffer
//--------------------------------------------------------------------------------------
WCHAR* DXUTMediaSearchPath()
{
    static WCHAR s_strMediaSearchPath[MAX_PATH] =
    {
        0
    };
    return s_strMediaSearchPath;

}

//--------------------------------------------------------------------------------------
LPCWSTR WINAPI DXUTGetMediaSearchPath()
{
    return DXUTMediaSearchPath();
}


//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetMediaSearchPath( LPCWSTR strPath )
{
    HRESULT hr;

    WCHAR* s_strSearchPath = DXUTMediaSearchPath();

    hr = wcscpy_s( s_strSearchPath, MAX_PATH, strPath );
    if( SUCCEEDED( hr ) )
    {
        // append slash if needed
        size_t ch = 0;
        ch = wcsnlen( s_strSearchPath, MAX_PATH);
        if( SUCCEEDED( hr ) && s_strSearchPath[ch - 1] != L'\\' )
        {
            hr = wcscat_s( s_strSearchPath, MAX_PATH, L"\\" );
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Tries to find the location of a SDK media file
//       cchDest is the size in WCHARs of strDestPath.  Be careful not to 
//       pass in sizeof(strDest) on UNICODE builds.
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTFindDXSDKMediaFileCch(__in_ecount(cchDest) WCHAR* strDestPath,
                                         int cchDest, 
                                         __in LPCWSTR strFilename )
{
    bool bFound;
    WCHAR strSearchFor[MAX_PATH];

    if( NULL == strFilename || strFilename[0] == 0 || NULL == strDestPath || cchDest < 10 )
        return E_INVALIDARG;

    // Get the exe name, and exe path
    WCHAR strExePath[MAX_PATH] =
    {
        0
    };
    WCHAR strExeName[MAX_PATH] =
    {
        0
    };
    WCHAR* strLastSlash = NULL;
    GetModuleFileName( NULL, strExePath, MAX_PATH );
    strExePath[MAX_PATH - 1] = 0;
    strLastSlash = wcsrchr( strExePath, TEXT( '\\' ) );
    if( strLastSlash )
    {
        wcscpy_s( strExeName, MAX_PATH, &strLastSlash[1] );

        // Chop the exe name from the exe path
        *strLastSlash = 0;

        // Chop the .exe from the exe name
        strLastSlash = wcsrchr( strExeName, TEXT( '.' ) );
        if( strLastSlash )
            *strLastSlash = 0;
    }

    // Typical directories:
    //      .\
    //      ..\
    //      ..\..\
    //      %EXE_DIR%\
    //      %EXE_DIR%\..\
    //      %EXE_DIR%\..\..\
    //      %EXE_DIR%\..\%EXE_NAME%
    //      %EXE_DIR%\..\..\%EXE_NAME%

    // Typical directory search
    bFound = DXUTFindMediaSearchTypicalDirs( strDestPath, cchDest, strFilename, strExePath, strExeName );
    if( bFound )
        return S_OK;

    // Typical directory search again, but also look in a subdir called "\media\" 
    swprintf_s( strSearchFor, MAX_PATH, L"media\\%s", strFilename );
    bFound = DXUTFindMediaSearchTypicalDirs( strDestPath, cchDest, strSearchFor, strExePath, strExeName );
    if( bFound )
        return S_OK;

    WCHAR strLeafName[MAX_PATH] =
    {
        0
    };

    // Search all parent directories starting at .\ and using strFilename as the leaf name
    wcscpy_s( strLeafName, MAX_PATH, strFilename );
    bFound = DXUTFindMediaSearchParentDirs( strDestPath, cchDest, L".", strLeafName );
    if( bFound )
        return S_OK;

    // Search all parent directories starting at the exe's dir and using strFilename as the leaf name
    bFound = DXUTFindMediaSearchParentDirs( strDestPath, cchDest, strExePath, strLeafName );
    if( bFound )
        return S_OK;

    // Search all parent directories starting at .\ and using "media\strFilename" as the leaf name
    swprintf_s( strLeafName, MAX_PATH, L"media\\%s", strFilename );
    bFound = DXUTFindMediaSearchParentDirs( strDestPath, cchDest, L".", strLeafName );
    if( bFound )
        return S_OK;

    // Search all parent directories starting at the exe's dir and using "media\strFilename" as the leaf name
    bFound = DXUTFindMediaSearchParentDirs( strDestPath, cchDest, strExePath, strLeafName );
    if( bFound )
        return S_OK;

    // On failure, return the file as the path but also return an error code
    wcscpy_s( strDestPath, cchDest, strFilename );

    return DXUTERR_MEDIANOTFOUND;
}


//--------------------------------------------------------------------------------------
// Search a set of typical directories
//--------------------------------------------------------------------------------------
bool DXUTFindMediaSearchTypicalDirs( __in_ecount(cchSearch) WCHAR* strSearchPath, 
                                    int cchSearch, 
                                    __in LPCWSTR strLeaf, 
                                    __in WCHAR* strExePath,
                                    __in WCHAR* strExeName )
{
    // Typical directories:
    //      .\
    //      ..\
    //      ..\..\
    //      %EXE_DIR%\
    //      %EXE_DIR%\..\
    //      %EXE_DIR%\..\..\
    //      %EXE_DIR%\..\%EXE_NAME%
    //      %EXE_DIR%\..\..\%EXE_NAME%
    //      ..\source\%EXE_NAME%
    //      DXSDK media path

    // Search in .\  
    wcscpy_s( strSearchPath, cchSearch, strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in ..\  
    swprintf_s( strSearchPath, cchSearch, L"..\\%s", strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in ..\..\ 
    swprintf_s( strSearchPath, cchSearch, L"..\\..\\%s", strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in ..\..\ 
    swprintf_s( strSearchPath, cchSearch, L"..\\..\\%s", strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in the %EXE_DIR%\ 
    swprintf_s( strSearchPath, cchSearch, L"%s\\%s", strExePath, strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in the %EXE_DIR%\..\ 
    swprintf_s( strSearchPath, cchSearch, L"%s\\..\\%s", strExePath, strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in the %EXE_DIR%\..\..\ 
    swprintf_s( strSearchPath, cchSearch, L"%s\\..\\..\\%s", strExePath, strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in "%EXE_DIR%\..\%EXE_NAME%\".  This matches the DirectX SDK layout
    swprintf_s( strSearchPath, cchSearch, L"%s\\..\\%s\\%s", strExePath, strExeName, strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in "%EXE_DIR%\..\..\%EXE_NAME%\".  This matches the DirectX SDK layout
    swprintf_s( strSearchPath, cchSearch, L"%s\\..\\..\\%s\\%s", strExePath, strExeName, strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in "..\source\%EXE_NAME%\" -- for shader files
    swprintf_s( strSearchPath, cchSearch, L"..\\source\\%s\\%s", strExeName, strLeaf );
    if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
        return true;

    // Search in media search dir 
    WCHAR* s_strSearchPath = DXUTMediaSearchPath();
    if( s_strSearchPath[0] != 0 )
    {
        swprintf_s( strSearchPath, cchSearch, L"%s%s", s_strSearchPath, strLeaf );
        if( GetFileAttributes( strSearchPath ) != 0xFFFFFFFF )
            return true;
    }

    return false;
}



//--------------------------------------------------------------------------------------
// Search parent directories starting at strStartAt, and appending strLeafName
// at each parent directory.  It stops at the root directory.
//--------------------------------------------------------------------------------------
bool DXUTFindMediaSearchParentDirs( __in_ecount(cchSearch) WCHAR* strSearchPath, 
                                    int cchSearch, 
                                    __in WCHAR* strStartAt, 
                                    __in WCHAR* strLeafName )
{
    WCHAR strFullPath[MAX_PATH] =
    {
        0
    };
    WCHAR strFullFileName[MAX_PATH] =
    {
        0
    };
    WCHAR strSearch[MAX_PATH] =
    {
        0
    };
    WCHAR* strFilePart = NULL;

    GetFullPathName( strStartAt, MAX_PATH, strFullPath, &strFilePart );
    if( strFilePart == NULL )
        return false;

    while( strFilePart != NULL && *strFilePart != '\0' )
    {
        swprintf_s( strFullFileName, MAX_PATH, L"%s\\%s", strFullPath, strLeafName );
        if( GetFileAttributes( strFullFileName ) != 0xFFFFFFFF )
        {
            wcscpy_s( strSearchPath, cchSearch, strFullFileName );
            return true;
        }

        swprintf_s( strSearch, MAX_PATH, L"%s\\..", strFullPath );
        GetFullPathName( strSearch, MAX_PATH, strFullPath, &strFilePart );
    }

    return false;
}

#include <iostream>
HRESULT WINAPI DXUTCompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D_SHADER_MACRO* pDefines /*= NULL */ )
{
	HRESULT hr = S_OK;

	// find the file
	WCHAR str[MAX_PATH];
	ZeroMemory(&str, sizeof(str));
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

	//std::cout << str << std::endl;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif
	ID3DBlob* pErrorBlob;
	hr = D3DCompileFromFile(str, pDefines, NULL, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	
	if( FAILED(hr) )
	{
		OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
		std::cerr << (char*)pErrorBlob->GetBufferPointer() << std::endl;
		SAFE_RELEASE( pErrorBlob );
		return hr;
	}
	SAFE_RELEASE( pErrorBlob );

	return S_OK;
}



//--------------------------------------------------------------------------------------
#ifdef DIRECTX_11_1_ENABLED
CDXUTTextHelper::CDXUTTextHelper( _In_ ID3D11Device1* pd3d11Device, ID3D11DeviceContext1* pd3d11DeviceContext, CDXUTDialogResourceManager* pManager, int nLineHeight )
#else
CDXUTTextHelper::CDXUTTextHelper( _In_ ID3D11Device* pd3d11Device, ID3D11DeviceContext* pd3d11DeviceContext, CDXUTDialogResourceManager* pManager, int nLineHeight )
#endif
{    
	m_clr = XMFLOAT4( 1, 1, 1, 1 );
	m_pt.x = 0;
	m_pt.y = 0;
	m_nLineHeight = nLineHeight;
	m_pd3d11Device = NULL;
	m_pd3d11DeviceContext = NULL;
	m_pManager = NULL; 

    m_pd3d11Device = pd3d11Device;
    m_pd3d11DeviceContext = pd3d11DeviceContext;
    m_pManager = pManager;
}

CDXUTTextHelper::~CDXUTTextHelper()
{

}


//--------------------------------------------------------------------------------------
HRESULT CDXUTTextHelper::DrawFormattedTextLine( const WCHAR* strMsg, ... )
{
    WCHAR strBuffer[512];

    va_list args;
    va_start( args, strMsg );
    vswprintf_s( strBuffer, 512, strMsg, args );
    strBuffer[511] = L'\0';
    va_end( args );

    return DrawTextLine( strBuffer );
}


//--------------------------------------------------------------------------------------
HRESULT CDXUTTextHelper::DrawTextLine( const WCHAR* strMsg )
{
	if( NULL == m_pd3d11DeviceContext )
		return DXUT_ERR_MSGBOX( L"DrawTextLine", E_INVALIDARG );

    HRESULT hr = S_OK;

	//CHECKME
    RECT rc;
    SetRect( &rc, m_pt.x, m_pt.y, 0, 0 );

	DrawText11DXUT(m_pd3d11Device, m_pd3d11DeviceContext, strMsg, rc, m_clr,
		(float)m_pManager->m_nBackBufferWidth, (float)m_pManager->m_nBackBufferHeight, false);

    if( FAILED( hr ) )
        return DXTRACE_ERR_MSGBOX( L"DrawText", hr );

    m_pt.y += m_nLineHeight;

    return S_OK;
}


HRESULT CDXUTTextHelper::DrawFormattedTextLine( RECT& rc, DWORD dwFlags, const WCHAR* strMsg, ... )
{
    WCHAR strBuffer[512];

    va_list args;
    va_start( args, strMsg );
    vswprintf_s( strBuffer, 512, strMsg, args );
    strBuffer[511] = L'\0';
    va_end( args );

    return DrawTextLine( rc, dwFlags, strBuffer );
}


HRESULT CDXUTTextHelper::DrawTextLine( RECT& rc, DWORD dwFlags, const WCHAR* strMsg )
{
	if( NULL == m_pd3d11DeviceContext )
		return DXUT_ERR_MSGBOX( L"DrawTextLine", E_INVALIDARG );

    HRESULT hr = S_OK;
	DrawText11DXUT(m_pd3d11Device, m_pd3d11DeviceContext, strMsg, rc, m_clr,
		(float)m_pManager->m_nBackBufferWidth, (float)m_pManager->m_nBackBufferHeight, false);

    if( FAILED( hr ) )
        return DXTRACE_ERR_MSGBOX( L"DrawText", hr );

    m_pt.y += m_nLineHeight;

    return S_OK;
}


//--------------------------------------------------------------------------------------
void CDXUTTextHelper::Begin()
{
	assert(m_pd3d11DeviceContext);
    m_pManager->StoreD3D11State( m_pd3d11DeviceContext );
    m_pManager->ApplyRenderUI11( m_pd3d11DeviceContext );
}

void CDXUTTextHelper::End()
{
	assert(m_pd3d11DeviceContext);
    m_pManager->RestoreD3D11State( m_pd3d11DeviceContext );
}
