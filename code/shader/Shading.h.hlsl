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

// spherical harmonic lighting
#ifndef M_PI
#define M_PI 3.14159f
#endif
#ifndef M_INV_PI
#define M_INV_PI 1.f/3.14159f
#endif
//static const float PI=3.14159265;

void SHDirection(in float3 v, out float sh[9])
{	
    float c0 = sqrt( 1.0) / ( 2.0 * sqrt(M_PI) );
    float c1 = sqrt( 3.0) / (2.0*sqrt(M_PI));
    float c2 = sqrt(15.0) / (2.0*sqrt(M_PI));
    float c3 = sqrt( 5.0) / (4.0*sqrt(M_PI));
    float c4 = sqrt(15.0) / (4.0*sqrt(M_PI));

    //sh[0] = c0;
    //sh[1] = c1*v.y;
    //sh[2] = c1*v.z;
    //sh[3] = c1*v.x;
    //sh[4] = c2*v.x*v.y;
    //sh[5] = c2*v.y*v.z;
    //sh[6] = c3*(3.0*v.z*v.z - 1.0);
    //sh[7] = c2*v.x*v.z;
    //sh[8] = c4*(v.x*v.x - v.y*v.y);
	//v = float3(v.x,-v.z,v.y);
	v= v.xzy;

	sh[0] = c0;
    sh[1] = c1*v.y;
    sh[2] = c1*v.z;
    sh[3] = c1*v.x;
    sh[4] = c2*v.x*v.y;
    sh[5] = c2*v.y*v.z;
    sh[6] = c3*(3.0*v.z*v.z - 1.0);
    sh[7] = c2*v.x*v.z;
    sh[8] = c4*(v.x*v.x - v.y*v.y);

}

void SHDirectionalLight(in float3 v, out float3 sh[9], in float3 col)
{
    float sh_dir[9];
    SHDirection(v, sh_dir);
    col *= M_PI;
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 9; i++)
            sh[i][c] = sh_dir[i]*col[c];
}

float3 SHEvaluate(in float sh_n[9], in float3 sh_f[9], in float3 sh_conv)
{
    float3 col;
    for (int c = 0; c < 3; c++)
    {
        // Band 0: constant
        col[c]  = sh_conv[0]*sh_f[0][c]*sh_n[0];

        // Band 1: linear
        col[c] += sh_conv[1]*sh_f[1][c]*sh_n[1];
        col[c] += sh_conv[1]*sh_f[2][c]*sh_n[2];
        col[c] += sh_conv[1]*sh_f[3][c]*sh_n[3];

        // Band 2: quadratic
        col[c] += sh_conv[2]*sh_f[4][c]*sh_n[4];
        col[c] += sh_conv[2]*sh_f[5][c]*sh_n[5];
        col[c] += sh_conv[2]*sh_f[6][c]*sh_n[6];
        col[c] += sh_conv[2]*sh_f[7][c]*sh_n[7];
        col[c] += sh_conv[2]*sh_f[8][c]*sh_n[8];
    }
    return col;
}


// SH Convolution Functions
///////////////////////////

float3 GeneralWrapSHOpt(float fA)
{
    float4 t0 = float4(-0.047771, -0.129310, 0.214438, 0.279310);
    float4 t1 = float4( 1.000000,  0.666667, 0.250000, 0.000000);

    float3 r;
    r.xyz = saturate(t0.xxy*fA + t0.yzw);
    r.xyz = -r*fA + t1.xyz;
    return r;
}

float3 GreenWrapSHOpt(float fW)
{
    float4 t0 = float4(0.0, 1.0/4.0, -1.0/3.0, -1.0/2.0);
    float4 t1 = float4(1.0, 2.0/3.0,  1.0/4.0,      0.0);

    float3 r;
    r.xyz = t0.xxy*fW + t0.xzw;
    r.xyz =  r.xyz*fW + t1.xyz;
    return r;
}

float3 SHConvolution(float wrap)
{   
	// select method
    //return GeneralWrapSHOpt(wrap);
	return GreenWrapSHOpt(wrap); 
}


// SH Lighting Functions
////////////////////////

float3 SHEvalEnvLight(in float3 n, in float wrap)
{
    float sh_n[9];
    SHDirection(n, sh_n);

    // From "An Efficient Representation for Irradiance Environment Maps"
    float3 sh_env[9];
	// st peters cathedral
    //sh_env[0] = float3( 0.79,  0.44,  0.54);
    //sh_env[1] = float3( 0.39,  0.35,  0.60);
    //sh_env[2] = float3(-0.34, -0.18, -0.27);
    //sh_env[3] = float3(-0.29, -0.06,  0.01);
    //sh_env[4] = float3(-0.11, -0.05, -0.12);
    //sh_env[5] = float3(-0.26, -0.22, -0.47);
    //sh_env[6] = float3(-0.16, -0.09, -0.15);
    //sh_env[7] = float3( 0.56,  0.21,  0.14);
    //sh_env[8] = float3( 0.21, -0.05, -0.30);
	
// snow
sh_env[0] = 2*float3(    0.125,     0.125,     0.209);
sh_env[1] = 2*float3(   -0.041,    -0.035,    -0.024);
sh_env[2] = 2*float3(    0.038,     0.034,     0.024);
sh_env[3] = 2*float3(    0.021,     0.034,     0.085);
sh_env[4] = 2*float3(   -0.042,    -0.037,    -0.028);
sh_env[5] = 2*float3(   -0.017,    -0.016,    -0.011);
sh_env[6] = 2*float3(    0.043,     0.041,     0.048);
sh_env[7] = 2*float3(    0.034,     0.032,     0.025);
sh_env[8] = 2*float3(   -0.046,    -0.044,    -0.063);

	//sh_env[0] = float3( 0.79,  0.74,  0.84);
 //   sh_env[1] = float3( 0.39,  0.35,  0.60);
 //   sh_env[2] = float3(-0.34, -0.18, -0.27);
 //   sh_env[3] = float3(-0.29, -0.06,  0.01);
 //   sh_env[4] = float3(-0.11, -0.05, -0.12);
 //   sh_env[5] = float3(-0.26, -0.22, -0.47);
 //   sh_env[6] = float3(-0.16, -0.09, -0.15);
 //   sh_env[7] = float3( 0.56,  0.21,  0.14);
 //   sh_env[8] = float3( 0.21, -0.05, -0.30);

    float3 sh_conv = SHConvolution(wrap);

    float3 col = SHEvaluate(sh_n, sh_env, sh_conv);
    col = max(col, float3(0, 0, 0));

    // Adjust exposure
    col *= 1.0;

    return col;
}

float3 SHEvalDirLight(in float3 n, in float3 dir, float wrap)
{
    float sh_n[9];
    SHDirection(n, sh_n);

    float3 sh_dir[9];
    SHDirectionalLight(dir, sh_dir, float3(1, 1, 1));

    float3 sh_conv = SHConvolution(wrap);

    float3 col = SHEvaluate(sh_n, sh_dir, sh_conv);
    col = max(col, float3(0, 0, 0));

    return float3(col);
}

float3 SHLight(in float3 n, in float3 lightDir, in float wrap)
{
    //if (env_sh)
       return SHEvalEnvLight(n, wrap);
    //else
    //   return SHEvalDirLight(n, lightDir, wrap);
}


// Regular Lighting
///////////////////

float Light(in float3 n, in float3 l, float wrap)
{
    float cosa = dot(n, l);
    float diff = max(cosa + wrap, 0.0)/(1.0 + wrap);

    //if (use_general)
    //{
   //     diff  = pow(diff, 1.0 + wrap);
   //     diff *= (wrap + 2.0)/(2.0*(wrap + 1.0));
    //}
    //else
        diff *= 1.0/(wrap + 1.0);

    return diff;
}



float3 shadingSH(float3 n, float3 L, float wrap)
{
	float3 color = SHLight(n, L, wrap);
	return color;

}





// ================================================================================================
// Calculates the Fresnel factor using Schlick's approximation
// ================================================================================================
float3 Fresnel(in float3 specAlbedo, in float3 h, in float3 l)
{
    float lDotH = saturate(dot(l, h));
    float3 fresnel = specAlbedo + (1.0f - specAlbedo) * pow((1.0f - lDotH), 5.0f);

    // Disable specular entirely if the albedo is set to 0.0
    fresnel *= dot(specAlbedo, 1.0f) > 0.0f;

    return fresnel;
}

float Beckmann_G1(float m, float nDotX)
{
    float nDotX2 = nDotX * nDotX;
    float tanTheta = sqrt((1 - nDotX2) / nDotX2);
    float a = 1.0f / (m * tanTheta);
    float a2 = a * a;

    float g = 1.0f;
    if(a < 1.6f)
        g *= (3.535f * a + 2.181f * a2) / (1.0f + 2.276f * a + 2.577f * a2);

    return g;
}

float Beckmann_Specular(in float m, in float3 n, in float3 h, in float3 v, in float3 l)
{
    float nDotH = max(dot(n, h), 0.0001f);
    float nDotL = saturate(dot(n, l));
    float nDotV = max(dot(n, v), 0.0001f);

    float nDotH2 = nDotH * nDotH;
    float nDotH4 = nDotH2 * nDotH2;
    float m2 = m * m;

    // Calculate the distribution term
    float tanTheta2 = (1 - nDotH2) / nDotH2;
    float expTerm = exp(-tanTheta2 / m2);
    float d = expTerm / (M_PI * m2 * nDotH4);

    // Calculate the matching geometric term
    float g1i = Beckmann_G1(m, nDotL);
    float g1o = Beckmann_G1(m, nDotV);
    float g = g1i * g1o;

    return d * g * (1.0f / (4.0f * nDotL * nDotV));
}

float GGX_V1(in float m2, in float nDotX)
{
    return 1.0f / (nDotX + sqrt(m2 + (1 - m2) * nDotX * nDotX));
}

float GGX_Specular(in float m, in float3 n, in float3 h, in float3 v, in float3 l)
{
    float nDotH = saturate(dot(n, h));
    float nDotL = saturate(dot(n, l));
    float nDotV = saturate(dot(n, v));

    float nDotH2 = nDotH * nDotH;
    float m2 = m * m;

    // Calculate the distribution term
    float d = m2 / (M_PI * pow(nDotH * nDotH * (m2 - 1) + 1, 2.0f));

    // Calculate the matching visibility term
    float v1i = GGX_V1(m2, nDotL);
    float v1o = GGX_V1(m2, nDotV);
    float vis = v1i * v1o;

    return d * vis;
}

// ================================================================================================
// Converts a Beckmann roughness parameter to a Phong specular power
// ================================================================================================
float RoughnessToSpecPower(in float m) {
    return 2.0f / (m * m) - 2.0f;
}

float SpecPowerToRoughness(in float s) {
    return sqrt(2.0f / (s + 2.0f));
}

float3 CalcLighting(in float3 normal, in float3 lightDir, in float3 lightColor,
					in float3 diffuseAlbedo, in float3 specularAlbedo, in float roughness,
					in float3 positionWS, in float3 camPos)
{
    float3 lighting = 0.0f;
    float nDotL = saturate(dot(normal, lightDir));
	float3 view = normalize(camPos - positionWS);
    if(nDotL > 0.0f)
    {
        //float3 view = normalize(camPos - positionWS);
        float3 h = normalize(view + lightDir);

        #if UseGGX_
            float specular = GGX_Specular(roughness, normal, h, view, lightDir);
        #else
            float specular = Beckmann_Specular(roughness, normal, h, view, lightDir);
        #endif

        float3 fresnel = Fresnel(specularAlbedo, h, lightDir);

        lighting = (diffuseAlbedo * M_INV_PI + specular * fresnel) * nDotL * lightColor	;
					
    }
	lighting +=   diffuseAlbedo * M_INV_PI * shadingSH(normal, lightDir, 0.5); //(1-Fresnel(diffuseAlbedo, h, lightDir)) 

    return max(lighting, 0.0f);
}