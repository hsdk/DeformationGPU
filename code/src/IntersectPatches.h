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
#include <SDX/DXBuffer.h>

// fwd decls
class ModelInstance;
class Brush;
class DXObjectOrientedBoundingBox;

enum class IntersectMode
{
	NONE = 0,
	Brush = 1,
	OBB = 2
};

enum class IntersectPatchType
{
	REGULAR = 0,
	GREGORY = 1
};

enum class IntersectFaceType 
{
	Triangle = 0,
	OSD = 1
};

union IntersectConfig{	// settings bitfield, check bit <32 to match int value for comparison
	struct {
		unsigned int face_mode		: 2;	// tri or subd
		unsigned int patch_type		: 2;	// regular or gregory
		unsigned int isct_mode		: 2;	// 0 obb, 1: brush
		unsigned int max_valence	: 7;
		unsigned int all_active		: 1;
		unsigned int batch_size		: 4;
		unsigned int use_maxdisp    : 1;
	}; 

	int value;

	IntersectConfig()	{	value = 0;	}
	bool operator < (const IntersectConfig &e) const	// for searching effect
	{
		return value < e.value;
	}
};


class EffectRegistryIntersect: public TEffectDrawRegistry<IntersectConfig> {

protected:
	virtual ConfigType * _CreateDrawConfig(	DescType const & desc,	SourceConfigType const * sconfig,
		ID3D11Device1 * pd3dDevice,	ID3D11InputLayout ** ppInputLayout,
		D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs, int numInputElements) const;

	virtual SourceConfigType *	_CreateDrawSourceConfig(DescType const & desc, ID3D11Device1 * pd3dDevice) const;
};

class IntersectGPU{
public:
	IntersectGPU();
	~IntersectGPU();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();
	
	HRESULT IntersectOBBBatch(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* deformable, const std::unordered_map<ModelInstance*, DXObjectOrientedBoundingBox>& batch );
	HRESULT SetAllActive(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance);

	void BindShaders( ID3D11DeviceContext1* pd3dImmediateContext, const IntersectConfig effect, const ModelInstance* instance) const ;
		 
	ID3D11UnorderedAccessView* GetIntersectedPatchesOSDRegularUAV(uint32_t i) const { return m_patchAppendRegular[i].UAV; }
	ID3D11ShaderResourceView*  GetIntersectedPatchesOSDRegularSRV(uint32_t i) const { return m_patchAppendRegular[i].SRV; }
																												 
	ID3D11UnorderedAccessView* GetIntersectedPatchesOSDGregoryUAV(uint32_t i) const { return m_patchAppendGregory[i].UAV; }
	ID3D11ShaderResourceView*  GetIntersectedPatchesOSDGregorySRV(uint32_t i) const { return m_patchAppendGregory[i].SRV; }

	unsigned int GetMaxValenceLastIntersected() const {return m_isctMeshMaxValence; }
	
	HRESULT CreateVisibilityBuffer(ID3D11Device1* pd3dDevice, UINT numTiles, ID3D11Buffer*& visibilityBUF, ID3D11ShaderResourceView*& visibilitySRV, ID3D11UnorderedAccessView*& visibilityUAV) const;
	HRESULT CreateCompactedVisibilityBuffer(ID3D11Device1* pd3dDevice, UINT numTiles, ID3D11Buffer*& visibilityBUF, ID3D11ShaderResourceView*& visibilitySRV, ID3D11UnorderedAccessView*& visibilityUAV) const;
private:
	HRESULT UpdateIntersectCB	(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance);
	void	ClearIntersectBuffer(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance);
	HRESULT IntersectOSDBatch(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance, uint32_t batchSize);

	Shader<ID3D11ComputeShader>	*m_intersectClear_CS;

	ID3D11Buffer				*m_osdConfigCB;			// constant buffer for osd patch config
	ID3D11Buffer				*m_intersectModelCB;
	ID3D11Buffer				*m_intersectOBBBatchCB;

	DirectX::DXBufferSRVUAV	m_patchAppendRegular[8];
	DirectX::DXBufferSRVUAV	m_patchAppendGregory[8];

	IntersectMode				m_intersectMode;
	unsigned int				m_isctMeshMaxValence;
	bool						m_setAllActive;
};

extern IntersectGPU g_intersectGPU;
extern EffectRegistryIntersect	g_intersectEffectRegistry;