//   Copyright 2013 Henry Sch�fer
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

cbuffer cbMat : register(b2)
{
	float3	g_MatAmbient;		// constant ambient color
	float	g_Special;
	float3	g_MatDiffuse;		// constant diffuse color
	float	g_Smoothness;
	float3	g_MatSpecular;		// constant specular color
	float	g_MatShininess;		// shininess
}