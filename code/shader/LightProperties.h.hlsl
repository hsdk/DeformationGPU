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

#ifndef __LIGHT_PROPERTIES_INCLUDED__
#define __LIGHT_PROPERTIES_INCLUDED__

#ifndef NUM_CASCADES
#define NUM_CASCADES 3
#endif


cbuffer cbLight : register( b7 )
{
	float4 g_lightDirWS;
	float4 g_lightDirES;
	matrix g_lightViewMatrix;				// render ES pos to shadow ES pos
	float4 g_cascadeOffset[NUM_CASCADES];
	float4 g_cascadeScale[NUM_CASCADES];

	int    g_nCascadeLevels;
	int    g_visualizeCascades;
	float  g_minBorderPadding;
	float  g_maxBorderPadding;

	float  g_cascadeBlendArea;
	float  g_texelSize;
	float  g_nativeTexelSize;
	float  g_lightCBpadding1;
	float4 g_cascadeSplitDepthsES;
	
};

cbuffer cbCascadeProj : register( b10 )
{
	matrix g_mCascadeProj[NUM_CASCADES];
};

#endif

