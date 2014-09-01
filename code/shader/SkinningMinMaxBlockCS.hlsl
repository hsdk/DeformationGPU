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

cbuffer cbOBB : register( b12 ) {
	uint N;
}

Buffer<float4> verticesSRV	: register ( t0 );
Buffer<float4> ONB			: register ( t1 );

RWBuffer<float4> outMin		: register ( u0 );
RWBuffer<float4> outMax		: register ( u1 );

groupshared float4 tmp[1024];

float4 scanInclusiveIntraWarpMin(uint lane, uint i) {
	uint4 X = i - uint4(1,2,4,8);
	if (lane >= 0x01) tmp[i] = min(tmp[i], tmp[X.x]);
	if (lane >= 0x02) tmp[i] = min(tmp[i], tmp[X.y]);
	if (lane >= 0x04) tmp[i] = min(tmp[i], tmp[X.z]);
	if (lane >= 0x08) tmp[i] = min(tmp[i], tmp[X.w]);
	if (lane >= 0x10) tmp[i] = min(tmp[i], tmp[i-16]);

	return tmp[i];
}

float4 scanInclusiveIntraWarpMax(uint lane, uint i) {
	uint4 X = i - uint4(1,2,4,8);
	if (lane >= 0x01) tmp[i] = max(tmp[i], tmp[X.x]);
	if (lane >= 0x02) tmp[i] = max(tmp[i], tmp[X.y]);
	if (lane >= 0x04) tmp[i] = max(tmp[i], tmp[X.z]);
	if (lane >= 0x08) tmp[i] = max(tmp[i], tmp[X.w]);
	if (lane >= 0x10) tmp[i] = max(tmp[i], tmp[i-16]);

	return tmp[i];
}

[numthreads(1024, 1, 1)]
void SkinningCS(	uint3 blockIdx : SV_GroupID, 
					uint3 DTid : SV_DispatchThreadID, 
					uint3 threadIdx : SV_GroupThreadID,
					uint GI : SV_GroupIndex ) {
	uint i = DTid.x;
	
	float4 v0 = ONB[0];
	float4 v1 = ONB[1];
	float4 v2 = ONB[2];
	float3x3 O = float3x3(v0.xyz, v1.xyz, v2.xyz);
	float4 X = float4(mul(O, verticesSRV[i].xyz), 1.0);
	
	// Min
	tmp[GI]  = i < N ? X : 1000000.0f;

	uint lane = GI & 31u;
	uint warp = GI >> 5u;
    
	float4 x = scanInclusiveIntraWarpMin(lane, GI);
	GroupMemoryBarrierWithGroupSync();
    
	if (lane == 31) tmp[warp] = x;
	GroupMemoryBarrierWithGroupSync();
    
	if (warp == 0) scanInclusiveIntraWarpMin(lane, lane);
	GroupMemoryBarrierWithGroupSync();
    
	if (warp > 0)
		x = min(x, tmp[warp-1]);
	GroupMemoryBarrierWithGroupSync();
	
	outMin[i] = x;//dot(x,x) == 0.0f ? 1000.0f : x;
	
	// Max
	tmp[GI]  = i < N ? X : -1000000.0f;

	x = scanInclusiveIntraWarpMax(lane, GI);
	GroupMemoryBarrierWithGroupSync();

	if (lane == 31) tmp[warp] = x;
	GroupMemoryBarrierWithGroupSync();

	if (warp == 0) scanInclusiveIntraWarpMax(lane, lane);
	GroupMemoryBarrierWithGroupSync();

	if (warp > 0)
		x = max(x, tmp[warp-1]);
	GroupMemoryBarrierWithGroupSync();	

	outMax[i] = x;
}

