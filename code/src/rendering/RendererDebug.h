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
#include <SDX/DXShaderManager.h>


class Scene;
class DXObjectOrientedBoundingBox;
class ModelGroup;
class ModelInstance;

class RendererBoundingGeometry{
public:
	RendererBoundingGeometry();
	~RendererBoundingGeometry();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	HRESULT Destroy();
	HRESULT RenderPhysicsAABB(Scene* scene, bool lines = true);
	HRESULT RenderPhysicsAABB( ModelGroup* group, bool lines = true); // for wheels

	// render oriented bounding box, show intersecting obbs in red
	HRESULT RenderOBB(const DXObjectOrientedBoundingBox* obb, bool isIntersectingOBB = false) const;
	HRESULT RenderControlCage(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance) const;

	HRESULT RenderFrustum(const DirectX::XMFLOAT3 cornerPoints[8]) const;

private:	
	HRESULT initCube();
	Shader<ID3D11VertexShader>* _aabbVS;
	Shader<ID3D11PixelShader>*  _aabbPS;
	
	Shader<ID3D11VertexShader>* _frustumVS;
	Shader<ID3D11PixelShader>*  _frustumPS;

	Shader<ID3D11VertexShader>* _controlCageVS;
	

	ID3D11Buffer*		_vertexBufferStatic;		// vertex position data
	ID3D11Buffer*		_solidIndicesBuffer;		// solid rendering index buffer
	ID3D11Buffer*		_linesIndicesBuffer;		// lines rendering index buffer
	ID3D11InputLayout*	_inputLayout;				// vertex shader input layout
	ID3D11Buffer*		_cbModelMatrix;				// AABB scale and model trafo

	ID3D11Buffer*		_vertexBufferDynamic;		// dynamic vertex position data
	ID3D11Buffer*		_indexBufferDynamic;		// dynamic writable index buffer


};