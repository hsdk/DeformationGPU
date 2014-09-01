//--------------------------------------------------------------------------------------
// File: DXUTcamera.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTres.h"

#include <iostream>
using namespace DirectX;

#undef min // use __min instead
#undef max // use __max instead



//--------------------------------------------------------------------------------------
CD3DArcBall::CD3DArcBall()
{
    Reset();
	
	m_vDownPt	 = XMVectorZero();
	m_vCurrentPt = XMVectorZero();

    m_Offset.x = m_Offset.y = 0;

    RECT rc;
    GetClientRect( GetForegroundWindow(), &rc );
    SetWindow( rc.right, rc.bottom );
}





//--------------------------------------------------------------------------------------
void CD3DArcBall::Reset()
{
	m_qDown = XMQuaternionIdentity();
	m_qNow  = XMQuaternionIdentity();
	m_mRotation = XMMatrixIdentity();
	m_mTranslation = XMMatrixIdentity();
	m_mTranslationDelta = XMMatrixIdentity();


    m_bDrag = FALSE;
    m_fRadiusTranslation = 1.0f;
    m_fRadius = 1.0f;
}




//--------------------------------------------------------------------------------------
XMVECTOR CD3DArcBall::ScreenToVector( float fScreenPtX, float fScreenPtY )
{
    // Scale to screen
    FLOAT x = -( fScreenPtX - m_Offset.x - m_nWidth  / 2 ) / ( m_fRadius * m_nWidth  / 2 );
    FLOAT y =  ( fScreenPtY - m_Offset.y - m_nHeight / 2 ) / ( m_fRadius * m_nHeight / 2 );

    FLOAT z = 0.0f;
    FLOAT mag = x * x + y * y;

    if( mag > 1.0f )
    {
        FLOAT scale = 1.0f / sqrtf( mag );
        x *= scale;
        y *= scale;
    }
    else
        z = sqrtf( 1.0f - mag );

    // Return vector
	return XMVectorSet(x,y,z,1);
    //return XMFLOAT3( x, y, z);
}



//--------------------------------------------------------------------------------------
XMVECTOR CD3DArcBall::QuatFromBallPoints( const XMVECTOR& vFrom, const XMVECTOR& vTo )
{
    //XMFLOAT3 vPart;
    //float fDot = XMVectorGetX(XMVector3Dot(vFrom, vTo));		// D3DXVec3Dot( &vFrom, &vTo );
	//XMStoreFloat3(&vPart, XMVector3Cross(vFrom,vTo));			// D3DXVec3Cross( &vPart, &vFrom, &vTo );
	//
	//
    //return XMVectorSet( vPart.x, vPart.y, vPart.z, fDot );

	XMVECTOR control = XMVectorSelectControl(0, 0, 0, 1);
	XMVECTOR result = XMVectorSelect(XMVector3Cross(vFrom, vTo),
									 XMVector3Dot(vFrom, vTo),
									 control);

	return result;
}




//--------------------------------------------------------------------------------------
void CD3DArcBall::OnBegin( int nX, int nY )
{
    // Only enter the drag state if the click falls
    // inside the click rectangle.
    if( nX >= m_Offset.x &&
        nX < m_Offset.x + m_nWidth &&
        nY >= m_Offset.y &&
        nY < m_Offset.y + m_nHeight )
    {
        m_bDrag = true;
        m_qDown = m_qNow;
        m_vDownPt = ScreenToVector( ( float )nX, ( float )nY );
    }
}




//--------------------------------------------------------------------------------------
void CD3DArcBall::OnMove( int nX, int nY )
{
    if( m_bDrag )
    {
        m_vCurrentPt = ScreenToVector( ( float )nX, ( float )nY );
		m_qNow = XMQuaternionMultiply( m_qDown,		QuatFromBallPoints( m_vDownPt, m_vCurrentPt ));
    }
}




//--------------------------------------------------------------------------------------
void CD3DArcBall::OnEnd()
{
    m_bDrag = false;
}




//--------------------------------------------------------------------------------------
// Desc:
//--------------------------------------------------------------------------------------
LRESULT CD3DArcBall::HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    // Current mouse position
    int iMouseX = ( short )LOWORD( lParam );
    int iMouseY = ( short )HIWORD( lParam );

    switch( uMsg )
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            SetCapture( hWnd );
            OnBegin( iMouseX, iMouseY );
            return TRUE;

        case WM_LBUTTONUP:
            ReleaseCapture();
            OnEnd();
            return TRUE;
        case WM_CAPTURECHANGED:
            if( ( HWND )lParam != hWnd )
            {
                ReleaseCapture();
                OnEnd();
            }
            return TRUE;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
            SetCapture( hWnd );
            // Store off the position of the cursor when the button is pressed
            m_ptLastMouse.x = iMouseX;
            m_ptLastMouse.y = iMouseY;
            return TRUE;

        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            ReleaseCapture();
            return TRUE;

        case WM_MOUSEMOVE:
            if( MK_LBUTTON & wParam )
            {
                OnMove( iMouseX, iMouseY );
            }
            else if( ( MK_RBUTTON & wParam ) || ( MK_MBUTTON & wParam ) )
            {
                // Normalize based on size of window and bounding sphere radius
                FLOAT fDeltaX = ( m_ptLastMouse.x - iMouseX ) * m_fRadiusTranslation / m_nWidth;
                FLOAT fDeltaY = ( m_ptLastMouse.y - iMouseY ) * m_fRadiusTranslation / m_nHeight;

				//CHECKME
                if( wParam & MK_RBUTTON )
                {
					m_mTranslationDelta = XMMatrixTranslation(-2 * fDeltaX, 2 * fDeltaY, 0.0f);					
					m_mTranslation =  XMMatrixMultiply(m_mTranslation, m_mTranslationDelta);
					//D3DXMatrixTranslation( &m_mTranslationDelta, -2 * fDeltaX, 2 * fDeltaY, 0.0f );
                    //D3DXMatrixMultiply( &m_mTranslation, &m_mTranslation, &m_mTranslationDelta );
                }
                else  // wParam & MK_MBUTTON
                {
					//m_mTranslationDelta = XMMatrixTranslation( 0.0f, 0.0f, 5 * fDeltaY);
					//m_mTranslation = XMMatrixMultiply(m_mTranslation, m_mTranslationDelta);
					m_mTranslationDelta = XMMatrixTranslation(0.0f, 0.0f, 5.f * fDeltaY);					
					m_mTranslation = XMMatrixMultiply(m_mTranslation, m_mTranslationDelta);
                    //D3DXMatrixTranslation( &m_mTranslationDelta, 0.0f, 0.0f, 5 * fDeltaY );
                    //D3DXMatrixMultiply( &m_mTranslation, &m_mTranslation, &m_mTranslationDelta );
                }

                // Store mouse coordinate
                m_ptLastMouse.x = iMouseX;
                m_ptLastMouse.y = iMouseY;
            }
            return TRUE;
    }

    return FALSE;
}




//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
CBaseCamera::CBaseCamera()
{
    m_cKeysDown = 0;
    ZeroMemory( m_aKeys, sizeof( BYTE ) * CAM_MAX_KEYS );
    ZeroMemory( m_GamePad, sizeof( DXUT_GAMEPAD ) * DXUT_MAX_CONTROLLERS );

    // Set attributes for the view matrix
    XMFLOAT3 vEyePt( 0.0f, 0.0f, 0.0f );
    XMFLOAT3 vLookatPt( 0.0f, 0.0f, 1.0f);

    // Setup the view matrix
    SetViewParams( &vEyePt, &vLookatPt );

    // Setup the projection matrix
    SetProjParams( XM_PIDIV4, 1.0f, 1.0f, 1000.0f );

    GetCursorPos( &m_ptLastMousePosition );
    m_bMouseLButtonDown = false;
    m_bMouseMButtonDown = false;
    m_bMouseRButtonDown = false;
    m_nCurrentButtonMask = 0;
    m_nMouseWheelDelta = 0;

    m_fCameraYawAngle = 0.0f;
    m_fCameraPitchAngle = 0.0f;

    SetRect( &m_rcDrag, LONG_MIN, LONG_MIN, LONG_MAX, LONG_MAX );
    m_vVelocity = XMFLOAT3( 0, 0, 0);
    m_bMovementDrag = false;
    m_vVelocityDrag = XMFLOAT3( 0, 0, 0);
    m_fDragTimer = 0.0f;
    m_fTotalDragTimeToZero = 0.25;
    m_vRotVelocity = XMFLOAT2( 0, 0);

    m_fRotationScaler = 0.01f;
    m_fMoveScaler = 5.0f;

    m_bInvertPitch = false;
    m_bEnableYAxisMovement = true;
    m_bEnablePositionMovement = true;

    m_vMouseDelta = XMFLOAT2( 0, 0);
    m_fFramesToSmoothMouseData = 2.0f;

    m_bClipToBoundary = false;
    m_vMinBoundary = XMFLOAT3( -1, -1, -1);
    m_vMaxBoundary = XMFLOAT3(  1,  1,  1);

    m_bResetCursorAfterMove = false;
}


//--------------------------------------------------------------------------------------
// Client can call this to change the position and direction of camera
//--------------------------------------------------------------------------------------
VOID CBaseCamera::SetViewParams( XMFLOAT3* pvEyePt, XMFLOAT3* pvLookatPt )
{
    if( NULL == pvEyePt || NULL == pvLookatPt )
        return;

    m_vDefaultEye = m_vEye = *pvEyePt;
    m_vDefaultLookAt = m_vLookAt = *pvLookatPt;

    // Calc the view matrix
	XMVECTOR vUp = XMVectorSet( 0.f, 0.f, 1.f, 0.f);

	m_mView = XMMatrixLookAtLH(XMLoadFloat3(pvEyePt), XMLoadFloat3(pvLookatPt), vUp);	
	    
	XMMATRIX mInvView = XMMatrixInverse(nullptr,m_mView);
	XMStoreFloat4x4(&m_mWorld, mInvView);
    // The axis basis vectors and camera position are stored inside the 
    // position matrix in the 4 rows of the camera's world matrix.
    // To figure out the yaw/pitch of the camera, we just need the Z basis vector
	//CHECKME
	XMFLOAT3 pZBasis;
	XMStoreFloat3(&pZBasis, mInvView.r[2]);	
	m_fCameraYawAngle = atan2f( pZBasis.x, pZBasis.z );
	float fLen = sqrtf( pZBasis.z * pZBasis.z + pZBasis.x * pZBasis.x );
	m_fCameraPitchAngle = -atan2f( pZBasis.y, fLen );

	//m_fCameraYawAngle = atan2f( pZBasis.x, pZBasis.y);
	//float fLen = sqrtf( pZBasis.y * pZBasis.y + pZBasis.x * pZBasis.x );
	//m_fCameraPitchAngle = -atan2f( pZBasis.z, fLen );	
}




//--------------------------------------------------------------------------------------
// Calculates the projection matrix based on input params
//--------------------------------------------------------------------------------------
VOID CBaseCamera::SetProjParams( FLOAT fFOV, FLOAT fAspect, FLOAT fNearPlane,
                                 FLOAT fFarPlane )
{
    // Set attributes for the projection matrix
    m_fFOV = fFOV;
    m_fAspect = fAspect;
    m_fNearPlane = fNearPlane;
    m_fFarPlane = fFarPlane;

	m_mProj = XMMatrixPerspectiveFovLH(fFOV, fAspect, fNearPlane, fFarPlane);
}




//--------------------------------------------------------------------------------------
// Call this from your message proc so this class can handle window messages
//--------------------------------------------------------------------------------------
LRESULT CBaseCamera::HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    UNREFERENCED_PARAMETER( hWnd );
    UNREFERENCED_PARAMETER( lParam );

    switch( uMsg )
    {
        case WM_KEYDOWN:
        {
            // Map this key to a D3DUtil_CameraKeys enum and update the
            // state of m_aKeys[] by adding the KEY_WAS_DOWN_MASK|KEY_IS_DOWN_MASK mask
            // only if the key is not down
            D3DUtil_CameraKeys mappedKey = MapKey( ( UINT )wParam );
            if( mappedKey != CAM_UNKNOWN )
            {
                if( FALSE == IsKeyDown( m_aKeys[mappedKey] ) )
                {
                    m_aKeys[ mappedKey ] = KEY_WAS_DOWN_MASK | KEY_IS_DOWN_MASK;
                    ++m_cKeysDown;
                }
            }
            break;
        }

        case WM_KEYUP:
        {
            // Map this key to a D3DUtil_CameraKeys enum and update the
            // state of m_aKeys[] by removing the KEY_IS_DOWN_MASK mask.
            D3DUtil_CameraKeys mappedKey = MapKey( ( UINT )wParam );
            if( mappedKey != CAM_UNKNOWN && ( DWORD )mappedKey < 8 )
            {
                m_aKeys[ mappedKey ] &= ~KEY_IS_DOWN_MASK;
                --m_cKeysDown;
            }
            break;
        }
		
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_LBUTTONDBLCLK:
            {
                // Compute the drag rectangle in screen coord.
                POINT ptCursor =
                {
                    ( short )LOWORD( lParam ), ( short )HIWORD( lParam )
                };

                // Update member var state
                if( ( uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK ) && PtInRect( &m_rcDrag, ptCursor ) )
                {
                    m_bMouseLButtonDown = true; m_nCurrentButtonMask |= MOUSE_LEFT_BUTTON;
                }
                if( ( uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK ) && PtInRect( &m_rcDrag, ptCursor ) )
                {
                    m_bMouseMButtonDown = true; m_nCurrentButtonMask |= MOUSE_MIDDLE_BUTTON;
                }
                if( ( uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK ) && PtInRect( &m_rcDrag, ptCursor ) )
                {
                    m_bMouseRButtonDown = true; m_nCurrentButtonMask |= MOUSE_RIGHT_BUTTON;
                }

                // Capture the mouse, so if the mouse button is 
                // released outside the window, we'll get the WM_LBUTTONUP message
                SetCapture( hWnd );
                GetCursorPos( &m_ptLastMousePosition );
                return TRUE;
            }

        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_LBUTTONUP:
            {
                // Update member var state
                if( uMsg == WM_LBUTTONUP )
                {
                    m_bMouseLButtonDown = false; m_nCurrentButtonMask &= ~MOUSE_LEFT_BUTTON;
                }
                if( uMsg == WM_MBUTTONUP )
                {
                    m_bMouseMButtonDown = false; m_nCurrentButtonMask &= ~MOUSE_MIDDLE_BUTTON;
                }
                if( uMsg == WM_RBUTTONUP )
                {
                    m_bMouseRButtonDown = false; m_nCurrentButtonMask &= ~MOUSE_RIGHT_BUTTON;
                }

                // Release the capture if no mouse buttons down
                if( !m_bMouseLButtonDown &&
                    !m_bMouseRButtonDown &&
                    !m_bMouseMButtonDown )
                {
                    ReleaseCapture();
                }
                break;
            }

        case WM_CAPTURECHANGED:
        {
            if( ( HWND )lParam != hWnd )
            {
                if( ( m_nCurrentButtonMask & MOUSE_LEFT_BUTTON ) ||
                    ( m_nCurrentButtonMask & MOUSE_MIDDLE_BUTTON ) ||
                    ( m_nCurrentButtonMask & MOUSE_RIGHT_BUTTON ) )
                {
                    m_bMouseLButtonDown = false;
                    m_bMouseMButtonDown = false;
                    m_bMouseRButtonDown = false;
                    m_nCurrentButtonMask &= ~MOUSE_LEFT_BUTTON;
                    m_nCurrentButtonMask &= ~MOUSE_MIDDLE_BUTTON;
                    m_nCurrentButtonMask &= ~MOUSE_RIGHT_BUTTON;
                    ReleaseCapture();
                }
            }
            break;
        }

        case WM_MOUSEWHEEL:
            // Update member var state
            m_nMouseWheelDelta += ( short )HIWORD( wParam );
            break;
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Figure out the velocity based on keyboard input & drag if any
//--------------------------------------------------------------------------------------
void CBaseCamera::GetInput( bool bGetKeyboardInput, bool bGetMouseInput, bool bGetGamepadInput,
                            bool bResetCursorAfterMove )
{
    m_vKeyboardDirection = XMFLOAT3( 0, 0, 0);
    if( bGetKeyboardInput )
    {
		// CHECKME, set m_vKeyboardDirection directly?
        // Update acceleration vector based on keyboard state
        if( IsKeyDown( m_aKeys[CAM_MOVE_FORWARD] ) )
		{
			m_vKeyboardDirection.z += 1.0f;
            //m_vKeyboardDirection = XMVectorSetZ(m_vKeyboardDirection, XMVectorGetZ(m_vKeyboardDirection) + 1.0f);
		}
        if( IsKeyDown( m_aKeys[CAM_MOVE_BACKWARD] ) )
		{
            m_vKeyboardDirection.z -= 1.0f;
			//m_vKeyboardDirection = XMVectorSetZ(m_vKeyboardDirection, XMVectorGetZ(m_vKeyboardDirection) - 1.0f);
		}
        if( m_bEnableYAxisMovement )
        {
            if( IsKeyDown( m_aKeys[CAM_MOVE_UP] ) )
			{
                m_vKeyboardDirection.y += 1.0f;
				//m_vKeyboardDirection = XMVectorSetY(m_vKeyboardDirection, XMVectorGetY(m_vKeyboardDirection) + 1.0f);
			}
            if( IsKeyDown( m_aKeys[CAM_MOVE_DOWN] ) )
			{
                m_vKeyboardDirection.y -= 1.0f;
				//m_vKeyboardDirection = XMVectorSetY(m_vKeyboardDirection, XMVectorGetY(m_vKeyboardDirection) - 1.0f);
			}
        }
        if( IsKeyDown( m_aKeys[CAM_STRAFE_RIGHT] ) )
		{
            m_vKeyboardDirection.x += 1.0f;
			//m_vKeyboardDirection = XMVectorSetX(m_vKeyboardDirection, XMVectorGetX(m_vKeyboardDirection) + 1.0f);
		}
        if( IsKeyDown( m_aKeys[CAM_STRAFE_LEFT] ) )
		{
            m_vKeyboardDirection.x -= 1.0f;
			//m_vKeyboardDirection = XMVectorSetX(m_vKeyboardDirection, XMVectorGetX(m_vKeyboardDirection) - 1.0f);			
		}
    }

    if( bGetMouseInput )
    {
        UpdateMouseDelta();
    }

    if( bGetGamepadInput )
    {
        m_vGamePadLeftThumb = XMFLOAT3(  0, 0, 0);
        m_vGamePadRightThumb = XMFLOAT3( 0, 0, 0);

        // Get controller state
        for( DWORD iUserIndex = 0; iUserIndex < DXUT_MAX_CONTROLLERS; iUserIndex++ )
        {
            DXUTGetGamepadState( iUserIndex, &m_GamePad[iUserIndex], true, true );

            // Mark time if the controller is in a non-zero state
            if( m_GamePad[iUserIndex].wButtons ||
                m_GamePad[iUserIndex].sThumbLX || m_GamePad[iUserIndex].sThumbLX ||
                m_GamePad[iUserIndex].sThumbRX || m_GamePad[iUserIndex].sThumbRY ||
                m_GamePad[iUserIndex].bLeftTrigger || m_GamePad[iUserIndex].bRightTrigger )
            {
                m_GamePadLastActive[iUserIndex] = DXUTGetTime();
            }
        }

        // Find out which controller was non-zero last
        int iMostRecentlyActive = -1;
        double fMostRecentlyActiveTime = 0.0f;
        for( DWORD iUserIndex = 0; iUserIndex < DXUT_MAX_CONTROLLERS; iUserIndex++ )
        {
            if( m_GamePadLastActive[iUserIndex] > fMostRecentlyActiveTime )
            {
                fMostRecentlyActiveTime = m_GamePadLastActive[iUserIndex];
                iMostRecentlyActive = iUserIndex;
            }
        }

        // Use the most recent non-zero controller if its connected
        if( iMostRecentlyActive >= 0 && m_GamePad[iMostRecentlyActive].bConnected )
        {
			//CHECKME
            //m_vGamePadLeftThumb.x = m_GamePad[iMostRecentlyActive].fThumbLX;
            //m_vGamePadLeftThumb.y = 0.0f;
            //m_vGamePadLeftThumb.z = m_GamePad[iMostRecentlyActive].fThumbLY;

            //m_vGamePadRightThumb.x = m_GamePad[iMostRecentlyActive].fThumbRX;
            //m_vGamePadRightThumb.y = 0.0f;
            //m_vGamePadRightThumb.z = m_GamePad[iMostRecentlyActive].fThumbRY;

			m_vGamePadLeftThumb = XMFLOAT3(m_GamePad[iMostRecentlyActive].fThumbLX, 0, m_GamePad[iMostRecentlyActive].fThumbLY);			
			m_vGamePadRightThumb = XMFLOAT3(m_GamePad[iMostRecentlyActive].fThumbRX, 0, m_GamePad[iMostRecentlyActive].fThumbRY);
        }
    }
}


//--------------------------------------------------------------------------------------
// Figure out the mouse delta based on mouse movement
//--------------------------------------------------------------------------------------
void CBaseCamera::UpdateMouseDelta()
{
//    POINT ptCurMouseDelta;
    POINT ptCurMousePos;

    // Get current position of mouse
    GetCursorPos( &ptCurMousePos );

    // Calc how far it's moved since last frame
    //ptCurMouseDelta.x = ptCurMousePos.x - m_ptLastMousePosition.x;
    //ptCurMouseDelta.y = ptCurMousePos.y - m_ptLastMousePosition.y;

	XMVECTOR curMouseDelta = XMVectorSet((FLOAT)(ptCurMousePos.x - m_ptLastMousePosition.x),
										 (FLOAT)(ptCurMousePos.y - m_ptLastMousePosition.y),
										 0.f, 0.f);

    // Record current position for next time
    m_ptLastMousePosition = ptCurMousePos;

    if( m_bResetCursorAfterMove && DXUTIsActive() )
    {
        // Set position of camera to center of desktop, 
        // so it always has room to move.  This is very useful
        // if the cursor is hidden.  If this isn't done and cursor is hidden, 
        // then invisible cursor will hit the edge of the screen 
        // and the user can't tell what happened
        POINT ptCenter;

        // Get the center of the current monitor
        MONITORINFO mi;
        mi.cbSize = sizeof( MONITORINFO );
        DXUTGetMonitorInfo( DXUTMonitorFromWindow( DXUTGetHWND(), MONITOR_DEFAULTTONEAREST ), &mi );
        ptCenter.x = ( mi.rcMonitor.left + mi.rcMonitor.right ) / 2;
        ptCenter.y = ( mi.rcMonitor.top + mi.rcMonitor.bottom ) / 2;
        SetCursorPos( ptCenter.x, ptCenter.y );
        m_ptLastMousePosition = ptCenter;
    }

    // Smooth the relative mouse data over a few frames so it isn't 
    // jerky when moving slowly at low frame rates.
    float fPercentOfNew = 1.0f / m_fFramesToSmoothMouseData;
    float fPercentOfOld = 1.0f - fPercentOfNew;

	//CHECKME
	//m_vMouseDelta.x = m_vMouseDelta.x * fPercentOfOld + ptCurMouseDelta.x * fPercentOfNew;
	//m_vMouseDelta.y = m_vMouseDelta.y * fPercentOfOld + ptCurMouseDelta.y * fPercentOfNew;
	//m_vRotVelocity = m_vMouseDelta * m_fRotationScaler;

	XMVECTOR vMouseDelta = XMLoadFloat2(&m_vMouseDelta);
	XMStoreFloat2(&m_vMouseDelta, XMVectorAdd( XMVectorScale(vMouseDelta, fPercentOfOld), XMVectorScale(curMouseDelta, fPercentOfNew)));
	//TODO better, see above
	//m_vMouseDelta = XMVectorSet(XMVectorGetX(m_vMouseDelta) * fPercentOfOld + ptCurMouseDelta.x * fPercentOfNew,
	//							XMVectorGetY(m_vMouseDelta) * fPercentOfOld + ptCurMouseDelta.y * fPercentOfNew,
	//							0,0);
    XMStoreFloat2(&m_vRotVelocity, XMVectorScale(vMouseDelta, m_fRotationScaler));
}




//--------------------------------------------------------------------------------------
// Figure out the velocity based on keyboard input & drag if any
//--------------------------------------------------------------------------------------
void CBaseCamera::UpdateVelocity( float fElapsedTime )
{
    XMMATRIX mRotDelta;
    //D3DXVECTOR2 vGamePadRightThumb = XMVectorSet( m_vGamePadRightThumb.x, -m_vGamePadRightThumb.z );
	XMVECTOR vGamePadRightThumb = XMVectorSet( m_vGamePadRightThumb.x, -m_vGamePadRightThumb.z, 0, 0); //XMVectorSet( XMVectorGetX(m_vGamePadRightThumb), -XMVectorGetZ(m_vGamePadRightThumb), 0, 0);
    //m_vRotVelocity = m_vMouseDelta * m_fRotationScaler + vGamePadRightThumb * 0.02f;
	XMStoreFloat2(&m_vRotVelocity, XMLoadFloat2(&m_vMouseDelta) * m_fRotationScaler + vGamePadRightThumb * 0.02f);

    //D3DXVECTOR3 vAccel = m_vKeyboardDirection + m_vGamePadLeftThumb;
	XMVECTOR vAccel = XMLoadFloat3(&m_vKeyboardDirection) + XMLoadFloat3(&m_vGamePadLeftThumb);

    // Normalize vector so if moving 2 dirs (left & forward), 
    // the camera doesn't move faster than if moving in 1 dir    
	//D3DXVec3Normalize( &vAccel, &vAccel );
	vAccel = XMVector3Normalize(vAccel);

    // Scale the acceleration vector
    vAccel *= m_fMoveScaler;

    if( m_bMovementDrag )
    {
        // Is there any acceleration this frame?
        //if( D3DXVec3LengthSq( &vAccel ) > 0 )
		if( XMVectorGetX(XMVector3LengthSq( vAccel )) > 0 )
        {
            // If so, then this means the user has pressed a movement key\
            // so change the velocity immediately to acceleration 
            // upon keyboard input.  This isn't normal physics
            // but it will give a quick response to keyboard input
            XMStoreFloat3(&m_vVelocity, vAccel);
            m_fDragTimer = m_fTotalDragTimeToZero;
            XMStoreFloat3(&m_vVelocityDrag, vAccel / m_fDragTimer);
        }
        else
        {
            // If no key being pressed, then slowly decrease velocity to 0
            if( m_fDragTimer > 0 )
            {
                // Drag until timer is <= 0
				XMVECTOR vel = XMLoadFloat3(&m_vVelocity);
				XMStoreFloat3(&m_vVelocity, XMVectorSubtract(vel, vel * fElapsedTime));
                //m_vVelocity -= m_vVelocityDrag * fElapsedTime;
				
                m_fDragTimer -= fElapsedTime;
            }
            else
            {
                // Zero velocity
                m_vVelocity = XMFLOAT3( 0, 0, 0);
            }
        }
    }
    else
    {
        // No drag, so immediately change the velocity
        //m_vVelocity = vAccel;
		XMStoreFloat3(&m_vVelocity, vAccel);
    }
}




//--------------------------------------------------------------------------------------
// Clamps pV to lie inside m_vMinBoundary & m_vMaxBoundary
//--------------------------------------------------------------------------------------
void CBaseCamera::ConstrainToBoundary( XMVECTOR* pV )
{
	*pV = XMVectorMax(*pV,XMLoadFloat3(&m_vMinBoundary));
    // Constrain vector to a bounding box 
    //pV->x = __max( pV->x, m_vMinBoundary.x );
    //pV->y = __max( pV->y, m_vMinBoundary.y );
    //pV->z = __max( pV->z, m_vMinBoundary.z );

	*pV = XMVectorMin(*pV,XMLoadFloat3(&m_vMaxBoundary));
    //pV->x = __min( pV->x, m_vMaxBoundary.x );
    //pV->y = __min( pV->y, m_vMaxBoundary.y );
    //pV->z = __min( pV->z, m_vMaxBoundary.z );
}




//--------------------------------------------------------------------------------------
// Maps a windows virtual key to an enum
//--------------------------------------------------------------------------------------
D3DUtil_CameraKeys CBaseCamera::MapKey( UINT nKey )
{
    // This could be upgraded to a method that's user-definable but for 
    // simplicity, we'll use a hardcoded mapping.
    switch( nKey )
    {
        case VK_CONTROL:
            return CAM_CONTROLDOWN;
        case VK_LEFT:
            return CAM_STRAFE_LEFT;
        case VK_RIGHT:
            return CAM_STRAFE_RIGHT;
        case VK_UP:
            return CAM_MOVE_FORWARD;
        case VK_DOWN:
            return CAM_MOVE_BACKWARD;
        case VK_PRIOR:
            return CAM_MOVE_UP;        // pgup
        case VK_NEXT:
            return CAM_MOVE_DOWN;      // pgdn

        case 'A':
            return CAM_STRAFE_LEFT;
        case 'D':
            return CAM_STRAFE_RIGHT;
        case 'W':
            return CAM_MOVE_FORWARD;
        case 'S':
            return CAM_MOVE_BACKWARD;
        case 'Q':
            return CAM_MOVE_DOWN;
        case 'E':
            return CAM_MOVE_UP;

        case VK_NUMPAD4:
            return CAM_STRAFE_LEFT;
        case VK_NUMPAD6:
            return CAM_STRAFE_RIGHT;
        case VK_NUMPAD8:
            return CAM_MOVE_FORWARD;
        case VK_NUMPAD2:
            return CAM_MOVE_BACKWARD;
        case VK_NUMPAD9:
            return CAM_MOVE_UP;
        case VK_NUMPAD3:
            return CAM_MOVE_DOWN;

        case VK_HOME:
            return CAM_RESET;
    }

    return CAM_UNKNOWN;
}




//--------------------------------------------------------------------------------------
// Reset the camera's position back to the default
//--------------------------------------------------------------------------------------
VOID CBaseCamera::Reset()
{
    SetViewParams( &m_vDefaultEye, &m_vDefaultLookAt );
}




//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
CFirstPersonCamera::CFirstPersonCamera() : m_nActiveButtonMask( 0x07 )
{
    m_bRotateWithoutButtonDown = false;
}




//--------------------------------------------------------------------------------------
// Update the view matrix based on user input & elapsed time
//--------------------------------------------------------------------------------------
VOID CFirstPersonCamera::FrameMove( FLOAT fElapsedTime )
{
    if( DXUTGetGlobalTimer()->IsStopped() ) {
        if (DXUTGetFPS() == 0.0f) fElapsedTime = 0;
        else fElapsedTime = 1.0f / DXUTGetFPS();
    }

    if( IsKeyDown( m_aKeys[CAM_RESET] ) )
        Reset();

    // Get keyboard/mouse/gamepad input
    GetInput( m_bEnablePositionMovement, ( m_nActiveButtonMask & m_nCurrentButtonMask ) || m_bRotateWithoutButtonDown,
              true, m_bResetCursorAfterMove );

    //// Get the mouse movement (if any) if the mouse button are down
    //if( (m_nActiveButtonMask & m_nCurrentButtonMask) || m_bRotateWithoutButtonDown )
    //    UpdateMouseDelta( fElapsedTime );

    // Get amount of velocity based on the keyboard input and drag (if any)
    UpdateVelocity( fElapsedTime );

    // Simple euler method to calculate position delta
    XMVECTOR vPosDelta = XMVectorScale(XMLoadFloat3(&m_vVelocity), fElapsedTime);

    // If rotating the camera 
    if( ( m_nActiveButtonMask & m_nCurrentButtonMask ) || m_bRotateWithoutButtonDown ||
        m_vGamePadRightThumb.x != 0 ||  m_vGamePadRightThumb.z != 0 )
    {
        // Update the pitch & yaw angle based on mouse movement
        float fYawDelta	  = m_vRotVelocity.x;
        float fPitchDelta = m_vRotVelocity.y;

        // Invert pitch if requested
        if( m_bInvertPitch )
            fPitchDelta = -fPitchDelta;

        m_fCameraPitchAngle += fPitchDelta;
        m_fCameraYawAngle += fYawDelta;

        // Limit pitch to straight up or straight down
        m_fCameraPitchAngle = __max( -XM_PIDIV2, m_fCameraPitchAngle );
        m_fCameraPitchAngle = __min( +XM_PIDIV2, m_fCameraPitchAngle );
    }

    // Make a rotation matrix based on the camera's yaw & pitch
	XMMATRIX mCameraRot = XMMatrixRotationRollPitchYaw(m_fCameraPitchAngle, m_fCameraYawAngle, 0);


    // Transform vectors based on camera's rotation matrix
	XMVECTOR vWorldUp; XMVECTOR vWorldAhead;
	XMVECTOR vLocalUp	 = XMVectorSet( 0.f, 1.f, 0.f, 0.f );
	XMVECTOR vLocalAhead = XMVectorSet( 0.f, 0.f, 1.f, 0.f );
	vWorldUp	= XMVector3TransformCoord( vLocalUp, mCameraRot );
	vWorldAhead = XMVector3TransformCoord( vLocalAhead, mCameraRot );



    // Transform the position delta by the camera's rotation 
	//CHECKME
    XMVECTOR vPosDeltaWorld;
    if( !m_bEnableYAxisMovement )
    {
        // If restricting Y movement, do not include pitch
        // when transforming position delta vector.        
		mCameraRot = XMMatrixRotationRollPitchYaw(0.0f, m_fCameraYawAngle, 0.0f );
    }
    vPosDeltaWorld = XMVector3TransformCoord(vPosDelta, mCameraRot); 

    // Move the eye position
	XMVECTOR vEye = XMLoadFloat3(&m_vEye);
	vEye += vPosDeltaWorld;
	
	if( m_bClipToBoundary )
	{
		//ConstrainToBoundary( &m_vEye );
        ConstrainToBoundary( &vEye );
	}
	XMStoreFloat3(&m_vEye, vEye);

    // Update the lookAt position based on the eye position 
    XMVECTOR vLookAt = XMLoadFloat3(&m_vLookAt);
	vLookAt = vEye + vWorldAhead;
	XMStoreFloat3(&m_vLookAt, vLookAt);

    // Update the view matrix
	m_mView = XMMatrixLookAtLH(vEye, vLookAt,vWorldUp);
	XMStoreFloat4x4(&m_mWorld, XMMatrixInverse(nullptr, m_mView));

}


//--------------------------------------------------------------------------------------
// Enable or disable each of the mouse buttons for rotation drag.
//--------------------------------------------------------------------------------------
void CFirstPersonCamera::SetRotateButtons( bool bLeft, bool bMiddle, bool bRight, bool bRotateWithoutButtonDown )
{
    m_nActiveButtonMask = ( bLeft ? MOUSE_LEFT_BUTTON : 0 ) |
        ( bMiddle ? MOUSE_MIDDLE_BUTTON : 0 ) |
        ( bRight ? MOUSE_RIGHT_BUTTON : 0 );
    m_bRotateWithoutButtonDown = bRotateWithoutButtonDown;
}


//--------------------------------------------------------------------------------------
// Constructor 
//--------------------------------------------------------------------------------------
CModelViewerCamera::CModelViewerCamera()
{

	XMStoreFloat4x4(&m_mModelWorld, XMMatrixIdentity( ));
	XMStoreFloat4x4(&m_mModelRot, XMMatrixIdentity( ));
	XMStoreFloat4x4(&m_mModelLastRot, XMMatrixIdentity( ));
	XMStoreFloat4x4(&m_mCameraRotLast, XMMatrixIdentity( ));

	m_vModelCenter  = XMFLOAT3( 0.f, 0.f, 0.f);
    m_fRadius = 5.0f;
    m_fDefaultRadius = 5.0f;
    m_fMinRadius = 1.0f;
    m_fMaxRadius = FLT_MAX;
    m_bLimitPitch = false;
    m_bEnablePositionMovement = false;
    m_bAttachCameraToModel = false;

    m_nRotateModelButtonMask = MOUSE_LEFT_BUTTON;
    m_nZoomButtonMask = MOUSE_WHEEL;
    m_nRotateCameraButtonMask = MOUSE_RIGHT_BUTTON;
    m_bDragSinceLastUpdate = true;
}




//--------------------------------------------------------------------------------------
// Update the view matrix & the model's world matrix based 
//       on user input & elapsed time
//--------------------------------------------------------------------------------------
VOID CModelViewerCamera::FrameMove( FLOAT fElapsedTime )
{
    if( IsKeyDown( m_aKeys[CAM_RESET] ) )
        Reset();

    // If no dragged has happend since last time FrameMove is called,
    // and no camera key is held down, then no need to handle again.
    if( !m_bDragSinceLastUpdate && 0 == m_cKeysDown )
        return;
    m_bDragSinceLastUpdate = false;

    //// If no mouse button is held down, 
    //// Get the mouse movement (if any) if the mouse button are down
    //if( m_nCurrentButtonMask != 0 ) 
    //    UpdateMouseDelta( fElapsedTime );

    GetInput( m_bEnablePositionMovement, m_nCurrentButtonMask != 0, true, false );

    // Get amount of velocity based on the keyboard input and drag (if any)
    UpdateVelocity( fElapsedTime );

    // Simple euler method to calculate position delta
    XMVECTOR vPosDelta = XMVectorScale(XMLoadFloat3(&m_vVelocity), fElapsedTime);

    // Change the radius from the camera to the model based on wheel scrolling
    if( m_nMouseWheelDelta && m_nZoomButtonMask == MOUSE_WHEEL )
        m_fRadius -= m_nMouseWheelDelta * m_fRadius * 0.1f / 800.0f;
    m_fRadius = __min( m_fMaxRadius, m_fRadius );
    m_fRadius = __max( m_fMinRadius, m_fRadius );
    m_nMouseWheelDelta = 0;

    // Get the inverse of the arcball's rotation matrix
	XMMATRIX mCameraRot =  XMMatrixInverse(nullptr, m_ViewArcBall.GetRotationMatrix()); 


    // Transform vectors based on camera's rotation matrix
	XMVECTOR vWorldUp; XMVECTOR vWorldAhead;
	XMVECTOR vLocalUp	 = XMVectorSet( 0.f, 1.f, 0.f, 0.f );
	XMVECTOR vLocalAhead = XMVectorSet( 0.f, 0.f, 1.f, 0.f );
	vWorldUp	= XMVector3TransformCoord( vLocalUp, mCameraRot );
	vWorldAhead = XMVector3TransformCoord( vLocalAhead, mCameraRot );


    // Transform the position delta by the camera's rotation 
    XMVECTOR vPosDeltaWorld;
	vPosDeltaWorld = XMVector3TransformCoord( vPosDelta, mCameraRot );

    // Move the lookAt position 
	XMVECTOR vLookAt = XMLoadFloat3(&m_vLookAt);
    vLookAt += vPosDeltaWorld;

    if( m_bClipToBoundary )
        ConstrainToBoundary( &vLookAt );

	XMStoreFloat3(&m_vLookAt, vLookAt);

    // Update the eye point based on a radius away from the lookAt position
	XMVECTOR vEye =  vLookAt - vWorldAhead * m_fRadius;
	XMStoreFloat3(&m_vEye, vEye);

    // Update the view matrix
	m_mView = XMMatrixLookAtLH(vEye, vLookAt, vWorldUp );

    XMMATRIX mInvView = XMMatrixInverse(nullptr, m_mView);
	XMStoreFloat4x4(&m_mWorld, mInvView);

	mInvView.r[3] = XMVectorSet(0.f, 0.f, 0.f, XMVectorGetW(mInvView.r[3]));	

    XMMATRIX mModelLastRotInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_mModelLastRot));

    // Accumulate the delta of the arcball's rotation in view space.
    // Note that per-frame delta rotations could be problematic over long periods of time.
	XMMATRIX mModelRot = m_ModelWorldArcBall.GetRotationMatrix();

	XMMATRIX tmpModelRot = XMLoadFloat4x4(&m_mModelRot);
    tmpModelRot *= m_mView * mModelLastRotInv * mModelRot * mInvView;

    if( m_ViewArcBall.IsBeingDragged() && m_bAttachCameraToModel && !IsKeyDown( m_aKeys[CAM_CONTROLDOWN] ) )
    {
        // Attach camera to model by inverse of the model rotation
        XMMATRIX mCameraLastRotInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_mCameraRotLast));
        XMMATRIX mCameraRotDelta = mCameraLastRotInv * mCameraRot; // local to world matrix
        tmpModelRot *= mCameraRotDelta;
    }
	XMStoreFloat4x4(&m_mCameraRotLast, mCameraRot);
	XMStoreFloat4x4(&m_mModelLastRot, mModelRot);


    // Since we're accumulating delta rotations, we need to orthonormalize 
    // the matrix to prevent eventual matrix skew
	XMVECTOR pXBasis = tmpModelRot.r[0];
	XMVECTOR pYBasis = tmpModelRot.r[1];
	XMVECTOR pZBasis = tmpModelRot.r[2];

	pXBasis = XMVector3Normalize( pXBasis );
	pYBasis = XMVector3Cross(pZBasis, pXBasis);
	pYBasis = XMVector3Normalize(pYBasis);	
	pZBasis = XMVector3Cross( pXBasis, pYBasis );

	tmpModelRot.r[0] = pXBasis;
	tmpModelRot.r[1] = pYBasis;
	tmpModelRot.r[2] = pZBasis;
	

    // Translate the rotation matrix to the same position as the lookAt position
	XMVECTOR tmpTrans = vLookAt;
	tmpTrans = XMVectorSetW(tmpTrans, XMVectorGetW(tmpModelRot.r[3]));	
	tmpModelRot.r[3] = tmpTrans;

    // Translate world matrix so its at the center of the model
    XMMATRIX mTrans = XMMatrixTranslationFromVector(XMVectorNegate(XMLoadFloat3(&m_vModelCenter)));
    XMStoreFloat4x4(&m_mModelWorld, mTrans * tmpModelRot);
	XMStoreFloat4x4(&m_mModelRot, tmpModelRot);
}


void CModelViewerCamera::SetDragRect( RECT& rc )
{
    CBaseCamera::SetDragRect( rc );

    m_ModelWorldArcBall.SetOffset( rc.left, rc.top );
    m_ViewArcBall.SetOffset( rc.left, rc.top );
    SetWindow( rc.right - rc.left, rc.bottom - rc.top );
}


//--------------------------------------------------------------------------------------
// Reset the camera's position back to the default
//--------------------------------------------------------------------------------------
VOID CModelViewerCamera::Reset()
{
    CBaseCamera::Reset();

    XMStoreFloat4x4(&m_mModelWorld, XMMatrixIdentity( ));
	XMStoreFloat4x4(&m_mModelRot, XMMatrixIdentity( ));
	XMStoreFloat4x4(&m_mModelLastRot, XMMatrixIdentity( ));
	XMStoreFloat4x4(&m_mCameraRotLast, XMMatrixIdentity( ));

    m_fRadius = m_fDefaultRadius;
    m_ModelWorldArcBall.Reset();
    m_ViewArcBall.Reset();
}


//--------------------------------------------------------------------------------------
// Override for setting the view parameters
//--------------------------------------------------------------------------------------
void CModelViewerCamera::SetViewParams( XMFLOAT3* pvEyePt, XMFLOAT3* pvLookatPt )
{
    CBaseCamera::SetViewParams( pvEyePt, pvLookatPt );

    // Propogate changes to the member arcball
    XMVECTOR quat;
    XMMATRIX mRotation;
    XMVECTOR vUp = XMVectorSet( 0.f, 0.f, 1.f, 0.f );	

	mRotation = XMMatrixLookAtLH(XMLoadFloat3(pvEyePt),XMLoadFloat3(pvLookatPt), vUp);
	quat = XMQuaternionRotationMatrix(mRotation);

    m_ViewArcBall.SetQuatNow( quat );

    // Set the radius according to the distance
    XMVECTOR vEyeToPoint;

	vEyeToPoint = XMVectorSubtract(XMLoadFloat3(pvLookatPt), XMLoadFloat3(pvEyePt) );	    
	SetRadius( XMVectorGetX( XMVector3Length(vEyeToPoint) ) );
	
    // View information changed. FrameMove should be called.
    m_bDragSinceLastUpdate = true;
}



//--------------------------------------------------------------------------------------
// Call this from your message proc so this class can handle window messages
//--------------------------------------------------------------------------------------
LRESULT CModelViewerCamera::HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    CBaseCamera::HandleMessages( hWnd, uMsg, wParam, lParam );

    if( ( ( uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK ) && m_nRotateModelButtonMask & MOUSE_LEFT_BUTTON ) ||
        ( ( uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK ) && m_nRotateModelButtonMask & MOUSE_MIDDLE_BUTTON ) ||
        ( ( uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK ) && m_nRotateModelButtonMask & MOUSE_RIGHT_BUTTON ) )
    {
        int iMouseX = ( short )LOWORD( lParam );
        int iMouseY = ( short )HIWORD( lParam );
        m_ModelWorldArcBall.OnBegin( iMouseX, iMouseY );
    }

    if( ( ( uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK ) && m_nRotateCameraButtonMask & MOUSE_LEFT_BUTTON ) ||
        ( ( uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK ) &&
          m_nRotateCameraButtonMask & MOUSE_MIDDLE_BUTTON ) ||
        ( ( uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK ) && m_nRotateCameraButtonMask & MOUSE_RIGHT_BUTTON ) )
    {
        int iMouseX = ( short )LOWORD( lParam );
        int iMouseY = ( short )HIWORD( lParam );
        m_ViewArcBall.OnBegin( iMouseX, iMouseY );
    }

    if( uMsg == WM_MOUSEMOVE )
    {
        int iMouseX = ( short )LOWORD( lParam );
        int iMouseY = ( short )HIWORD( lParam );
        m_ModelWorldArcBall.OnMove( iMouseX, iMouseY );
        m_ViewArcBall.OnMove( iMouseX, iMouseY );
    }

    if( ( uMsg == WM_LBUTTONUP && m_nRotateModelButtonMask & MOUSE_LEFT_BUTTON ) ||
        ( uMsg == WM_MBUTTONUP && m_nRotateModelButtonMask & MOUSE_MIDDLE_BUTTON ) ||
        ( uMsg == WM_RBUTTONUP && m_nRotateModelButtonMask & MOUSE_RIGHT_BUTTON ) )
    {
        m_ModelWorldArcBall.OnEnd();
    }

    if( ( uMsg == WM_LBUTTONUP && m_nRotateCameraButtonMask & MOUSE_LEFT_BUTTON ) ||
        ( uMsg == WM_MBUTTONUP && m_nRotateCameraButtonMask & MOUSE_MIDDLE_BUTTON ) ||
        ( uMsg == WM_RBUTTONUP && m_nRotateCameraButtonMask & MOUSE_RIGHT_BUTTON ) )
    {
        m_ViewArcBall.OnEnd();
    }

    if( uMsg == WM_CAPTURECHANGED )
    {
        if( ( HWND )lParam != hWnd )
        {
            if( ( m_nRotateModelButtonMask & MOUSE_LEFT_BUTTON ) ||
                ( m_nRotateModelButtonMask & MOUSE_MIDDLE_BUTTON ) ||
                ( m_nRotateModelButtonMask & MOUSE_RIGHT_BUTTON ) )
            {
                m_ModelWorldArcBall.OnEnd();
            }

            if( ( m_nRotateCameraButtonMask & MOUSE_LEFT_BUTTON ) ||
                ( m_nRotateCameraButtonMask & MOUSE_MIDDLE_BUTTON ) ||
                ( m_nRotateCameraButtonMask & MOUSE_RIGHT_BUTTON ) )
            {
                m_ViewArcBall.OnEnd();
            }
        }
    }

    if( uMsg == WM_LBUTTONDOWN ||
        uMsg == WM_LBUTTONDBLCLK ||
        uMsg == WM_MBUTTONDOWN ||
        uMsg == WM_MBUTTONDBLCLK ||
        uMsg == WM_RBUTTONDOWN ||
        uMsg == WM_RBUTTONDBLCLK ||
        uMsg == WM_LBUTTONUP ||
        uMsg == WM_MBUTTONUP ||
        uMsg == WM_RBUTTONUP ||
        uMsg == WM_MOUSEWHEEL ||
        uMsg == WM_MOUSEMOVE )
    {
        m_bDragSinceLastUpdate = true;
    }

    return FALSE;
}



