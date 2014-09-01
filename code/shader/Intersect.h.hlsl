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

#define INTERSECT_TRUE  1
#define INTERSECT_FALSE 0

#ifndef FLT_MAX
#define FLT_MAX 3.999e+10F
#endif

#define MINF asfloat(0xff800000)

#ifndef M_PI
#define M_PI 3.14159265358f
#endif

// constant buffers
#ifndef DEFORMATION_BATCH_SIZE
#define DEFORMATION_BATCH_SIZE 6u
#endif

cbuffer IntersectCB : register( b11 )
{	
	float4x4		g_matModelToOBB[DEFORMATION_BATCH_SIZE];			// for transforming from model to brush space
	float			g_displacementScaler;			// for box enlargement
}







