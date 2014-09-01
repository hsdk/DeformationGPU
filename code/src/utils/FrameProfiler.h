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
#include <map>
#include <set>
#include <vector>


enum class DXPerformanceQuery
{	
	RENDER			= 0,	
	SUBD_KERNEL		= 1,
	DEFORMATION		= 2,
	PAINT_SCULPT	= 3,
	SHADOW			= 4,	
	GUI				= 5,
	POSTPRO			= 6,
	OVERLAP			= 7,
	NUM_QUERYS
};


class FrameProfiler{
public:
	FrameProfiler();
	~FrameProfiler();

	HRESULT Create(ID3D11Device1* pd3dDevice);
	void Destroy();

	void FrameStart(ID3D11DeviceContext1* pd3dImmediateContext);
	void FrameEnd(ID3D11DeviceContext1* pd3dImmediateContext);

	void BeginQuery(ID3D11DeviceContext1* pd3dImmediateContext, DXPerformanceQuery query);
	void EndQuery(ID3D11DeviceContext1* pd3dImmediateContext, DXPerformanceQuery query);
	
	const std::vector<float>& GetStageTimings() const;
	void InitGUI();
protected:
	std::vector<ID3D11Query*> m_qryStart;
	std::vector<ID3D11Query*> m_qryEnd;

	std::vector<float>		  m_stageTimings;
	std::vector<bool>		  m_stageActive;

	ID3D11Query* m_qryTimestampDisjoint;

	//DISJOINT = 0,
};

extern FrameProfiler	g_frameProfiler;