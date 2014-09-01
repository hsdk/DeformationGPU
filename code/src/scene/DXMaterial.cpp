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
#include <DXUT.h>
#include "App.h"
#include "DXMaterial.h"
#include <SDX/DXBuffer.h>

#include "utils/DbgNew.h" // has to be last include

using namespace DirectX;

DXMaterial::DXMaterial()	
{
	_name				= "";
	_kAmbient			= XMFLOAT3(0.2f,0.2f,0.2f);
	_specialMaterialFlag= 0;
	_kDiffuse			= XMFLOAT3(0,0,0);
	_kSpecular			= XMFLOAT3(0,0,0);
	_shininess			= 90.f;
	_smoothness			= 0.25;
	_cbMat				= NULL;		

	_hasDiffuseTex		= false;
	_hasNormalsTex		= false;
	_hasDisplacementTex	= false;		
	_hasAlphaTex		= false;
	_hasSpecularTex		= false;

	_diffuseTexSRV		= NULL;
	_normalsTexSRV		= NULL;
	_displTexSRV		= NULL;
	_alphaTexSRV		= NULL;
	_specularTexSRV		= NULL;

	_displacementScalar  = 1.f;
	_dispTileFileName = "";
}

DXMaterial::~DXMaterial()
{
	SAFE_RELEASE(_cbMat);
	// textures are deleted by texture cache
}

HRESULT DXMaterial::initCB()
{
	HRESULT hr =  S_OK;

	CB_MATERIAL mat;
	mat.kAmbient	= _kAmbient;
	mat.magicFlag	= _specialMaterialFlag;
	mat.kDiffuse	= _kDiffuse;
	mat.smoothness	= _smoothness;
	mat.kSpecular	= _kSpecular;
	mat.Nshininess	= _shininess;


	V_RETURN(DXUTCreateBuffer(DXUTGetD3D11Device(), D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_MATERIAL), 0, D3D11_USAGE_IMMUTABLE, _cbMat, &mat));
	return hr;
}
