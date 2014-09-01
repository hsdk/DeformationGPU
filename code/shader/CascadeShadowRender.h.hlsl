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

#define SELECT_CASCADE_BY_INTERVAL_FLAG

Texture2DArray  g_ShadowMap              : register( t12 );
SamplerState	g_ShadowSampler          : register( s6 );


float Linstep(float a, float b, float v)
{
    return saturate((v - a) / (b - a));
}

// Reduces VSM light bleedning
float ReduceLightBleeding(float pMax, float amount)
{
  // Remove the [0, amount] tail and linearly rescale (amount, 1].
   return Linstep(amount, 1.0f, pMax);
}

float ChebyshevUpperBound(float2 moments, float mean, float minVariance,
                          float lightBleedingReduction)
{
    // Compute variance
    float variance = moments.y - (moments.x * moments.x);
    variance = max(variance, minVariance);

    // Compute probabilistic upper bound
    float d = mean - moments.x;
    float pMax = variance / (variance + (d * d));

    pMax = ReduceLightBleeding(pMax, lightBleedingReduction);

    // One-tailed Chebyshev
    return (mean <= moments.x ? 1.0f : pMax);
}

// Sample the variance shadow map
float SampleShadowMapVSM(in float3 shadowPos, in float3 shadowPosDX, in float3 shadowPosDY, in uint cascadeIdx)
{

	float depth = shadowPos.z;
	float2 occluder = g_ShadowMap.SampleGrad(g_ShadowSampler, float3(shadowPos.xy, cascadeIdx), shadowPosDX.xy, shadowPosDY.xy).xy;
	
	//return ChebyshevUpperBound(occluder, depth, 0.00001f,0.3);

	float  fAvgZ  = occluder.x; // Filtered z
    float  fAvgZ2 = occluder.y; // Filtered z-squared
    
	float fPercentLit = 1.f;
    if ( shadowPos.z <= fAvgZ ) // We put the z value in w so that we can index the texture array with Z.
    {
        fPercentLit = 1.0;		
	}
	else 
	{
	    float variance = ( fAvgZ2 ) - ( fAvgZ * fAvgZ );
	    variance       = min( 1.0f, max( 0.0f, variance + 0.00001f ) );
    
        float mean     = fAvgZ;
        float d        = shadowPos.z - mean; // We put the z value in w so that we can index the texture array with Z.
        float p_max    = variance / ( variance + d*d );
		 // To combat light-bleeding, experiment with raising p_max to some power
        // (Try values from 0.1 to 100.0, if you like.)        	    
		fPercentLit = p_max*p_max
					  //*p_max*p_max*p_max*p_max*p_max*p_max
					  //*p_max*p_max*p_max*p_max*p_max*p_max
					  //*p_max*p_max*p_max*p_max*p_max*p_max
					  //*p_max*p_max*p_max*p_max*p_max*p_max
					  ;	
	}
	return fPercentLit;
}
						  

// Sample the shadow map cascade
float3 SampleShadowCascade(in float3 shadowPos, in float3 shadowPosDX, in float3 shadowPosDY, in uint cascadeIdx)
{	

#ifdef SHADOW_OLD
	shadowPos *= g_cascadeScale[cascadeIdx].xyz;
	shadowPos += g_cascadeOffset[cascadeIdx].xyz;
#else
	shadowPos += g_cascadeOffset[cascadeIdx].xyz;
	shadowPos *= g_cascadeScale[cascadeIdx].xyz;
#endif


	
	
	
	

	shadowPosDX *= g_cascadeScale[cascadeIdx].xyz;
	shadowPosDY *= g_cascadeScale[cascadeIdx].xyz;

	float3 cascadeColor = 1.f;

	if(g_visualizeCascades)
	{
		const float3 CascadeColors[4] = 
		{
			float3(1,0,0),
			float3(0,1,0),
			float3(0,0,1),
			float3(1,1,0)
		};
		cascadeColor = CascadeColors[cascadeIdx];
	}	
	
	float shadow = SampleShadowMapVSM(shadowPos, shadowPosDX, shadowPosDY, cascadeIdx);

	return shadow*cascadeColor;	
}
 
//-------------------------------------------------------------------------------------------------
// Calculates the offset to use for sampling the shadow map, based on the surface normal
//-------------------------------------------------------------------------------------------------
float3 GetShadowPosOffset(in float nDotL, in float3 normal)
{
    float2 shadowMapSize;
    float numSlices;
    g_ShadowMap.GetDimensions(shadowMapSize.x, shadowMapSize.y, numSlices);
    float texelSize = 2.0f / shadowMapSize.x;
    float nmlOffsetScale = saturate(1.0f - nDotL);
    //return texelSize * g_OffsetScale * nmlOffsetScale * normal;
	return texelSize * 0.1 * nmlOffsetScale * normal;
}

// checkme
float3 ShadowVisibility(in float3 positionLightES, in float depthVS, in float nDotL, in float3 normal)
{
	float3 shadowVisibility = 1.f;
	uint cascadeIdx = 0;
	
	// select cascade
	[unroll]
	for(uint i = 0; i < NUM_CASCADES-1; ++i)
	{
		[flatten]
		if(depthVS > g_cascadeSplitDepthsES[i])
			cascadeIdx = i+1;
		
	}

	// apply offset
	float3 offset = GetShadowPosOffset(nDotL, normal)/abs(g_cascadeScale[cascadeIdx].z);


	// we are already in shadow space
	//float3 shadowPos = positionLightES+offset;

	// or if ws pos project into shadow space
	float3 samplePos = positionLightES + offset;
	float3 shadowPos = mul(g_lightViewMatrix, float4(samplePos, 1.0)).xyz;

	float3 shadowPosDX = ddx_fine(shadowPos);
	float3 shadowPosDY = ddy_fine(shadowPos);

	shadowVisibility = SampleShadowCascade(shadowPos, shadowPosDX, shadowPosDY, cascadeIdx);

//#define BLEND_BETWEEN_CASCADES
#ifdef BLEND_BETWEEN_CASCADES
	//sample next cascade and blend between the two results to smooth the transition
	const float BlendThreshold = 0.1f;
	float nextSplit = g_cascadeSplitDepthsES[cascadeIdx];
	float splitSize = cascadeIdx == 0 ? nextSplit : nextSplit - g_cascadeSplitDepthsES[cascadeIdx -1];
	float splitDist = (nextSplit-depthVS)/splitSize;

	[branch]
	if(splitDist <= BlendThreshold && cascadeIdx!= NUM_CASCADES -1 )
	{
		float3 nextSplitVisibility = SampleShadowCascade(shadowPos, shadowPosDX, shadowPosDY, cascadeIdx+1);
		float lerpAmt = smoothstep(0.0f, BlendThreshold, splitDist);
		shadowVisibility = lerp(nextSplitVisibility, shadowVisibility, lerpAmt);
	}
#endif

	return shadowVisibility;
}



// OLD
static const float4 vCascadeColorsMultiplier[8] = 
{
    float4 ( 1.5f, 0.0f, 0.0f, 1.0f ),
    float4 ( 0.0f, 1.5f, 0.0f, 1.0f ),
    float4 ( 0.0f, 0.0f, 5.5f, 1.0f ),
    float4 ( 1.5f, 0.0f, 5.5f, 1.0f ),
    float4 ( 1.5f, 1.5f, 0.0f, 1.0f ),
    float4 ( 1.0f, 1.0f, 1.0f, 1.0f ),
    float4 ( 0.0f, 1.0f, 5.5f, 1.0f ),
    float4 ( 0.5f, 3.5f, 0.75f, 1.0f )
};


void ComputeCoordinatesTransform( in int iCascadeIndex,                                  
                                  in out float4 vShadowTexCoord,
                                  in out float4 vShadowTexCoordViewSpace ) 
{
    // Now that we know the correct map, we can transform the world space position of the current fragment                
#ifdef SELECT_CASCADE_BY_INTERVAL_FLAG 
    {
        vShadowTexCoord = vShadowTexCoordViewSpace * g_cascadeScale[iCascadeIndex];
        vShadowTexCoord += g_cascadeOffset[iCascadeIndex];
    }  
#endif
    vShadowTexCoord.w = vShadowTexCoord.z; // We put the z value in w so that we can index the texture array with Z.
    vShadowTexCoord.z = iCascadeIndex;
    
} 

//--------------------------------------------------------------------------------------
// Use PCF to sample the depth map and return a percent lit value.
//--------------------------------------------------------------------------------------
void CalculateVarianceShadow ( in float4 vShadowTexCoord, in float4 vShadowMapTextureCoordViewSpace, int iCascade, out float fPercentLit ) 
{
    fPercentLit = 0.0f;
    // This loop could be unrolled, and texture immediate offsets could be used if the kernel size were fixed.
    // This would be a performance improvment.
	        
    float2 mapDepth = 0;

    // In orderto pull the derivative out of divergent flow control we calculate the 
    // derivative off of the view space coordinates an then scale the deriviative.
    
    float3 vShadowTexCoordDDX = ddx_fine(vShadowMapTextureCoordViewSpace.xyz ).xyz;
    vShadowTexCoordDDX *= g_cascadeScale[iCascade].xyz; 
    
	float3 vShadowTexCoordDDY = ddy_fine(vShadowMapTextureCoordViewSpace.xyz ).xyz;
    vShadowTexCoordDDY *= g_cascadeScale[iCascade].xyz; 
    
    mapDepth += g_ShadowMap.SampleGrad( g_ShadowSampler, vShadowTexCoord.xyz, 
									   vShadowTexCoordDDX.xy,
									   vShadowTexCoordDDY.xy).xy;
    // The sample instruction uses gradients for some filters.
		        
    float  fAvgZ  = mapDepth.x; // Filtered z
    float  fAvgZ2 = mapDepth.y; // Filtered z-squared
    
    if ( vShadowTexCoord.w <= fAvgZ ) // We put the z value in w so that we can index the texture array with Z.
    {
        fPercentLit = 1;		
	}
	else 
	{
	    float variance = ( fAvgZ2 ) - ( fAvgZ * fAvgZ );
        variance       = min( 1.0f, max( 0.0f, variance + 0.00001f ) );
    
        float mean     = fAvgZ;
        float d        = vShadowTexCoord.w - mean; // We put the z value in w so that we can index the texture array with Z.
        float p_max    = variance / ( variance + d*d );

        // To combat light-bleeding, experiment with raising p_max to some power
        // (Try values from 0.1 to 100.0, if you like.)
        //fPercentLit = pow( p_max, 4 );	    
		fPercentLit = p_max*p_max;
	}

	//if(vShadowMapTextureCoordViewSpace.x > 1 || vShadowMapTextureCoordViewSpace.y > 1)
		//fPercentLit = vShadowMapTextureCoordViewSpace.w;
	//fPercentLit = ChebyshevUpperBound(mapDepth,vShadowTexCoord.w, 0.00001f, 0.3);
    
}

//--------------------------------------------------------------------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------------------------------------------------------------------
void CalculateBlendAmountForInterval ( in int iNextCascadeIndex, 
                                       in out float fPixelDepth, 
                                       in out float fCurrentPixelsBlendBandLocation,
                                       out float fBlendBetweenCascadesAmount
                                       ) 
{

    // We need to calculate the band of the current shadow map where it will fade into the next cascade.
    // We can then early out of the expensive PCF for loop.
    // 
    float fBlendInterval = g_cascadeSplitDepthsES[ iNextCascadeIndex - 1 ];
    if( iNextCascadeIndex > 1 ) 
    {
        fPixelDepth -= g_cascadeSplitDepthsES[ iNextCascadeIndex-2 ];
        fBlendInterval -= g_cascadeSplitDepthsES[ iNextCascadeIndex-2 ];
    } 
    // The current pixel's blend band location will be used to determine when we need to blend and by how much.
    fCurrentPixelsBlendBandLocation = fPixelDepth / fBlendInterval;
    fCurrentPixelsBlendBandLocation = 1.0f - fCurrentPixelsBlendBandLocation;
    // The fBlendBetweenCascadesAmount is our location in the blend band.
    fBlendBetweenCascadesAmount = fCurrentPixelsBlendBandLocation / g_cascadeBlendArea;
}


//--------------------------------------------------------------------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------------------------------------------------------------------
void CalculateBlendAmountForMap ( in float4 vShadowMapTextureCoord, 
                                  in out float fCurrentPixelsBlendBandLocation,
                                  out float fBlendBetweenCascadesAmount ) 
{
    // Calcaulte the blend band for the map based selection.
    float2 distanceToOne = float2 ( 1.0f - vShadowMapTextureCoord.x, 1.0f - vShadowMapTextureCoord.y );
    fCurrentPixelsBlendBandLocation = min( vShadowMapTextureCoord.x, vShadowMapTextureCoord.y );
    float fCurrentPixelsBlendBandLocation2 = min( distanceToOne.x, distanceToOne.y );
    fCurrentPixelsBlendBandLocation =  min( fCurrentPixelsBlendBandLocation, fCurrentPixelsBlendBandLocation2 );
    fBlendBetweenCascadesAmount = fCurrentPixelsBlendBandLocation / g_cascadeBlendArea;
}

// vDepth = esPos.z, vTexShadow = lightView*posWorld, esNormal = worldView*osNormal, esLightDir = mView * lightDir
// esNormal and esLightDir have to be normalized before sending to eval
float3 EvalCascadeShadow(in float vDepth, in float4 vPosLightSpace, in float3 esNormal, in float3 esLightDir)
{
	float4 vShadowMapTextureCoordViewSpace = 0.0f;
    float4 vShadowMapTextureCoord = 0.0f;
    float4 vShadowMapTextureCoord_blend = 0.0f;
    
    float4 vVisualizeCascadeColor = float4(0.0f,0.0f,0.0f,1.0f);
    
    float fPercentLit = 0.0f;
    float fPercentLit_blend = 0.0f;

    int iCascadeFound = 0;
    int iCurrentCascadeIndex=1;
    int iNextCascadeIndex = 0;

    float fCurrentPixelDepth;

    // The interval based selection technique compares the pixel's depth against the frustum's cascade divisions.
    fCurrentPixelDepth = vDepth;	
    
    // This for loop is not necessary when the frustum is uniformaly divided and interval based selection is used.
    // In this case fCurrentPixelDepth could be used as an array lookup into the correct frustum. 
    //vShadowMapTextureCoordViewSpace = vPosLightSpace;
    
	vShadowMapTextureCoordViewSpace = mul(g_lightViewMatrix, float4(vPosLightSpace.xyz, 1.0));
    
#ifdef  SELECT_CASCADE_BY_INTERVAL_FLAG 
    {
        iCurrentCascadeIndex = 0;
        if (NUM_CASCADES > 1 ) 
        {
            float4 vCurrentPixelDepth = vDepth;
            float4 fComparison = ( vCurrentPixelDepth > g_cascadeSplitDepthsES);            
            float fIndex = dot( float4( NUM_CASCADES > 0,
										NUM_CASCADES > 1, 
										NUM_CASCADES > 2,
										NUM_CASCADES > 3)
                            , fComparison );

                                    
            fIndex = min( fIndex, NUM_CASCADES - 1 );
            iCurrentCascadeIndex = (int)fIndex;
        }
    }
#else    
    //if ( !SELECT_CASCADE_BY_INTERVAL_FLAG ) 
    {
        iCurrentCascadeIndex = 0;
        if ( NUM_CASCADES == 1 ) 
        {
            vShadowMapTextureCoord = vShadowMapTextureCoordViewSpace * g_cascadeScale[0];
            vShadowMapTextureCoord += g_cascadeOffset[0];
        }
        if ( NUM_CASCADES > 1 ) {
            for( int iCascadeIndex = 0; iCascadeIndex < NUM_CASCADES && iCascadeFound == 0; ++iCascadeIndex ) 
            {
                vShadowMapTextureCoord = vShadowMapTextureCoordViewSpace * g_cascadeScale[iCascadeIndex];
                vShadowMapTextureCoord += g_cascadeOffset[iCascadeIndex];

                if ( min( vShadowMapTextureCoord.x, vShadowMapTextureCoord.y ) > g_minBorderPadding
                  && max( vShadowMapTextureCoord.x, vShadowMapTextureCoord.y ) < g_maxBorderPadding )
                { 
                    iCurrentCascadeIndex = iCascadeIndex;   
                    iCascadeFound = 1; 
                }
            }
        }
    }    
#endif
    // Found the correct map.
    vVisualizeCascadeColor = vCascadeColorsMultiplier[iCurrentCascadeIndex];
    
    //ComputeCoordinatesTransform( iCurrentCascadeIndex, vInterpPos, vShadowMapTextureCoord, vShadowMapTextureCoordViewSpace  );    
	ComputeCoordinatesTransform( iCurrentCascadeIndex, vShadowMapTextureCoord, vShadowMapTextureCoordViewSpace  );    
    
	bool blendBetweenCascades = g_cascadeBlendArea > 0;
    if( blendBetweenCascades && NUM_CASCADES > 1 ) 
    {
        // Repeat text coord calculations for the next cascade. 
        // The next cascade index is used for blurring between maps.
        iNextCascadeIndex = min ( NUM_CASCADES - 1, iCurrentCascadeIndex + 1 ); 
#ifndef SELECT_CASCADE_BY_INTERVAL_FLAG
        //if( !SELECT_CASCADE_BY_INTERVAL_FLAG ) 
        {
            vShadowMapTextureCoord_blend = vShadowMapTextureCoordViewSpace * g_cascadeScale[iNextCascadeIndex];
            vShadowMapTextureCoord_blend += g_cascadeOffset[iNextCascadeIndex];
        }        
#endif
		ComputeCoordinatesTransform( iNextCascadeIndex, vShadowMapTextureCoord_blend, vShadowMapTextureCoordViewSpace );  
    }            
    float fBlendBetweenCascadesAmount = 1.0f;
    float fCurrentPixelsBlendBandLocation = 1.0f;
    
#ifdef SELECT_CASCADE_BY_INTERVAL_FLAG
    //if( SELECT_CASCADE_BY_INTERVAL_FLAG ) 
    {
        if( NUM_CASCADES > 1 && blendBetweenCascades ) 
        {
            CalculateBlendAmountForInterval ( iNextCascadeIndex, fCurrentPixelDepth, fCurrentPixelsBlendBandLocation, fBlendBetweenCascadesAmount );
            
        }   
    }
#else
    //else 
    {
        if( NUM_CASCADES > 1 && blendBetweenCascades ) 
        {
            CalculateBlendAmountForMap ( vShadowMapTextureCoord, fCurrentPixelsBlendBandLocation, fBlendBetweenCascadesAmount );			
        }   
    }
#endif
    
    // Because the Z coordinate specifies the texture array,
    // the derivative will be 0 when there is no divergence
    //float fDivergence = abs( ddy( vShadowMapTextureCoord.z ) ) +  abs( ddx( vShadowMapTextureCoord.z ) );
    CalculateVarianceShadow ( vShadowMapTextureCoord, vShadowMapTextureCoordViewSpace, iCurrentCascadeIndex, fPercentLit);
								
    // We repeat the calcuation for the next cascade layer, when blending between maps.
    if(blendBetweenCascades  && NUM_CASCADES > 1 ) 
    {
        if( fCurrentPixelsBlendBandLocation < g_cascadeBlendArea ) 
        {  // the current pixel is within the blend band.

			// Because the Z coordinate species the texture array,
			// the derivative will be 0 when there is no divergence
			float fDivergence =   abs( ddy( vShadowMapTextureCoord_blend.z ) )   
								+ abs( ddx( vShadowMapTextureCoord_blend.z) );
            CalculateVarianceShadow ( vShadowMapTextureCoord_blend, vShadowMapTextureCoordViewSpace, 
										iNextCascadeIndex, fPercentLit_blend );

            // Blend the two calculated shadows by the blend amount.
            fPercentLit = lerp( fPercentLit_blend, fPercentLit, fBlendBetweenCascadesAmount ); 
        }   
    }    
  
    if( !g_visualizeCascades ) vVisualizeCascadeColor = float4( 1.0f, 1.0f, 1.0f, 1.0f );
    
    float3 vLightDir1 = float3( -1.0f, 1.0f, -1.0f ); 
    float3 vLightDir2 = float3( 1.0f, 1.0f, -1.0f ); 
    float3 vLightDir3 = float3( 0.0f, -1.0f, 0.0f );
    float3 vLightDir4 = float3( 1.0f, 1.0f, 1.0f );     
    // Some ambient-like lighting.
    float fLighting = 
                      saturate( dot( vLightDir1 , esNormal ) )*0.05f +
                      saturate( dot( vLightDir2 , esNormal ) )*0.05f +
                      saturate( dot( vLightDir3 , esNormal ) )*0.05f +
                      saturate( dot( vLightDir4 , esNormal ) )*0.05f ;
    
    float vShadowLighting = fLighting * 0.5f;    
	fLighting += saturate( dot( normalize(esLightDir).xyz , esNormal.xyz ) );
    fLighting = lerp( vShadowLighting, fLighting, fPercentLit );

	return fLighting * vVisualizeCascadeColor.xyz;
}