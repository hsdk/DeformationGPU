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
#include <map>

class  Scene;
class  ModelGroup;
class  ModelInstance;
struct DXModel;
class  TriangleSubmesh;
struct DXMaterial;

union Effect{	// settings bitfield, check bit <32 to match int value for comparison
	struct {
		unsigned int color		: 2;		// 0 none, 1 uv color tex, 2 multires attributes tex, 3 light baking tex
		unsigned int displacement: 2;		// 0 none, 1 uv displ tex, 2 multires attributes tex
		unsigned int normal		: 1;		// 0 none, 1 normal tex	
		unsigned int alpha		: 1;		// 1 with alpha tex
		unsigned int specular	: 1;		// 1 none, 1 specular tex		
		unsigned int patchCull	: 1;		// 1 with hull shader frustum patch culling
		unsigned int screenSpaceTess:1;		// 1 with screen space tessellation
		unsigned int fractionalSpacing: 1;	// 0 integer, 1 fractional tessellation
		unsigned int voxelize	: 1;		// 1 enable voxelize
		unsigned int shadow		: 3;		// 0 none, 1, generate, 2 use during rendering, 3 generate shadow gs : generate outputs depth, depth^2 and no color				
		unsigned int ssao		: 1;		// output for ssao
		unsigned int quad		: 1;		// 0 triangle, 1 multires attributes on quads		
		unsigned int multiresAttributes : 1; // 1 use multires attributes rendering
		unsigned int numShadowCascades : 1; 
		unsigned int rendertype	: 3;		// 0 standard, 1 pn-triangles, 2, phong-tessellation, 3: tess render tests
		unsigned int wire : 2;		// 0 off, 1 fill, 2 line
		unsigned int envmap : 1;
	}; 
	int value;

	Effect()	{	value = 0;	}
	bool operator < (const Effect &e) const	// for searching effect
	{
		return value < e.value;
	}
};

enum TriDisplayType {	DISPLAY_SHADED,
						DISPLAY_WIRE,						
						DISPLAY_WIRE_ON_SHADED };

enum class TriColorMode : uint32_t
{
	NONE		  = 0,
	TEX			  = 1,
	MULTIRESATTR  = 2,
	LIGHTBAKING   = 3
};

enum class TriDisplacementMode : uint32_t
{
	NONE		  = 0,
	TEX			  = 1,
	MULTIRESATTR  = 2,	
};

enum class TriShadowMode : uint32_t
{
	NONE			= 0,
	GENERATE		= 1,
	RENDER			= 2,	
	GENERATE_GS		= 3
};

enum class TriangleWireframe
{
	NONE = 0,
	OVERLAY = 1,
	LINES = 2
};

class EffectDrawRegistryTriangle: public TEffectDrawRegistry<Effect> {

protected:
	virtual ConfigType * _CreateDrawConfig(	DescType const & desc,	SourceConfigType const * sconfig,
		ID3D11Device1 * pd3dDevice,	ID3D11InputLayout ** ppInputLayout,
		D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs, int numInputElements) const;

	virtual SourceConfigType *	_CreateDrawSourceConfig(DescType const & desc, ID3D11Device1 * pd3dDevice) const;
};

class RenderTri{
public:


	RenderTri();
	~RenderTri();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	HRESULT Destroy();
	
	//HRESULT FrameRender( ID3D11DeviceContext1* pd3dImmediateContext, const DXModel* model ) const;
	HRESULT FrameRender( ID3D11DeviceContext1* pd3dImmediateContext, const ModelInstance* instance ) const;
	HRESULT FrameRenderSkydome( ID3D11DeviceContext1* pd3dImmediateContext, const DXModel* model ) const;

	HRESULT Voxelize(ID3D11DeviceContext1* pd3dImmediateContext, const ModelInstance* instance) const;
		
	void SetRenderLines(bool b)		{ m_renderLines = b; }
	void SetUseSSAO(bool b)			{ m_UseSSAO = b; }
	void SetUseShadows(bool b)		{ m_UseShadows = b; }
	void SetGenShadows(bool b)		{ m_GenShadows = b; }	
	void SetGenShadowsFast(bool b)	{ m_GenShadowFast = b; }		
	VOID SetWireframeOverlay(bool b) {m_UseWireframeOverlay = b; }
		
private:	
	void BindShaders( ID3D11DeviceContext1* pd3dImmediateContext, const Effect effect, const ModelInstance* instance, const DXMaterial* material) const ;
	HRESULT FrameRenderInternal( ID3D11DeviceContext1* pd3dImmediateContext, const ModelInstance* instance ) const;
	HRESULT VoxelizeInternal(ID3D11DeviceContext1* pd3dImmediateContext, const ModelInstance* instance) const;

	//bool							m_VoxelizeMode;
	bool							m_renderLines;				// TODO implement lines index buffer
	bool							m_UseSSAO;			
	bool							m_UseShadows;
	bool							m_GenShadows;
	bool							m_GenShadowFast;
	bool							m_UseWireframeOverlay;
	
	ID3D11InputLayout*				m_inputLayout;				// vertex shader input layout
	ID3D11SamplerState*				m_AnisotropicWrapSampler;
	//ID3D11SamplerState*				m_envMapSampler;


	Shader<ID3D11VertexShader>* m_colorVS;
	Shader<ID3D11PixelShader>*  m_tex_ssao_shadowPS;

	Shader<ID3D11VertexShader>* m_SkydomeVS;
	Shader<ID3D11PixelShader>*  m_SkydomePS;

	Shader<ID3D11PixelShader>*	m_PSVoxelizeSolidBackward;
	Shader<ID3D11PixelShader>*	m_PSVoxelizeSolidForward;

};

extern RenderTri					g_renderTriMeshes;
extern EffectDrawRegistryTriangle	g_triEffectRegistry;
