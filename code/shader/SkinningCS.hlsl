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

#define SKINNING_NUM_MAX_BONES 80

cbuffer cbSkinning : register( b12 ) {
	uint g_mNumOriginalVertices;
	uint g_mBaseVertex;
	uint2 padding;
	float4x4 g_mBoneMatrices[SKINNING_NUM_MAX_BONES];
}

Buffer<float> originalVerticesSRV	: register ( t0 );
Buffer<float> originalNormalsSRV	: register ( t1 );
Buffer<uint4> boneIDs				: register ( t2 );
Buffer<float4> boneWeights			: register ( t3 );

RWBuffer<float> skinnedVerticesUAV : register( u0 );
RWBuffer<float> skinnedNormalsUAV : register( u1 );

float4x4 inverse(float4x4 m)
{
	float4x4 mT = transpose(m);
	float3 T = float3(mT[3].xyz);	
	mT[3] = float4(0,0,0,1);

	float3x3 R = (float3x3)mT;
	
	float3 iT = float3( -dot(R[0],T),
						-dot(R[1],T),
						-dot(R[2],T));

	mT[0].w = iT.x; //-dot(R[0],T);
	mT[1].w = iT.y; //-dot(R[1],T);
	mT[2].w = iT.z; //-dot(R[2],T);
	return mT;	
}

[numthreads(128, 1, 1)]
void SkinningCS(	uint3 blockIdx : SV_GroupID, 
					uint3 DTid : SV_DispatchThreadID, 
					uint3 threadIdx : SV_GroupThreadID,
					uint GI : SV_GroupIndex ) {
	uint i = DTid.x;
	//skinnedVerticesUAV.Store3(i*12, asuint(float3(0,0,0)));

	if (i > g_mNumOriginalVertices) return;

	//uint4 IDs = boneIDs[i+g_mBaseVertex];
	//float4 weights = boneWeights[i+g_mBaseVertex];

	uint4 IDs = boneIDs[i];
	float4 weights = boneWeights[i];

	float4 originalVertex = {originalVerticesSRV[i*4+0], originalVerticesSRV[i*4+1], originalVerticesSRV[i*4+2], 1.0};
	float4 originalNormal = {originalNormalsSRV[i*4+0], originalNormalsSRV[i*4+1], originalNormalsSRV[i*4+2], 0.0};

	
	float4x4 bT =
			g_mBoneMatrices[IDs[0]] * weights[0] +
			g_mBoneMatrices[IDs[1]] * weights[1] +
			g_mBoneMatrices[IDs[2]] * weights[2] +
			g_mBoneMatrices[IDs[3]] * weights[3];
			
	//float4x4 bT2 =
	//		transpose(g_mBoneMatrices[IDs[0]]) * weights[0] +
	//		transpose(g_mBoneMatrices[IDs[1]]) * weights[1] +
	//		transpose(g_mBoneMatrices[IDs[2]]) * weights[2] +
	//		transpose(g_mBoneMatrices[IDs[3]]) * weights[3];
			
	originalVertex = mul(originalVertex, bT); // tranpose (!)
	//originalNormal = normalize(mul(transpose(bT), float4(normalize(originalNormal.xyz),0)));
	originalNormal.xyz = normalize(mul(normalize(originalNormal.xyz), (float3x3)bT));

	//const float scale = 15.0f;
	//const float3 offset = float3(25,10,1.1f);
	const float scale = 15.0f;
	const float3 offset = float3(0,0.0,-0.75f);

	skinnedVerticesUAV[i*4+0] = originalVertex.x *scale + offset.x;//* scale- 30.;
	skinnedVerticesUAV[i*4+1] = originalVertex.y *scale + offset.y;//* scale;
	skinnedVerticesUAV[i*4+2] = originalVertex.z  *scale + offset.z;//* scale + offset;



	skinnedNormalsUAV[i*4+0] = originalNormal.x;
	skinnedNormalsUAV[i*4+1] = originalNormal.y;
	skinnedNormalsUAV[i*4+2] = originalNormal.z;

}

