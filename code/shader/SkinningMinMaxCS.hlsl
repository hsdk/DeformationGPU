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

cbuffer cbSkinning : register( b12 ) {
	uint N;
}

Buffer<float4> ONB : register ( t0 );
Buffer<float4> inMin : register ( t1 );
Buffer<float4> inMax : register ( t2 );

RWBuffer<float4> anchorAndAxes : register ( u0 );

groupshared float4 tmp[1024];

float4 getIncrValueMin(int i) {
	float4 x = inMin[i];
	//if (i > 1024 && N >= 1024)
		x = min(x, tmp[i/1024] == 0.0f ? x : tmp[i/1024]);
	//	//if (i > 1024*1024) 
	//	return 0.0;
	return x;
}

float4 getIncrValueMax(int i) {
	float4 x = inMax[i];
	//if (N > 1024)
		x = max(x, tmp[i/1024] == 0.0f ? x : tmp[i/1024]);
	return x;
}

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
	int v =  int(GI*1024)-1;
	uint lane = GI & 31u;
	uint warp = GI >> 5u;

	// MIN
	if (dot(inMin[i], inMin[GI]) == 0.0f) tmp[GI] =  -1000.0f;
	else
	tmp[i]  = v < int(N) && N > 1024 ? inMin[v] : 100000.0;
	//tmp[GI] = 0.0;

	GroupMemoryBarrierWithGroupSync();
	float4 x = scanInclusiveIntraWarpMin(lane, GI);
	GroupMemoryBarrierWithGroupSync();

	if (lane == 31) tmp[warp] = x;
	GroupMemoryBarrierWithGroupSync();

	if (warp == 0) scanInclusiveIntraWarpMin(lane, lane);
	GroupMemoryBarrierWithGroupSync();

	if (warp > 0)
		x = min(tmp[warp-1], x);
	GroupMemoryBarrierWithGroupSync();

	float4 MIN = 0.0f;
	if (GI == 0) {
		MIN = getIncrValueMin(int(N)-1);
	}
	GroupMemoryBarrierWithGroupSync();


	// MAX
	tmp[GI] = v < int(N) && N > 1024 ? inMax[v] : -100000.0;
	GroupMemoryBarrierWithGroupSync();
	x = scanInclusiveIntraWarpMax(lane, GI);
	GroupMemoryBarrierWithGroupSync();

	if (lane == 31) tmp[warp] = x;
	GroupMemoryBarrierWithGroupSync();

	if (warp == 0) scanInclusiveIntraWarpMax(lane, lane);
	GroupMemoryBarrierWithGroupSync();

	if (warp > 0)
		x = max(x, tmp[warp-1]);
	GroupMemoryBarrierWithGroupSync();

	float4 MAX = 0.0f;
	if (GI == 0) {
		MAX = getIncrValueMax(int(N)-1);

		float4 v0 = ONB[0];
		float4 v1 = ONB[1];
		float4 v2 = ONB[2];
		float3x3 O = float3x3(v0.xyz, v1.xyz, v2.xyz);
		//O = transpose(O);
	
		float4 extend = MAX - MIN;
		v0.xyz *= extend.x;
		v1.xyz *= extend.y;
		v2.xyz *= extend.z;

		float4 anchor = float4(mul(transpose(O), MIN.xyz), 1.0f); 
		anchorAndAxes[0] = anchor;
		anchorAndAxes[1] = v0.xyzw;
		anchorAndAxes[2] = v1.xyzw;
		anchorAndAxes[3] = v2.xyzw;
		//anchorAndAxes[4] = MIN;
		//anchorAndAxes[5] = MAX;
	}
}

