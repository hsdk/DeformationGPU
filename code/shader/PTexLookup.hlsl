//struct PtexPacking
//{
//    int page;
//    int nMipmap;
//    int uOffset;
//    int vOffset;
//    int adjSizeDiffs[4];
//    int width;
//    int height;
//};

struct PtexPacking
{
	int page;	
	int uOffset;
	int vOffset;	
	uint tileSize;
	uint nMipmap;
};

SamplerState g_samplerPTex : register( s0 );

PtexPacking getPtexPacking(Buffer<uint> packings, int faceID)
{
	uint offset = faceID * 4;
    PtexPacking packing = (PtexPacking)0;
	packing.page = packings[offset];
	if((uint)packing.page == 0xffff )	// check if allocated
	{
		packing.page = -1;
		return packing;
	}    
    packing.uOffset   = packings[offset + 1];
    packing.vOffset   = packings[offset + 2];
	uint packedSizeMip = packings[offset + 3];
	packing.tileSize  = 1 << (packedSizeMip >> 8);
	packing.nMipmap   = (packedSizeMip & 0xff);

    return packing;
}

int computeMipmapOffsetU(int w, int level)
{
    int width = 1 << w;
    int m = (0x55555555 & (width | (width-1))) << (w&1);
    int x = ~((1 << (w -((level-1)&~1))) - 1);
    return (m & x) + ((level+1)&~1);
}

int computeMipmapOffsetV(int h, int level)
{
    int height = 1 << h;
    int m = (0x55555555 & (height-1)) << ((h+1)&1);;
    int x = ~((1 << (h - (level&~1))) - 1 );
    return (m & x) + (level&~1);
}

PtexPacking getPtexPacking(Buffer<uint> packings, int faceID, int level)
{
	uint offset = faceID*4;
	PtexPacking packing = (PtexPacking)0;
	packing.page = packings[offset].x;
	if((uint)packing.page == 0xffff )	// check if allocated
	{		
		packing.page = -1;
		return packing;
	}
	packing.uOffset    = packings[offset + 1].x;
	packing.vOffset    = packings[offset + 2].x;
	uint packedSizeMip = packings[offset + 3].x;
	int tileSizeLog2   = (packedSizeMip >> 8);
	packing.nMipmap    = (packedSizeMip & 0xff);

    // clamp max level
    level = min(level, packing.nMipmap);

	packing.uOffset += computeMipmapOffsetU(tileSizeLog2, level);
	packing.vOffset += computeMipmapOffsetV(tileSizeLog2, level);
	packing.tileSize = 1 << (tileSizeLog2 - level);	

    return packing;

}


float4 PTexLookupNearest(float4 patchCoord,
                         Texture2DArray data,
                         Buffer<uint> packings)
{
    //float2 uv = patchCoord.xy;
	float2 uv = saturate(patchCoord.xy); // clamp 0..1
    int faceID = patchCoord.w;
    PtexPacking ppack = getPtexPacking(packings, faceID);
	if(ppack.page == -1)
		return float4(0.5, 0, 0, -1);

	float2 coords = float2(	uv.x * ppack.tileSize + ppack.uOffset,
							uv.y * ppack.tileSize + ppack.vOffset);
	
    return data[int3(int(coords.x), int(coords.y), ppack.page)];

}

float4 PTexLookupNearestInclOverlap(float4 patchCoord,
                         Texture2DArray data,
                         Buffer<uint> packings)
{
    //float2 uv = patchCoord.xy;
	float2 uv = saturate(patchCoord.xy); // clamp 0..1
    int faceID = patchCoord.w;
    PtexPacking ppack = getPtexPacking(packings, faceID);
	if(ppack.page == -1)
		return float4(0.5, 0, 0, -1);
	//// test show with overlap data
	float2 coords = float2(ppack.uOffset - 1 + uv.x * (ppack.tileSize + 2),
		ppack.vOffset - 1 + uv.y * (ppack.tileSize + 2));
	
    return data[int3(int(coords.x), int(coords.y), ppack.page)];
}

float4 PTexLookup(float4 patchCoord,
                  int level,
                  Texture2DArray data,
                  Buffer<uint> packings)
{    
	float2 uv = saturate(patchCoord.xy); // clamp 0..1 MSAA fix
    int faceID = int(patchCoord.w);
    PtexPacking ppack = getPtexPacking(packings, faceID, level);
	if(ppack.page == -1)
	{
		return float4(0,0,0,-1);
	}

	float2 coords = float2(uv.x * ppack.tileSize + ppack.uOffset,
		uv.y * ppack.tileSize + ppack.vOffset);

    coords -= float2(0.5, 0.5);

    int c0X = int(floor(coords.x));
    int c1X = int(ceil(coords.x));
    int c0Y = int(floor(coords.y));
    int c1Y = int(ceil(coords.y));

    float t = coords.x - float(c0X);
    float s = coords.y - float(c0Y);

    float4 d0 = data[int3(c0X, c0Y, ppack.page)];
    float4 d1 = data[int3(c0X, c1Y, ppack.page)];
    float4 d2 = data[int3(c1X, c0Y, ppack.page)];
    float4 d3 = data[int3(c1X, c1Y, ppack.page)];

    float4 result = (1-t) * ((1-s)*d0 + s*d1) + t * ((1-s)*d2 + s*d3);

    return result;
}

// quadratic

void evalQuadraticBSpline(float u, out float B[3], out float BU[3])
{
    B[0] = 0.5 * (u*u - 2.0*u + 1);
    B[1] = 0.5 + u - u*u;
    B[2] = 0.5 * u*u;

    BU[0] = u - 1.0;
    BU[1] = 1 - 2 * u;
    BU[2] = u;
}

float4 PTexLookupQuadratic(out float4 du,
                           out float4 dv,
                           float4 patchCoord,
                           int level,
                           Texture2DArray<float> data,
                           Buffer<uint> packings)
{
    //float2 uv = patchCoord.xy;	
	float2 uv = saturate(patchCoord.xy); // clamp 0..1
    int faceID = int(patchCoord.w);
    PtexPacking ppack = getPtexPacking(packings, faceID, level);
	if(ppack.page == -1)
	{
		du = float4(0,0,0,0);
		dv = float4(0,0,0,0);		
		return float4(0,0,0,-1);
	}

	float2 coords = float2(uv.x * ppack.tileSize + ppack.uOffset,
		uv.y * ppack.tileSize + ppack.vOffset);

    coords -= float2(0.5, 0.5);

	

    int cX = int(round(coords.x));
    int cY = int(round(coords.y));



    float x = 0.5 - (float(cX) - coords.x);
    float y = 0.5 - (float(cY) - coords.y);

	//float2 coords2 = float2(uv.x*32,
    //                       uv.y*32);
	//
	//coords2 -= float2(0.5, 0.5);
	//int cX2 = int(round(coords2.x));
    //int cY2 = int(round(coords2.y));
	////float x = patchCoord.xy;uv.x;//0.5 - (float(cX) - coords.x);
	////float y = patchCoord.xy;uv.y;//0.5 - (float(cY) - coords.y);
	//
	//float x = 0.5 - (float(cX2) - coords2.x);
	//float y = 0.5 - (float(cY2) - coords2.y);

    float d[9];
    d[0] = data[int3(cX-1, cY-1, ppack.page)].x;
    d[1] = data[int3(cX-1, cY-0, ppack.page)].x;
    d[2] = data[int3(cX-1, cY+1, ppack.page)].x;
    d[3] = data[int3(cX-0, cY-1, ppack.page)].x;
    d[4] = data[int3(cX-0, cY-0, ppack.page)].x;
    d[5] = data[int3(cX-0, cY+1, ppack.page)].x;
    d[6] = data[int3(cX+1, cY-1, ppack.page)].x;
    d[7] = data[int3(cX+1, cY-0, ppack.page)].x;
    d[8] = data[int3(cX+1, cY+1, ppack.page)].x;

    float B[3], D[3];    
    evalQuadraticBSpline(y, B, D);

	float BUCP[3], DUCP[3];
    for (int i = 0; i < 3; ++i) {
		BUCP[i] = 0;// float4(0, 0, 0, 0);
		DUCP[i] = 0;//float4(0, 0, 0, 0);
		[unroll]
        for (int j = 0; j < 3; j++) {
            float A = d[i*3+j];
            BUCP[i] += A * B[j];
            DUCP[i] += A * D[j];
        }
    }

    evalQuadraticBSpline(x, B, D);

    float4 result = float4(0, 0, 0, 0);
    du = float4(0, 0, 0, 0);
    dv = float4(0, 0, 0, 0);
    for (int k = 0; k < 3; ++k) {
        result.x += B[k] * BUCP[k];
        du.x += D[k] * BUCP[k];
        dv.x += B[k] * DUCP[k];
    }

	du *= ppack.tileSize;
	dv *= ppack.tileSize;

    return result;
}

float4 PTexMipmapLookup(float4 patchCoord,
                        float level,
                        Texture2DArray data,
                        Buffer<uint> packings)
{
#if defined(SEAMLESS_MIPMAP)
    // diff level
    int faceID = int(patchCoord.w);	
    //float2 uv = patchCoord.xy;
	float2 uv = saturate(patchCoord.xy); // clamp 0..1
    PtexPacking packing = getPtexPacking(packings, faceID);
    level += lerp(lerp(packing.adjSizeDiffs[0], packing.adjSizeDiffs[1], uv.x),
                  lerp(packing.adjSizeDiffs[3], packing.adjSizeDiffs[2], uv.x),
                  uv.y);
#endif

    int levelm = int(floor(level));
    int levelp = int(ceil(level));
    float t = level - float(levelm);

    float4 result = (1-t) * PTexLookup(patchCoord, levelm, data, packings)
        + t * PTexLookup(patchCoord, levelp, data, packings);
    return result;
}

float4 PTexMipmapLookupQuadratic(out float4 du,
                                 out float4 dv,
                                 in float4 patchCoord,
                                 in float level,
                                 in Texture2DArray<float> data,
                                 in Buffer<uint> packings)
{
	// MSAA patchcoord > 1  or < 0 fix
	patchCoord.xy = saturate(patchCoord.xy);
#if defined(SEAMLESS_MIPMAP)
    // diff level
	
    int faceID = int(patchCoord.w);
    //float2 uv = patchCoord.xy;
	float2 uv = saturate(patchCoord.xy); // clamp 0..1
    PtexPacking packing = getPtexPacking(packings, faceID);
    level += lerp(lerp(packing.adjSizeDiffs[0], packing.adjSizeDiffs[1], uv.x),
                  lerp(packing.adjSizeDiffs[3], packing.adjSizeDiffs[2], uv.x),
                  uv.y);
#endif

    int levelm = int(floor(level));
    int levelp = int(ceil(level));
    float t = level - float(levelm);

    float4 du0, du1, dv0, dv1;
    float4 r0 = PTexLookupQuadratic(du0, dv0, patchCoord, levelm, data, packings);
    float4 r1 = PTexLookupQuadratic(du1, dv1, patchCoord, levelp, data, packings);

    float4 result = lerp(r0, r1, t);
    du = lerp(du0, du1, t);
    dv = lerp(dv0, dv1, t);

    return result;
}

float4 PTexMipmapLookupQuadratic(float4 patchCoord,
                                 float level,
                                 Texture2DArray<float> data,
                                 Buffer<uint> packings)
{
    float4 du, dv;
    return PTexMipmapLookupQuadratic(du, dv, patchCoord, level, data, packings);
}





float4 PTexLookupFast(float4 patchCoord,
                      Texture2DArray data,
                      Buffer<uint> packings)
{
	//float2 uv = patchCoord.xy;
	float2 uv = saturate(patchCoord.xy); // clamp 0..1
    int faceID = patchCoord.w;
    PtexPacking ppack = getPtexPacking(packings, faceID);
	if(ppack.page == -1)
		return float4(1,0,0,-1);

	uint texW;
	uint texH;
	uint elems;
	data.GetDimensions(texW,texH, elems);

	float3 coords = float3((uv.x * ppack.tileSize + ppack.uOffset) / float(texW),
		(uv.y * ppack.tileSize + ppack.vOffset) / float(texH),
						   ppack.page);

	float4 result = data.Sample(g_samplerPTex, coords).xyzw;    
	return result;
}




// rw ptex lookup

float4 PTexLookupQuadraticRW(out float4 du,
                           out float4 dv,
                           float4 patchCoord,
                           int level,
                           RWTexture2DArray<float> data,
                           Buffer<uint> packings)
{
    //float2 uv = patchCoord.xy;
	float2 uv = saturate(patchCoord.xy);
    int faceID = int(patchCoord.w);
    PtexPacking ppack = getPtexPacking(packings, faceID, level);
	if(ppack.page == -1)
	{
		du = float4(0,0,0,0);
		dv = float4(0,0,0,0);		
		return float4(0,0,0,-1);
	}
	float2 coords = float2(uv.x * ppack.tileSize + ppack.uOffset,
		uv.y * ppack.tileSize + ppack.vOffset);

    coords -= float2(0.5, 0.5);

    int cX = int(round(coords.x));
    int cY = int(round(coords.y));

    float x = 0.5 - (float(cX) - coords.x);
    float y = 0.5 - (float(cY) - coords.y);

    // ---------------------------

    float4 d[9];
    d[0] = data[int3(cX-1, cY-1, ppack.page)];
    d[1] = data[int3(cX-1, cY-0, ppack.page)];
    d[2] = data[int3(cX-1, cY+1, ppack.page)];
    d[3] = data[int3(cX-0, cY-1, ppack.page)];
    d[4] = data[int3(cX-0, cY-0, ppack.page)];
    d[5] = data[int3(cX-0, cY+1, ppack.page)];
    d[6] = data[int3(cX+1, cY-1, ppack.page)];
    d[7] = data[int3(cX+1, cY-0, ppack.page)];
    d[8] = data[int3(cX+1, cY+1, ppack.page)];

    static float B[3], D[3];
    static float4 BUCP[3], DUCP[3];
    evalQuadraticBSpline(y, B, D);

    for (int i = 0; i < 3; ++i) {
        BUCP[i] = float4(0, 0, 0, 0);
        DUCP[i] = float4(0, 0, 0, 0);
        for (int j = 0; j < 3; j++) {
            float4 A = d[i*3+j];
            BUCP[i] += A * B[j];
            DUCP[i] += A * D[j];
        }
    }

    evalQuadraticBSpline(x, B, D);

    float4 result = float4(0, 0, 0, 0);
    du = float4(0, 0, 0, 0);
    dv = float4(0, 0, 0, 0);
	{
	for (int i = 0; i < 3; ++i) {
        result += B[i] * BUCP[i];
        du += D[i] * BUCP[i];
        dv += B[i] * DUCP[i];
    }
	}
	du *= ppack.tileSize;
	dv *= ppack.tileSize;

    return result;
}

float4 PTexMipmapLookupQuadraticRW(out float4 du,
                                 out float4 dv,
                                 in float4 patchCoord,
                                 in float level,
                                 in RWTexture2DArray<float> data,
                                 in Buffer<uint> packings)
{
//#if defined(SEAMLESS_MIPMAP)
//    // diff level
//    int faceID = int(patchCoord.w);
//    float2 uv = patchCoord.xy;
//    PtexPacking packing = getPtexPacking(packings, faceID);
//    level += lerp(lerp(packing.adjSizeDiffs[0], packing.adjSizeDiffs[1], uv.x),
//                  lerp(packing.adjSizeDiffs[3], packing.adjSizeDiffs[2], uv.x),
//                  uv.y);
//#endif

    int levelm = int(floor(level));
    int levelp = int(ceil(level));
    float t = level - float(levelm);

    float4 du0, du1, dv0, dv1;
    float4 r0 = PTexLookupQuadraticRW(du0, dv0, patchCoord, levelm, data, packings);
    float4 r1 = PTexLookupQuadraticRW(du1, dv1, patchCoord, levelp, data, packings);

    float4 result = lerp(r0, r1, t);
    du = lerp(du0, du1, t);
    dv = lerp(dv0, dv1, t);

    return result;
}

float4 PTexMipmapLookupQuadraticRW(float4 patchCoord,
                                 float level,
                                 RWTexture2DArray<float> data,
                                 Buffer<uint> packings)
{
    float4 du, dv;
    return PTexMipmapLookupQuadraticRW(du, dv, patchCoord, level, data, packings);
}