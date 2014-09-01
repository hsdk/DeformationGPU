//--------------------------------------------------------------------------------------
// File: Camera.h
//
// Helper functions for Direct3D programming.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//--------------------------------------------------------------------------------------
#pragma once
#ifndef CAMERA_H
#define CAMERA_H

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

//--------------------------------------------------------------------------------------
class CD3DArcBall
{	
public:
	CD3DArcBall();

	// Functions to change behavior
	void                            Reset();
	void                            SetTranslationRadius( FLOAT fRadiusTranslation )
	{
		m_fRadiusTranslation = fRadiusTranslation;
	}
	void                            SetWindow( INT nWidth, INT nHeight, FLOAT fRadius = 0.9f )
	{
		m_nWidth = nWidth; m_nHeight = nHeight; m_fRadius = fRadius;
		m_vCenter = DirectX::XMFLOAT2( m_nWidth / 2.0f, m_nHeight / 2.0f );
	}
	void                            SetOffset( INT nX, INT nY )
	{
		m_Offset.x = nX; m_Offset.y = nY;
	}

	// Call these from client and use GetRotationMatrix() to read new rotation matrix
	void                            OnBegin( int nX, int nY );  // start the rotation (pass current mouse position)
	void                            OnMove( int nX, int nY );   // continue the rotation (pass current mouse position)
	void                            OnEnd();                    // end the rotation 

	// Or call this to automatically handle left, middle, right buttons
	LRESULT                         HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	// Functions to get/set state
	const DirectX::XMMATRIX& GetRotationMatrix() 
	{
		//XMStoreFloat4x4(&m_mRotation, DirectX::XMMatrixRotationQuaternion(XMLoadFloat4(&m_qNow)));
		m_mRotation = DirectX::XMMatrixRotationQuaternion(m_qNow);
		return m_mRotation;
	};
	const DirectX::XMMATRIX& GetTranslationMatrix() const
	{
		return m_mTranslation;
	}
	const DirectX::XMMATRIX& GetTranslationDeltaMatrix() const
	{
		return m_mTranslationDelta;
	}
	bool                            IsBeingDragged() const
	{
		return m_bDrag;
	}
	DirectX::XMVECTOR               GetQuatNow() const
	{
		return m_qNow;
	}
	void                            SetQuatNow( DirectX::FXMVECTOR q )
	{
		//XMStoreFloat4(&m_qNow, q);
		m_qNow = q;
	}

	static DirectX::XMVECTOR WINAPI    QuatFromBallPoints( const DirectX::XMVECTOR& vFrom, const DirectX::XMVECTOR& vTo );


protected:
	DirectX::XMMATRIX m_mRotation;         // Matrix for arc ball's orientation
	DirectX::XMMATRIX m_mTranslation;      // Matrix for arc ball's position
	DirectX::XMMATRIX m_mTranslationDelta; // Matrix for arc ball's position

	DirectX::XMVECTOR m_qDown;             // Quaternion before button down
	DirectX::XMVECTOR m_qNow;              // Composite quaternion for current drag
	DirectX::XMVECTOR m_vDownPt;           // starting point of rotation arc
	DirectX::XMVECTOR m_vCurrentPt;        // current point of rotation arc

	POINT m_Offset;   // window offset, or upper-left corner of window
	INT m_nWidth;   // arc ball's window width
	INT m_nHeight;  // arc ball's window height
	DirectX::XMFLOAT2 m_vCenter;  // center of arc ball 
	FLOAT m_fRadius;  // arc ball's radius in screen coords
	FLOAT m_fRadiusTranslation; // arc ball's radius for translating the target

	bool m_bDrag;             // Whether user is dragging arc ball

	POINT m_ptLastMouse;      // position of last mouse point


	DirectX::XMVECTOR          ScreenToVector( float fScreenPtX, float fScreenPtY );
};


//--------------------------------------------------------------------------------------
// used by CCamera to map WM_KEYDOWN keys
//--------------------------------------------------------------------------------------
enum D3DUtil_CameraKeys
{
	CAM_STRAFE_LEFT = 0,
	CAM_STRAFE_RIGHT,
	CAM_MOVE_FORWARD,
	CAM_MOVE_BACKWARD,
	CAM_MOVE_UP,
	CAM_MOVE_DOWN,
	CAM_RESET,
	CAM_CONTROLDOWN,
	CAM_MAX_KEYS,
	CAM_UNKNOWN     = 0xFF
};

#define KEY_WAS_DOWN_MASK 0x80
#define KEY_IS_DOWN_MASK  0x01

#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_MIDDLE_BUTTON 0x02
#define MOUSE_RIGHT_BUTTON  0x04
#define MOUSE_WHEEL         0x08


//--------------------------------------------------------------------------------------
// Simple base camera class that moves and rotates.  The base class
//       records mouse and keyboard input for use by a derived class, and 
//       keeps common state.
//--------------------------------------------------------------------------------------
class CBaseCamera
{
public:
	CBaseCamera();

	// Call these from client and use Get*Matrix() to read new matrices
	virtual LRESULT             HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual void                FrameMove( FLOAT fElapsedTime ) = 0;

	// Functions to change camera matrices
	virtual void                Reset();
	virtual void                SetViewParams( DirectX::XMFLOAT3* pvEyePt, DirectX::XMFLOAT3* pvLookatPt );
	virtual void                SetProjParams( FLOAT fFOV, FLOAT fAspect, FLOAT fNearPlane, FLOAT fFarPlane );

	// Functions to change behavior
	virtual void                SetDragRect( RECT& rc )
	{
		m_rcDrag = rc;
	}
	void                        SetInvertPitch( bool bInvertPitch )
	{
		m_bInvertPitch = bInvertPitch;
	}
	void                        SetDrag( bool bMovementDrag, FLOAT fTotalDragTimeToZero = 0.25f )
	{
		m_bMovementDrag = bMovementDrag; m_fTotalDragTimeToZero = fTotalDragTimeToZero;
	}
	void                        SetEnableYAxisMovement( bool bEnableYAxisMovement )
	{
		m_bEnableYAxisMovement = bEnableYAxisMovement;
	}
	void                        SetEnablePositionMovement( bool bEnablePositionMovement )
	{
		m_bEnablePositionMovement = bEnablePositionMovement;
	}
	void                        SetClipToBoundary( bool bClipToBoundary, DirectX::XMFLOAT3 pvMinBoundary, DirectX::XMFLOAT3 pvMaxBoundary )
	{
		m_bClipToBoundary = bClipToBoundary;
		//if( pvMinBoundary ) XMStoreFloat3(&m_vMinBoundary, *pvMinBoundary);
		//if( pvMaxBoundary ) XMStoreFloat3(&m_vMaxBoundary, *pvMaxBoundary);

		//if( pvMinBoundary ) XMStoreFloat3(&m_vMinBoundary, *pvMinBoundary);
		//if( pvMaxBoundary ) XMStoreFloat3(&m_vMaxBoundary, *pvMaxBoundary);

		m_vMinBoundary = pvMinBoundary;
		m_vMaxBoundary = pvMaxBoundary;
	}
	void                        SetScalers( FLOAT fRotationScaler = 0.01f, FLOAT fMoveScaler = 5.0f )
	{
		m_fRotationScaler = fRotationScaler; m_fMoveScaler = fMoveScaler;
	}
	void                        SetNumberOfFramesToSmoothMouseData( int nFrames )
	{
		if( nFrames > 0 ) m_fFramesToSmoothMouseData = ( float )nFrames;
	}
	void                        SetResetCursorAfterMove( bool bResetCursorAfterMove )
	{
		m_bResetCursorAfterMove = bResetCursorAfterMove;
	}

	// Functions to get state
	__forceinline const DirectX::XMMATRIX& GetViewMatrix() const
	{
		return m_mView;
	}

	__forceinline const DirectX::XMMATRIX& GetProjMatrix() const
	{
		return m_mProj;
	}

	//__forceinline const DirectX::XMFLOAT3* GetEyePt() const
	//{
	//	return &m_vEye;
	//}
	__forceinline const DirectX::XMFLOAT3* GetLookAtPt() const
	{
		return &m_vLookAt;
	}

	__forceinline float                       GetNearClip() const
	{
		return m_fNearPlane;
	}
	
	__forceinline float                       GetFarClip() const
	{
		return m_fFarPlane;
	}

	bool                        IsBeingDragged() const
	{
		return ( m_bMouseLButtonDown || m_bMouseMButtonDown || m_bMouseRButtonDown );
	}
	bool                        IsMouseLButtonDown() const
	{
		return m_bMouseLButtonDown;
	}
	bool                        IsMouseMButtonDown() const
	{
		return m_bMouseMButtonDown;
	}
	bool                        IsMouseRButtonDown() const
	{
		return m_bMouseRButtonDown;
	}

	DirectX::XMFLOAT4X4* GetWorldMatrix()
	{
		return &m_mWorld;
	}


	__forceinline const DirectX::XMFLOAT3 GetWorldUp() const
	{		
		return DirectX::XMFLOAT3(m_mWorld._21, m_mWorld._22, m_mWorld._23);//, m_mWorld._24);
	}

	__forceinline const DirectX::XMFLOAT3 GetWorldDown() const
	{		
		return DirectX::XMFLOAT3(-m_mWorld._21, -m_mWorld._22, -m_mWorld._23);//, m_mWorld._24);
	}

	__forceinline const DirectX::XMFLOAT3 GetWorldRight() const
	{			
		return DirectX::XMFLOAT3(m_mWorld._11, m_mWorld._12, m_mWorld._13);//, m_mWorld._14);		
	}

	__forceinline const DirectX::XMFLOAT3 GetWorldLeft() const
	{			
		return DirectX::XMFLOAT3(-m_mWorld._11, -m_mWorld._12, -m_mWorld._13);//, m_mWorld._14);		
	}

	__forceinline const DirectX::XMFLOAT3 GetWorldAhead() const
	{
		return DirectX::XMFLOAT3(m_mWorld._31, m_mWorld._32, m_mWorld._33);//, m_mWorld._34);
	}

	__forceinline const DirectX::XMFLOAT3 GetWorldBack() const
	{
		return DirectX::XMFLOAT3(-m_mWorld._31, -m_mWorld._32, -m_mWorld._33);//, m_mWorld._34);
	}

	__forceinline const DirectX::XMFLOAT3 GetWorldTranslation() const
	{			
		return DirectX::XMFLOAT3(m_mWorld._41, m_mWorld._42, m_mWorld._43);//, m_mWorld._44);		
	}


	__forceinline const DirectX::XMFLOAT3* GetEyePt() const
	{
		return (DirectX::XMFLOAT3*)&m_mWorld._41;
		//return DirectX::XMFLOAT4(m_mCameraWorld._41, m_mCameraWorld._42, m_mCameraWorld._43, m_mCameraWorld._44);
	}

protected:
	// Functions to map a WM_KEYDOWN key to a D3DUtil_CameraKeys enum
	virtual D3DUtil_CameraKeys  MapKey( UINT nKey );
	bool                        IsKeyDown( BYTE key ) const
	{
		return( ( key & KEY_IS_DOWN_MASK ) == KEY_IS_DOWN_MASK );
	}
	bool                        WasKeyDown( BYTE key ) const
	{
		return( ( key & KEY_WAS_DOWN_MASK ) == KEY_WAS_DOWN_MASK );
	}

	void                        ConstrainToBoundary( DirectX::XMVECTOR* pV );
	void                        UpdateMouseDelta();
	void                        UpdateVelocity( float fElapsedTime );
	void                        GetInput( bool bGetKeyboardInput, bool bGetMouseInput, bool bGetGamepadInput,
		bool bResetCursorAfterMove );

	//DirectX::XMMATRIX m_mView;              // View matrix 
	DirectX::XMMATRIX m_mView;              // View matrix 
	DirectX::XMMATRIX m_mProj;              // Projection matrix
	DirectX::XMFLOAT4X4 m_mWorld;       // World matrix of the camera (inverse of the view matrix)

	DirectX::XMFLOAT3			m_vGamePadLeftThumb;
	DirectX::XMFLOAT3			m_vGamePadRightThumb;
	DXUT_GAMEPAD                m_GamePad[DXUT_MAX_CONTROLLERS];  // XInput controller state
	double                      m_GamePadLastActive[DXUT_MAX_CONTROLLERS];

	int m_cKeysDown;            // Number of camera keys that are down.
	BYTE                        m_aKeys[CAM_MAX_KEYS];  // State of input - KEY_WAS_DOWN_MASK|KEY_IS_DOWN_MASK
	DirectX::XMFLOAT3			m_vKeyboardDirection;   // Direction vector of keyboard input
	POINT m_ptLastMousePosition;			// Last absolute position of mouse cursor
	bool m_bMouseLButtonDown;				// True if left button is down 
	bool m_bMouseMButtonDown;				// True if middle button is down 
	bool m_bMouseRButtonDown;				// True if right button is down 
	int m_nCurrentButtonMask;				// mask of which buttons are down
	int m_nMouseWheelDelta;					// Amount of middle wheel scroll (+/-) 
	DirectX::XMFLOAT2 m_vMouseDelta;        // Mouse relative delta smoothed over a few frames
	float m_fFramesToSmoothMouseData;		// Number of frames to smooth mouse data over

	DirectX::XMFLOAT3  m_vDefaultEye;       // Default camera eye position
	DirectX::XMFLOAT3  m_vDefaultLookAt;    // Default LookAt position
	DirectX::XMFLOAT3  m_vEye;              // Camera eye position
	DirectX::XMFLOAT3  m_vLookAt;           // LookAt position
	float m_fCameraYawAngle;				// Yaw angle of camera
	float m_fCameraPitchAngle;				// Pitch angle of camera

	RECT m_rcDrag;							// Rectangle within which a drag can be initiated.
	DirectX::XMFLOAT3  m_vVelocity;         // Velocity of camera
	bool m_bMovementDrag;					// If true, then camera movement will slow to a stop otherwise movement is instant
	DirectX::XMFLOAT3  m_vVelocityDrag;     // Velocity drag force
	FLOAT m_fDragTimer;						// Countdown timer to apply drag
	FLOAT m_fTotalDragTimeToZero;			// Time it takes for velocity to go from full to 0
	DirectX::XMFLOAT2 m_vRotVelocity;       // Velocity of camera

	float m_fFOV;                 // Field of view
	float m_fAspect;              // Aspect ratio
	float m_fNearPlane;           // Near plane
	float m_fFarPlane;            // Far plane

	float m_fRotationScaler;      // Scaler for rotation
	float m_fMoveScaler;          // Scaler for movement

	bool m_bInvertPitch;			// Invert the pitch axis
	bool m_bEnablePositionMovement; // If true, then the user can translate the camera/model 
	bool m_bEnableYAxisMovement;	// If true, then camera can move in the y-axis

	bool m_bClipToBoundary;			// If true, then the camera will be clipped to the boundary
	DirectX::XMFLOAT3 m_vMinBoundary;         // Min point in clip boundary
	DirectX::XMFLOAT3 m_vMaxBoundary;         // Max point in clip boundary

	bool m_bResetCursorAfterMove;// If true, the class will reset the cursor position so that the cursor always has space to move 
};


//--------------------------------------------------------------------------------------
// Simple first person camera class that moves and rotates.
//       It allows yaw and pitch but not roll.  It uses WM_KEYDOWN and 
//       GetCursorPos() to respond to keyboard and mouse input and updates the 
//       view matrix based on input.  
//--------------------------------------------------------------------------------------
class CFirstPersonCamera : public CBaseCamera
{
public:
	CFirstPersonCamera();

	// Call these from client and use Get*Matrix() to read new matrices
	virtual void    FrameMove( FLOAT fElapsedTime );

	// Functions to change behavior
	void            SetRotateButtons( bool bLeft, bool bMiddle, bool bRight, bool bRotateWithoutButtonDown = false );

	// Functions to get state


protected:


	int m_nActiveButtonMask;  // Mask to determine which button to enable for rotation
	bool m_bRotateWithoutButtonDown;
};


//--------------------------------------------------------------------------------------
// Simple model viewing camera class that rotates around the object.
//--------------------------------------------------------------------------------------
class CModelViewerCamera : public CBaseCamera
{
public:
	CModelViewerCamera();

	// Call these from client and use Get*Matrix() to read new matrices
	virtual LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual void    FrameMove( FLOAT fElapsedTime );


	// Functions to change behavior
	virtual void    SetDragRect( RECT& rc );
	void            Reset();
	void            SetViewParams( DirectX::XMFLOAT3* pvEyePt, DirectX::XMFLOAT3* pvLookatPt );
	void            SetButtonMasks( int nRotateModelButtonMask = MOUSE_LEFT_BUTTON, int nZoomButtonMask = MOUSE_WHEEL,
		int nRotateCameraButtonMask = MOUSE_RIGHT_BUTTON )
	{
		m_nRotateModelButtonMask = nRotateModelButtonMask, m_nZoomButtonMask = nZoomButtonMask;
		m_nRotateCameraButtonMask = nRotateCameraButtonMask;
	}
	void            SetAttachCameraToModel( bool bEnable = false )
	{
		m_bAttachCameraToModel = bEnable;
	}
	void            SetWindow( int nWidth, int nHeight, float fArcballRadius=0.9f )
	{
		m_ModelWorldArcBall.SetWindow( nWidth, nHeight, fArcballRadius );
		m_ViewArcBall.SetWindow( nWidth, nHeight, fArcballRadius );
	}
	void            SetRadius( float fDefaultRadius=5.0f, float fMinRadius=1.0f, float fMaxRadius=FLT_MAX )
	{
		m_fDefaultRadius = m_fRadius = fDefaultRadius; m_fMinRadius = fMinRadius; m_fMaxRadius = fMaxRadius;
		m_bDragSinceLastUpdate = true;
	}
	void            SetModelCenter( DirectX::FXMVECTOR vModelCenter )
	{
		//m_vModelCenter = vModelCenter;
		XMStoreFloat3(&m_vModelCenter, vModelCenter);
	}
	void            SetLimitPitch( bool bLimitPitch )
	{
		m_bLimitPitch = bLimitPitch;
	}
	void            SetViewQuat( DirectX::FXMVECTOR q )
	{
		m_ViewArcBall.SetQuatNow( q ); m_bDragSinceLastUpdate = true;
	}

	void            GetViewQuat( DirectX::XMVECTOR& q ) const
	{
		q = m_ViewArcBall.GetQuatNow(); //m_bDragSinceLastUpdate = true;
	}

	void            SetModelWorldQuat( DirectX::FXMVECTOR q )
	{
		m_ModelWorldArcBall.SetQuatNow( q ); m_bDragSinceLastUpdate = true;
	}

	// Functions to get state
	const DirectX::XMFLOAT4X4* GetModelWorldMatrix() const
	{
		return &m_mModelWorld;
	}
	void            SetModelWorldMatrix( DirectX::XMMATRIX& mWorld )
	{
		//m_mWorld = mWorld;
		XMStoreFloat4x4(&m_mModelWorld, mWorld);
		m_bDragSinceLastUpdate = true;
	}

protected:
	CD3DArcBall m_ModelWorldArcBall;
	CD3DArcBall m_ViewArcBall;
	DirectX::XMFLOAT3 m_vModelCenter;
	DirectX::XMFLOAT4X4 m_mModelLastRot;        // Last arcball rotation matrix for model 
	DirectX::XMFLOAT4X4 m_mModelRot;            // Rotation matrix of model
	DirectX::XMFLOAT4X4 m_mModelWorld;               // World matrix of model

	int m_nRotateModelButtonMask;
	int m_nZoomButtonMask;
	int m_nRotateCameraButtonMask;

	bool m_bAttachCameraToModel;
	bool m_bLimitPitch;
	float m_fRadius;              // Distance from the camera to model 
	float m_fDefaultRadius;       // Distance from the camera to model 
	float m_fMinRadius;           // Min radius
	float m_fMaxRadius;           // Max radius
	bool m_bDragSinceLastUpdate; // True if mouse drag has happened since last time FrameMove is called.

	DirectX::XMFLOAT4X4 m_mCameraRotLast;

};


#endif
