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

cbuffer cbCamAndDist : register(b4)
{
	float3 g_eye;						
	float g_depthRange;
}

cbuffer cbPostprocessing : register(b2)
{
	float g_bokehFocus;
	float d0, d1, d2;
};


#define WIDTH 1280
#define HEIGHT 720

#define vec2 float2
#define vec3 float3
#define vec4 float4
#define ivec2 int2
#define ivec3 int3
#define ivec4 int4

#define mix lerp

Texture2D g_tex0 : register(t0);
Texture2D g_tex1 : register(t1);
Texture2D g_tex2 : register(t2);
Texture2D g_tex3 : register(t3);
Texture2D g_tex4 : register(t4);

SamplerState g_samplerRepeat : register( s0 );
SamplerState g_samplerClamp : register( s1 );
SamplerState g_sampler : register( s2 );


struct VSInput {
    float4 position : POSITION;  	
	float2 tc : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
	float2 tc : TEXCOORD0;
};

struct PSInput {
	float4 position : SV_Position;
};

struct PSOutput {
    float4 o0 : SV_Target0;
};

struct PSOutput2 {
    float4 o0 : SV_Target0;
    float4 o1 : SV_Target1;
};

struct PSOutput3 {
    float4 o0 : SV_Target0;
    float4 o1 : SV_Target1;
	float4 o2 : SV_Target2;
};

static const float blurRadius = 12.0f;

// BLUR PASSES

PSOutput blur4HPS(PSInput I) {
	vec2 pixelSize = 4.0f/vec2(WIDTH, HEIGHT);
	vec2 tc = I.position.xy * pixelSize;
	vec2 blurDirection = vec2(1,0);
	float sigma = blurRadius / 3.0f;
	vec4 sum = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	for (float i = -blurRadius; i <= blurRadius; ++i) {
		float weight = exp(-(i*i)/(2*sigma*sigma));
		sum += weight * g_tex0.Sample(g_sampler, tc + blurDirection*i*pixelSize) * weight;
	}
	PSOutput O;
	O.o0 = sum / (sigma*sqrt(2*acos(-1)));
	return O;
}

PSOutput blur4VPS(PSInput I) {
	vec2 pixelSize = 4.0f/vec2(WIDTH, HEIGHT);
	vec2 tc = I.position.xy * pixelSize;
	vec2 blurDirection = vec2(0,1);
	float sigma = blurRadius / 3.0f;
	vec4 sum = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	for (float i = -blurRadius; i <= blurRadius; ++i) {
		float weight = exp(-(i*i)/(2*sigma*sigma));
		sum += weight * g_tex0.Sample(g_sampler, tc + blurDirection*i*pixelSize) * weight;
	}
	PSOutput O;
	O.o0 = sum / (sigma*sqrt(2*acos(-1)));
	return O;
}

PSOutput blur16HPS(PSInput I) {
	vec2 pixelSize = 16.0f/vec2(WIDTH, HEIGHT);
	vec2 tc = I.position.xy * pixelSize;
	vec2 blurDirection = vec2(1,0);
	float sigma = blurRadius / 3.0f;
	vec4 sum = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	for (float i = -blurRadius; i <= blurRadius; ++i) {
		float weight = exp(-(i*i)/(2*sigma*sigma));
		sum += weight * g_tex0.Sample(g_sampler, tc + blurDirection*i*pixelSize) * weight;
	}
	PSOutput O;
	O.o0 = sum / (sigma*sqrt(2*acos(-1)));
	return O;
}

PSOutput blur16VPS(PSInput I) {
	vec2 pixelSize = 16.0f/vec2(WIDTH, HEIGHT);
	vec2 tc = I.position.xy * pixelSize;
	vec2 blurDirection = vec2(0,1);
	float sigma = blurRadius / 3.0f;
	vec4 sum = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	for (float i = -blurRadius; i <= blurRadius; ++i) {
		float weight = exp(-(i*i)/(2*sigma*sigma));
		sum += weight * g_tex0.Sample(g_sampler, tc + blurDirection*i*pixelSize) * weight;
	}
	PSOutput O;
	O.o0 = sum / (sigma*sqrt(2*acos(-1)));
	return O;
}

PSOutput blur32HPS(PSInput I) {
	vec2 pixelSize = 16.0f/vec2(WIDTH, HEIGHT);
	vec2 tc = I.position.xy * pixelSize;
	vec2 blurDirection = vec2(1,0);
	float sigma = blurRadius / 3.0f;
	vec4 sum = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	for (float i = -blurRadius; i <= blurRadius; ++i) {
		float weight = exp(-(i*i)/(2*sigma*sigma));
		sum += weight * g_tex0.Sample(g_sampler, tc + blurDirection*i*pixelSize) * weight;
	}
	PSOutput O;
	O.o0 = sum / (sigma*sqrt(2*acos(-1)));
	return O;
}

PSOutput blur32VPS(PSInput I) {
	vec2 pixelSize = 16.0f/vec2(WIDTH, HEIGHT);
	vec2 tc = I.position.xy * pixelSize;
	vec2 blurDirection = vec2(0,1);
	float sigma = blurRadius / 3.0f;
	vec4 sum = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	for (float i = -blurRadius; i <= blurRadius; ++i) {
		float weight = exp(-(i*i)/(2*sigma*sigma));
		sum += weight * g_tex0.Sample(g_sampler, tc + blurDirection*i*pixelSize) * weight;
	}
	PSOutput O;
	O.o0 = sum / (sigma*sqrt(2*acos(-1)));
	return O;
}

// BOKEH PASSES

static const float bokeh_aperture = 3.0;//6.0;//3.0//6.0;
static const float bokeh_focus = g_bokehFocus;// 45.0f;//g_bokehFocus;//120.0f;//g_bokehFocus;//45.0;//g_bokehFocus;//60.1415;
static const float bokeh_radius = 1.;//.98;
static const vec2 bokehPixelSize = 1/vec2(1280,720);

#define N 10.

float calc_coc1(float depth) {
   float c=abs((depth-bokeh_focus) / bokeh_focus * bokeh_aperture)*bokehPixelSize.y*720;
   return clamp(c, 1., N);
}

vec4 sample_bokeh1( vec2 d1, vec2 sc ){
	vec4 c1=vec4(0,0,0,0);
	float c1n = 0;
	for(float i=.5; i<N; i++) {
		// Read depth of source pixel
		vec2 pc = sc+d1*bokeh_radius*i*bokehPixelSize;
		float pdepth = g_tex1.Sample(g_sampler, pc).a * g_depthRange;
		float coc= calc_coc1(pdepth);
		vec4 pval = g_tex0.Sample(g_sampler, pc);
		float contrib = step(i,coc)/(coc*coc);
		if (i+1 > coc)
			contrib *=2;
		pval.a = coc;
		pval.xyz *= coc;
		c1 += contrib * pval;
		c1n += contrib;
	}
	//return g_tex0.Sample(g_sampler, sc);//vec4(,1,1,1);
	return c1 /= c1n;
}

PSOutput2 bokeh1PS(PSInput I) {
	vec2 sc = I.position.xy * bokehPixelSize;
	
	vec4 c1 = sample_bokeh1(vec2(.8660254, -.5), sc);
	vec4 c2 = sample_bokeh1(vec2(-.8660254, -.5), sc);
	
	PSOutput2 O;
	
	O.o1 = c1;
	c2+=c1;
	c2/=2;
	O.o0 = c2;
	//O.o0.a = g_tex1.Sample(g_sampler, sc).a* g_depthRange;
	//O.o1.a = O.o0.a;
	return O;
	//out0.xy = sc.xy;
	//out0 = vec4(1); out1 = vec4(1);
}

vec4 sample_bokeh( vec2 d1, vec2 sc, Texture2D tex ){
	vec4 c1=vec4(0,0,0,0);
	// Read depth of source pixel
	for(float i=.5; i<N; i++) {
		vec2 pc = sc+d1*bokeh_radius*i*bokehPixelSize;
		vec4 pval = tex.Sample(g_sampler, pc);
		float coc = pval.a;
		float contrib = step(i,coc)/(coc*coc);
		if (i+1 > coc)
			contrib *=2;
		
		c1 += contrib * pval;
	}
	return c1;
}

PSOutput bokeh2PS(PSInput I) {
	vec2 sc = I.position.xy * bokehPixelSize;
	vec4 c1 = sample_bokeh( vec2(-.8660254,-.5), sc, g_tex1 );
	vec4 c2 = sample_bokeh( vec2(0,1), sc, g_tex0 );
	c1/=2;
	c1+=c2;
	c1/=c1.a;	
	
	PSOutput O;
	O.o0 = c1;	
	O.o0.a = g_tex1.Sample(g_sampler, sc).a;
	O.o0.a = 1.0;
	//O.o0 =  0.0;//g_tex0.Sample(g_sampler, sc).a;
	//O.o0 = g_tex0.Sample(g_sampler, sc).a;
	return O;
	
}

// STREAKS
static const vec2 streakPixelSize = 1/vec2(1280,720);
static const float streakBlurRadius = 160;

PSOutput streaksPS(PSInput I) {
	vec2 texcoord = I.position.xy * streakPixelSize;
	
	vec4 sum = vec4(0,0,0,0);
	float sigma = blurRadius/3;
	vec2 dir = vec2(1,0);  

	for (float i = -streakBlurRadius; i <= streakBlurRadius; i++) {
		vec2 pos = texcoord+dir*i*streakPixelSize;
		float weight = (-streakBlurRadius+abs(i))/(streakBlurRadius*streakBlurRadius);
		sum += weight*g_tex0.Sample(g_sampler, pos) * weight;
	}
	PSOutput O;
	//O.o0 = g_tex0.Sample(g_sampler, texcoord);
	O.o0 = sum;
	return O;
}

// MERGE

#define streak g_tex0
#define bokeh g_tex1
#define blurredSmall g_tex2
#define blurredMedium g_tex3
#define blurredBig g_tex4

static const vec2 mergePixelSize = 1/vec2(1280, 720);

static const vec3 cg_lift = vec3(0.75, 0.75, 0.75);
static const vec3 cg_gamma = vec3(0.25, 0.25, 0.25);
static const vec3 cg_gain = vec3(1.0, 1.0, 1.0);
static const float cg_saturation = 1.05;
static const vec3 maxwhite = vec3(5, 5, 5);
static const float maxwhite_multiplier = 5;
static const float vignette_offset = 1.;
static const float vignette_exponent = 2.5;

static const float streak_strength = 4.5;
static const float noise_strength = 0.1;

static const float scenecolor_multiplier = 0.4;

vec3 colorcorrect(vec3 input_col) {
	vec3 lift = 2.*(cg_lift - .5)*0.2;
	vec3 gain = 2.*cg_gain;
	vec3 gammah = 2.*cg_gamma;
	vec3 sop_out = pow(clamp(
				 (gain*input_col+lift*(1.-gain*input_col)),
				 0.,1.),1./gammah);
	sop_out = clamp(sop_out, 0., 1.);

	float luma = 0.2126 * sop_out.r + 0.7152 * sop_out.g + 0.0722 * sop_out.b;
	return luma + cg_saturation * (sop_out - luma);
}

float pn(vec3 p) {
	p *= 1.5;
	vec3 i = floor(p);
	vec4 a = dot(i, vec3(1,57,21)) + vec4(0,57,21,78);
	vec3 f = cos((p-i)*acos(-1.))*(-.5) + .5;
	a = mix(sin(cos(a)*a), sin(cos(1+a)*(1+a)), f.x);
	a.xy = mix(a.xz, a.yw, f.y);
	return mix(a.x, a.y, f.z);
}

float fpn(vec3 p) {
	float r;
	p.xz = cos(1.) * p.xz + vec2(-sin(1.), sin(1.)) * p.zx;
	r = pn(p*.06125)*.5;
	p.xy = cos(1.) * p.xy + vec2(-sin(1.), sin(1.)) * p.yx;
	r += pn(p*.125)*.25;
	p.zy = cos(1.) * p.zy + vec2(-sin(1.), sin(1.)) * p.yz;
	r += pn(p*.25)*.125;
	return r;
}


int LFSR_Rand_Gen(in int n) {
	n = (n << 13) ^ n;
	return (n * (n*n*15731+789221) + 1376312589) & 0x7fffffff;
}

float noise3f(in vec3 p) {
	ivec3 ip = ivec3(floor(p));
	int n = ip.x + ip.y*57 + ip.z*113;
	return float(LFSR_Rand_Gen(n))/1073741824. - .5;
}

float vignette(vec2 sc, float offset, float exponent) {
	sc -= .5;
	sc *= 2.;
	sc *= offset;
	sc = abs(sc);
	return max(0,1.-pow(sc.x, exponent)-pow(sc.y,exponent));
}

PSOutput mergePS(PSInput I) {
	vec2 texcoord = I.position.xy*mergePixelSize;
	//texcoord.x = 1 - texcoord.x;
	vec2 r = 1/mergePixelSize;
	texcoord = 0.5 + (texcoord-0.5);

	vec4 streakCol = streak.Sample(g_sampler, texcoord);
	vec4 bokehCol = bokeh.Sample(g_sampler, texcoord);
	vec4 blurredSmallCol = blurredSmall.Sample(g_sampler, texcoord);
	vec4 blurredMediumCol = blurredMedium.Sample(g_sampler, texcoord);
	vec4 blurredBigCol = blurredBig.Sample(g_sampler, texcoord);

	streakCol *= 2+fpn(vec3(0,texcoord.y*10000,0));

	// sum all the effects
	vec4 col = bokehCol*0.8 + blurredSmallCol*0.1 +  blurredMediumCol*0.1 + blurredBigCol*0.0;
	col +=  streakCol*2*streak_strength;
	
	col *= scenecolor_multiplier;

	//tone mapping
	PSOutput O;
	O.o0 = col;
	//col.rgb = col.rgb*(strobo*stroboMultiplier*30+1)+strobo*stroboColor*stroboMultiplier;
	O.o0.rgb = col.rgb*(1 + col.rgb/(maxwhite*maxwhite_multiplier))/(1+col.rgb); //reinhard tone mapping

	//color grading:
	O.o0.rgb = colorcorrect(O.o0.rgb);

	O.o0 *= vignette(texcoord,vignette_offset,pow(20.0,vignette_exponent));
	float noise = 0.0f;//noise3f(vec3(texcoord.xy*1000,t*10000)); FIX ME - add time!
	O.o0 = O.o0*(1+.08*noise*noise_strength) + .003*noise*noise_strength;
	O.o0 = pow(O.o0, 1/2.2); //gamma correction
	//O.o0 = blurredSmallCol;
	return O;
}