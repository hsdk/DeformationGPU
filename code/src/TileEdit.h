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

class ModelInstance;
class IntersectGPU;
class Voxelizable;

//! Painting/Sculpting on meshes using brushes and gpu memory management
// support: tri and adaptive subdiv meshes
// TODO find better name

// Steps
//  ||1.  determine intersection of mesh faces with brush (is now in intersectGPU) || done on IntersectGPU
//  ||2.  determine which faces require memory or dont need memory anymore		 || done in MemoryManager
//  ||2.1  either scan
//  ||2.2  or append buffer 
//  ||3.  deallocate 
//  ||4.  allocate
//  5.  apply brush			
//  ||6. update overlap for analytic displacement maps || MOVED to TileOverlapUpdater


enum class EditPatchType : uint32_t
{
	REGULAR = 0,
	GREGORY = 1
};

union PaintDeformConfig{	// settings bitfield, check bit <32 to match int value for comparison
	struct {
		unsigned int patch_type				: 2;	// subd patch type: 0 regular or 1 gregory		
		unsigned int max_valence			: 6;	// max vertex valence in mesh
		unsigned int num_vertex_components	: 4;	// for vertex data stride
		unsigned int continuous				: 1;	// use continuous brush
		unsigned int voxel_multisampling	: 1;	// cast multiple rays
		unsigned int with_constraints		: 2;	// 0 disabled, 1 write to constraints buffer, 2 eval and write displacement
		unsigned int with_culling			: 1;	// enable 
		unsigned int displacement_tile_size : 4;	// log2 tile size	
		unsigned int update_max_disp		: 1;
	}; 

	int value;

	PaintDeformConfig()	: value(0) {}
	bool operator < (const PaintDeformConfig &e) const	// for triggering shader compile on demand
	{
		return value < e.value;
	}
};


class EffectRegistryPaintDeform: public TEffectDrawRegistry<PaintDeformConfig> 
{
protected:
	virtual ConfigType * _CreateDrawConfig(	DescType const & desc,	SourceConfigType const * sconfig,
		ID3D11Device1 * pd3dDevice,	ID3D11InputLayout ** ppInputLayout,
		D3D11_INPUT_ELEMENT_DESC const * pInputElementDescs, int numInputElements) const;

	virtual SourceConfigType *	_CreateDrawSourceConfig(DescType const & desc, ID3D11Device1 * pd3dDevice) const;
};


class TileEdit {
public:
	TileEdit();
	~TileEdit();
	
	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();
	
	HRESULT Apply(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance, IntersectGPU* intersect, uint32_t batchIdx, ModelInstance* penetratorVoxelization = NULL);
	HRESULT VoxelDeformOSD(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* deformable, ModelInstance* penetratorVoxelization, IntersectGPU* intersect, uint32_t batchIdx);
		
	
protected:
	void BindShaders(ID3D11DeviceContext1* pd3dImmediateContext, const PaintDeformConfig effect, ModelInstance* instance, IntersectGPU* intersect, ModelInstance* penetratorVoxelization, uint32_t batchIdx);
	void SetVoxelGridCB(ID3D11DeviceContext1* pd3dImmediateContext, Voxelizable* penetratorVoxelization, ModelInstance* deformableInstance) const;
	
	ID3D11Buffer*	m_intersectModelCB;
	ID3D11Buffer*	m_VoxelGridCB;	
	ID3D11Buffer*   m_osdCB; 


	ID3D11Buffer*		m_dispatchIndirectBUF;

	ID3D11SamplerState* m_samplerBilinear;
	ID3D11SamplerState* m_samplerNearest;

};
extern TileEdit					g_deformation;
extern EffectRegistryPaintDeform	g_paintDeformEffectRegistry;