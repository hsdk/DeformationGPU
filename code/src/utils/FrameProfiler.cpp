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

#include "FrameProfiler.h"
#include "App.h"

#include "AntTweakBar.h"

FrameProfiler	g_frameProfiler;

FrameProfiler::FrameProfiler()
{
	m_qryTimestampDisjoint = NULL;
	m_qryStart.resize(static_cast<unsigned int>(DXPerformanceQuery::NUM_QUERYS), NULL);
	m_qryEnd.resize(static_cast<unsigned int>(DXPerformanceQuery::NUM_QUERYS), NULL);
	m_stageTimings.resize(static_cast<unsigned int>(DXPerformanceQuery::NUM_QUERYS), 0);
	m_stageActive.resize(static_cast<unsigned int>(DXPerformanceQuery::NUM_QUERYS), false);

	
}

FrameProfiler::~FrameProfiler()
{
	Destroy();
}

HRESULT FrameProfiler::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = NULL;
	D3D11_QUERY_DESC perfQueryDesc;

	perfQueryDesc.MiscFlags = 0;

	perfQueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	V_RETURN(pd3dDevice->CreateQuery(&perfQueryDesc, &m_qryTimestampDisjoint));

	perfQueryDesc.Query = D3D11_QUERY_TIMESTAMP;

	for(int i = 0; i< static_cast<unsigned int>(DXPerformanceQuery::NUM_QUERYS); ++i )
	{		
		V_RETURN(pd3dDevice->CreateQuery(&perfQueryDesc, &m_qryStart[i]));
		V_RETURN(pd3dDevice->CreateQuery(&perfQueryDesc, &m_qryEnd[i]));
	}



	return hr;
}

void FrameProfiler::InitGUI()
{
	TwBar* profileBar = TwNewBar("Stats");				
	TwDefine("Stats position='200 0' size='200 200' refresh=0.1");

	TwAddVarCB(profileBar, "Enable", TW_TYPE_BOOLCPP, 		(TwSetVarCallback)[](const void *value, void* clientData){g_app.g_profilePipelineStages = *static_cast<const bool *>(value);},
															(TwGetVarCallback)[](void *value, void*){*static_cast<bool*>(value) =g_app.g_profilePipelineStages;},NULL, "label = 'enable' " );
	
	TwAddVarRO(profileBar, "Render", TW_TYPE_FLOAT, 		&m_stageTimings[(UINT)DXPerformanceQuery::RENDER],		"" );
	TwAddVarRO(profileBar, "Shadows", TW_TYPE_FLOAT, 		&m_stageTimings[(UINT)DXPerformanceQuery::SHADOW],		"" );
	TwAddVarRO(profileBar, "SubD_kernel", TW_TYPE_FLOAT,	&m_stageTimings[(UINT)DXPerformanceQuery::SUBD_KERNEL], "" );
	TwAddVarRO(profileBar, "Deformation", TW_TYPE_FLOAT,	&m_stageTimings[(UINT)DXPerformanceQuery::DEFORMATION], "" );
	TwAddVarRO(profileBar, "PaintSculpt", TW_TYPE_FLOAT,	&m_stageTimings[(UINT)DXPerformanceQuery::PAINT_SCULPT], "" );
	TwAddVarRO(profileBar, "Overlap", TW_TYPE_FLOAT,		&m_stageTimings[(UINT)DXPerformanceQuery::OVERLAP], "" );
	TwAddVarRO(profileBar, "Postpro", TW_TYPE_FLOAT,		&m_stageTimings[(UINT)DXPerformanceQuery::POSTPRO], "" );
	TwAddVarRO(profileBar, "GUI", TW_TYPE_FLOAT, 			&m_stageTimings[(UINT)DXPerformanceQuery::GUI],			"" );

}



void FrameProfiler::Destroy()
{
	SAFE_RELEASE(m_qryTimestampDisjoint);
	for(int i = 0; i< static_cast<unsigned int>(DXPerformanceQuery::NUM_QUERYS); ++i )
	{
		SAFE_RELEASE(m_qryStart[i]);
		SAFE_RELEASE(m_qryEnd[i]);
	}
}

void FrameProfiler::FrameStart(ID3D11DeviceContext1* pd3dImmediateContext)
{
	if(!g_app.g_profilePipelineStages) return;
	pd3dImmediateContext->Begin(m_qryTimestampDisjoint);
}

void FrameProfiler::FrameEnd(ID3D11DeviceContext1* pd3dImmediateContext)
{
	if(!g_app.g_profilePipelineStages) return;

	pd3dImmediateContext->End(m_qryTimestampDisjoint);
	


	while (pd3dImmediateContext->GetData(m_qryTimestampDisjoint, NULL, 0, 0) == S_FALSE){	Sleep(1);    }   // Wait a bit, but give other threads a chance to run	

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestampDisjoint;
	pd3dImmediateContext->GetData(m_qryTimestampDisjoint, &timestampDisjoint, sizeof(timestampDisjoint), 0);

	if(timestampDisjoint.Disjoint)
		return;

	// eval perf timers
	for(int i = 0; i< static_cast<unsigned int>(DXPerformanceQuery::NUM_QUERYS); ++i )
	{
		if(!m_stageActive[i])
		{
			m_stageTimings[i] = -1;
			continue;
		}

		UINT64 timestampStart;
		UINT64 timestampEnd;

		while(pd3dImmediateContext->GetData(m_qryStart[i], &timestampStart, sizeof(timestampStart), 0)!= S_OK);
		while(pd3dImmediateContext->GetData(m_qryEnd[i],   &timestampEnd,   sizeof(timestampEnd),   0) != S_OK);

		//pd3dImmediateContext->GetData(m_qryStart[i], &timestampStart, sizeof(timestampStart),0);
		//pd3dImmediateContext->GetData(m_qryEnd[i],   &timestampEnd,   sizeof(timestampEnd),0);



		m_stageTimings[i] = static_cast<float>(1000 * double(timestampEnd - timestampStart) / double(timestampDisjoint.Frequency));
		m_stageActive[i] = false;
	}
	//std::cout << "Deformation " << m_stageTimings[(UINT)DXPerformanceQuery::DEFORMATION] << std::endl;
	//std::cout << "Shadows " << m_stageTimings[(UINT)DXPerformanceQuery::SHADOW] << std::endl;
	//std::cout << "Render Shaded " << m_stageTimings[(UINT)DXPerformanceQuery::RENDER] << std::endl;	
	//std::cout << "SubD Kernel " << m_stageTimings[(UINT)DXPerformanceQuery::SUBD_KERNEL] << std::endl;
	//std::cout << "freq: " << timestampDisjoint.Frequency << std::endl;
	//if(g_qryTimeActive)	std::cerr << "perf query is active in GPUPerfTimerElapsed, check your code for nested timings" << std::endl;
	//return (float)elapsed*1000.0;	
}

void FrameProfiler::BeginQuery(ID3D11DeviceContext1* pd3dImmediateContext, DXPerformanceQuery query )
{
	if(!g_app.g_profilePipelineStages) return;	
	m_stageActive[static_cast<unsigned int>(query)] = true;
	pd3dImmediateContext->End(m_qryStart[static_cast<unsigned int>(query)]);
	
	
}

void FrameProfiler::EndQuery(ID3D11DeviceContext1* pd3dImmediateContext, DXPerformanceQuery query )
{	
	if(!g_app.g_profilePipelineStages) return;	
	pd3dImmediateContext->End(m_qryEnd[static_cast<unsigned int>(query)]);	
}
