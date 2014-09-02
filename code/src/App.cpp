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
#include "App.h"
#include <SDX/DXBuffer.h>
//Henry: has to be last header
#include "utils/DbgNew.h"

HRESULT App::Create( ID3D11Device1* pd3dDevice )
{
	HRESULT hr = S_OK;
	// Setup constant buffers
	
	V_RETURN(DXCreateBuffer(pd3dDevice,D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_PER_FRAME), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, g_CBCamera));
	DXUT_SetDebugName(g_CBCamera, "g_pCamCB");
		

	V_RETURN(DXCreateBuffer(pd3dDevice,D3D11_BIND_CONSTANT_BUFFER, sizeof(CB_GEN_SHADOW), D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC, g_CBGenShadow));
	DXUT_SetDebugName(g_CBGenShadow, "g_CBGenShadow");

	// create performance query
	D3D11_QUERY_DESC queryDesc;
	queryDesc.Query = D3D11_QUERY_EVENT;
	queryDesc.MiscFlags = 0; 
	V_RETURN(pd3dDevice->CreateQuery(&queryDesc, &g_query)); 


	D3D11_QUERY_DESC perfQueryDesc;
	perfQueryDesc.Query = D3D11_QUERY_TIMESTAMP;
	perfQueryDesc.MiscFlags = 0;
	

	pd3dDevice->CreateQuery(&perfQueryDesc, &g_qryTimestampStart);
	pd3dDevice->CreateQuery(&perfQueryDesc, &g_qryTimestampEnd);
	
	perfQueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	pd3dDevice->CreateQuery(&perfQueryDesc, &g_qryTimestampDisjoint);


	queryDesc.Query = D3D11_QUERY_PIPELINE_STATISTICS;
	queryDesc.MiscFlags = 0; 
	V_RETURN(pd3dDevice->CreateQuery(&queryDesc, &g_pipelineQuery)); 

	return hr;	
}

void App::Destroy()
{	
	SAFE_RELEASE( g_CBCamera );
	SAFE_RELEASE( g_CBGenShadow );
	SAFE_RELEASE( g_query );
	SAFE_RELEASE( g_qryTimestampStart );
	SAFE_RELEASE( g_qryTimestampEnd );
	SAFE_RELEASE( g_qryTimestampDisjoint );
	SAFE_RELEASE( g_pipelineQuery );

	SAFE_DELETE(  g_osdSubdivider );	
}
