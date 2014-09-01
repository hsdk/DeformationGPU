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
#include <osd/d3d11DrawRegistry.h>

class DXOSDMesh;
class ModelInstance;

__declspec(align(16))
struct CBOSDTessellation {
	float TessLevel;
	int GregoryQuadOffsetBase;
	int PrimitiveIdBase;
};

__declspec(align(16))
struct CBOSDConfig {
	float displacementScale;
	float mipmapBias;
};

union OSDEffect{	// settings bitfield, check bit <32 to match int value for comparison
	struct {
		unsigned int wire : 2;		// 0 on, 1, shaded, 2 wireframe overlay
		unsigned int color : 3;		// 0 none, 1 ptex nearest, 2 ptex hw bilinear, 3 ptex bilinear, 4 ptex biquadratic, 5 patch type, 6 patch coord, 7 normals
		unsigned int displacement: 3;
		unsigned int normal : 3;
		unsigned int occlusion : 1;
		unsigned int specular: 1;
		unsigned int patchCull: 1;			// hull shader frustum patch culling
		unsigned int screenSpaceTess:1;		
		unsigned int fractionalSpacing: 1;	// with fractional
		unsigned int ibl: 1;					// ?
		unsigned int seamless : 1;			// seamless mipmap

		unsigned int ssao   : 1;				// output for ssao
		unsigned int shadow : 3;	// 0 none, 1, generate, 2 use during rendering, 3 generate shadow gs : generate outputs depth, depth^2 and no color
		unsigned int picking : 1;	// 0 disabled, 1 enabled
		unsigned int showAllocs : 1;
		unsigned int quad : 1;
		unsigned int numShadowCascades:1;	// is 3 or 4, 
		unsigned int colorUV : 1;			// 0: disable, 1: enable uv atlas tex
		unsigned int showisct:1;
		unsigned int voxelize : 2;			// 0: off, 1 forward, 2 backward
		
		// int dynamic : 3			// memory management flags: 0 all static, 1 dynamic displacement, 2 dynamic color, 3 dynamic displacement and color

	}; 
	int value;

	bool operator < (const OSDEffect &e) const	// for searching effect
	{
		return value < e.value;
	}
};

typedef std::pair<OpenSubdiv::OsdDrawContext::PatchDescriptor, OSDEffect> OSDEffectDesc;

class EffectDrawRegistry : public OpenSubdiv::OsdD3D11DrawRegistry<OSDEffectDesc> {

protected:
	virtual ConfigType * _CreateDrawConfig(	DescType const & desc,	SourceConfigType const * sconfig,
											ID3D11Device1 * pd3dDevice,	ID3D11InputLayout ** ppInputLayout,
											D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs, int numInputElements);

	virtual SourceConfigType *	_CreateDrawSourceConfig(DescType const & desc, ID3D11Device1 * pd3dDevice);
};

class RendererSubD{
public:
	RendererSubD();
	~RendererSubD();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	HRESULT FrameRender( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* model) const ;
	//HRESULT FrameRender( ID3D11DeviceContext1* pd3dImmediateContext, DXOSDMesh* mesh  );
	HRESULT Voxelize(ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* model, bool withCulling );
		
	//void SetTessLevel(int tessLevel) {assert (tessLevel<8); _tessLevel = tessLevel;}

	void Destroy();
	enum DisplayType {		DISPLAY_WIRE,
							DISPLAY_SHADED,
							DISPLAY_WIRE_ON_SHADED };

	enum ColorType {		COLOR_NONE,
							COLOR_PTEX_NEAREST,
							COLOR_PTEX_HW_BILINEAR,
							COLOR_PTEX_BILINEAR,
							COLOR_PTEX_BIQUADRATIC,
							COLOR_PATCHTYPE,
							COLOR_PATCHCOORD,
							COLOR_NORMAL };
	

	enum DisplacementType { DISPLACEMENT_NONE,
							DISPLACEMENT_HW_BILINEAR,
							DISPLACEMENT_BILINEAR,
							DISPLACEMENT_BIQUADRATIC,
							DISPLACEMENT_UV_TEX};

	enum NormalType {		NORMAL_SURFACE,
							NORMAL_FACET,
							NORMAL_HW_SCREENSPACE,
							NORMAL_SCREENSPACE,
							NORMAL_BIQUADRATIC,
							NORMAL_BIQUADRATIC_WG,
							NORMAL_UV_TEX};

	enum ShadowType {
							SHADOW_NONE = 0,
							SHADOW_GENERATE = 1,
							SHADOW_ENABLED = 2,
							SHADOW_GENERATE_GS = 3
					};

	enum Voxelization {
		VOXELIZATION_NONE = 0,
		VOXELIZATION_FORWARD = 1,
		VOXELIZATION_BACKWARD = 2,		
	};

	enum PickingType {
		PICKING_DISABLED,
		PICKING_ENABLED		
	};

	void SetWithSSAO(bool b)	{ _bWithSSAO  = b;}
	void SetGenShadows(bool b)	{ _bGenShadow = b; }								 				  
	void SetGenShadowsFast(bool b)	{ _bGenShadowFast = b; }		
	void SetUseShadows(bool b)	{ _bUseShadow = b; }

	void SetVoxelize(bool b)	{ _bGenVoxelization = b;			}
	bool GetVoxelize() const	{ return _bGenVoxelization;		}

	void SetWireframeOverlay(bool b) { _bWireFrameOverlay = b;}
	
protected:
	void BindShaders( ID3D11DeviceContext1* pd3dImmediateContext, OSDEffect effect, DXOSDMesh* mesh, OpenSubdiv::OsdDrawContext::PatchArray const & patch, ModelInstance* instance) const;
	HRESULT FrameRenderInternal( ID3D11DeviceContext1* pd3dImmediateContext, ModelInstance* instance ) const;


	ID3D11Buffer*			_pcbTessellation;
	ID3D11Buffer*			_pcbConfig;
	ID3D11SamplerState*		_samplerLinear;

	// global config
	//int						_tessLevel;
	
	bool					_bGenShadow;
	bool					_bGenShadowFast;
	bool					_bUseShadow;
	bool					_bGenVoxelization;
	bool					_bWithSSAO;

	bool					_bWireFrameOverlay;
	
	bool					_voxelizeMode;
	ID3D11InputLayout*		_inputLayout;
	ID3D11InputLayout*		_inputLayoutTexCoords;
		
};
extern RendererSubD		  g_rendererSubD;
extern EffectDrawRegistry g_osdEffectRegistry;