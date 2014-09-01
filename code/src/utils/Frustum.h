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

#pragma once

#include <DXUT.h>

__declspec(align(16)) struct Frustum
{
	DirectX::XMFLOAT3 Origin;		// origin of frustum and projection
	DirectX::XMFLOAT4 Orientation;  // Quaternion representing rotation

	FLOAT RightSlope;				// positive X slope (X/Z)
	FLOAT LeftSlope;				// negative X slope
	FLOAT TopSlope;					// positive Y slope (Y/Z)
	FLOAT BottomSlope;				// negative Y slope

	FLOAT Near, Far;
};

void ComputeFrustumFromProjection( Frustum* pOut, const DirectX::XMMATRIX& pProjection );

