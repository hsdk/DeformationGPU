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

// constant buffers
cbuffer cbFrame : register( b0 )
{
	float4x4    g_modelViewProjection;  
    float4x4    g_modelViewMatrix;  
	float4x4    g_projectionMatrix;  
}

cbuffer cbAABB : register( b1 )
{
	float4x4    g_modelAABB;  
}


struct VS_INPUT
{
    float3 f3Position   : POSITION; 	
};

struct VS_INPUT_FRUSTUM
{
    float3 f3Position   : POSITION;
	uint   vertexID		: SV_VertexID;
};

struct PS_INPUT
{
    float4 f4Position   : SV_Position;  
};

struct PS_INPUT_FRUSTUM
{
    float4 f4Position   : SV_Position; 
	float4 color		: COLOR0;
};

struct PS_OUTPUT
{
	float4 f4Color		: SV_Target0;
};

PS_INPUT VS_CAGE(VS_INPUT I)
{
	PS_INPUT O;		
	O.f4Position = mul( g_modelViewProjection, float4( I.f3Position.xyz, 1.0 ) );
	return O;
}

PS_INPUT VS_AABB(VS_INPUT I)
{
	PS_INPUT O;		
	O.f4Position = mul( g_modelViewProjection, mul(g_modelAABB, float4( I.f3Position.xyz, 1.0 )) );
	return O;
}

PS_OUTPUT PS_AABB(PS_INPUT I)
{
	PS_OUTPUT O;

	O.f4Color = float4(1,0,0,1);
	return O;
}

PS_INPUT_FRUSTUM VS_FRUSTUM(VS_INPUT_FRUSTUM I)
{
	PS_INPUT_FRUSTUM O;		
	O.f4Position = mul( g_modelViewProjection, mul(g_modelAABB, float4( I.f3Position.xyz, 1.0 )) );
	if(I.vertexID < 4)
		O.color = float4(0,1,0,1);
	else
		O.color =float4(1,0,0,1);
	return O;
}

PS_OUTPUT PS_FRUSTUM(PS_INPUT_FRUSTUM I)
{
	PS_OUTPUT O;

	O.f4Color = I.color;
	return O;
}
