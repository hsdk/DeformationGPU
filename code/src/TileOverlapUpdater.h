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

// fwd decls
class ModelInstance;


class TileOverlapUpdater{
public:
	TileOverlapUpdater();
	~TileOverlapUpdater();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();

	HRESULT UpdateOverlapDisplacement(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance);
	HRESULT UpdateOverlapColor(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance);

	HRESULT UpdateOverlapDisplacementConstraints(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance);
	void SetConstraintsOverlap(bool b) {m_constraintsMode = b;}
	HRESULT ClearIntersectAllBuffer( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance ) const;

private:
	HRESULT UpdateOverlapDisplacementInternal(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance) const;
	HRESULT UpdateOverlapColorInternal(ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance) const;
	HRESULT UpdateOverlapExtraordinaryCB(ID3D11DeviceContext1 *pd3dImmediateContext, UINT numExtraordinary) const;
	HRESULT CompactIntersectAllBuffer( ID3D11DeviceContext1 *pd3dImmediateContext, ModelInstance* instance ) const;

	
	Shader<ID3D11ComputeShader>		*m_updateOverlapEdgesDisplacementOSD_CS;
	Shader<ID3D11ComputeShader>		*m_updateOverlapCornersDisplacementOSD_CS;
	Shader<ID3D11ComputeShader>		*m_updateOverlapEqualizeCornerDisplacementOSD_CS;

	Shader<ID3D11ComputeShader>		*m_updateOverlapEdgesColorOSD_CS;
	Shader<ID3D11ComputeShader>		*m_updateOverlapCornersColorOSD_CS;
	Shader<ID3D11ComputeShader>		*m_updateOverlapEqualizeCornerColorOSD_CS;

	// compacted visibility variants	
	Shader<ID3D11ComputeShader>		*m_updateOverlapCompacted_EdgesDisplacementOSD_CS;
	Shader<ID3D11ComputeShader>		*m_updateOverlapCompacted_CornersDisplacementOSD_CS;

	Shader<ID3D11ComputeShader>		*m_updateOverlapCompacted_EdgesColorOSD_CS;
	Shader<ID3D11ComputeShader>		*m_updateOverlapCompacted_CornersColorOSD_CS;
	
	Shader<ID3D11ComputeShader>		*m_compactIntersectAll_CS;

	ID3D11Buffer					*m_updateExtraordinaryCB;
	ID3D11Buffer					*m_dispatchIndirectBUF; 

	bool	m_constraintsMode;
};

extern TileOverlapUpdater	g_overlapUpdater;

