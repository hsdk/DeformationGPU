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

#define MAGIC_EPSILON float3(0.001f, 0.001f, 0.001f)


cbuffer cbOBB : register( b12 ) {
	uint N;
}

Buffer<float4> covarianceD : register ( t0 );
Buffer<float4> covarianceL : register ( t1 );

RWBuffer<float4> ONB : register( u0 );

groupshared float4 tmp[1024];

float4 getIncrValueD(int i) {
	float4 x = covarianceD[i];
	if (i < int(N)) x += tmp[i/1024];
	return x;
}

float4 getIncrValueL(int i) {
	float4 x = covarianceL[i];
	if (i < int(N)) x += tmp[i/1024];
	return x;
}

float4 scanInclusiveIntraWarp(uint lane, uint i) {
	uint4 X = i - uint4(1,2,4,8);
	if (lane >= 0x01) tmp[i] += tmp[X.x];
	if (lane >= 0x02) tmp[i] += tmp[X.y];
	if (lane >= 0x04) tmp[i] += tmp[X.z];
	if (lane >= 0x08) tmp[i] += tmp[X.w];
	if (lane >= 0x10) tmp[i] += tmp[i-16];

	return tmp[i];
}

//---
//--------
// Stolen from http://source.openwalnut.org/src/core/graphicsEngine/shaders/shaders/WGETensorTools.glsl
//--------

// Muhahaha.
#define vec3 float3
#define vec4 float4
#define atan atan2
//atan(Y,X) (atan2((Y),(X)))

// tensors have to be stored in the following way:
// diag.x = Dxx     Diagonal elements are stored in one vector
// diag.y = Dyy
// diag.z = Dzz
// offdiag.x = Dyz  Off diagonal elements are stored in another vector
// offdiag.y = Dxz      where all components are stored in that location
// offdiag.z = Dxy      that is indexed by the coordinate that is not in the tensor index
//
// Dxx Dxy Dxz
// Dyx Dyy Dyz
// Dzx Dzy Dzz
//

// Well... I decided to use another ordering.
vec3 convertOffDiag(vec3 v) {
	return v.zyx;
}

// Method similar to the above by Hasan proposed by Cardano et al.
// implementation is more stable than the above one, so use this as
// default in all applications
float sqr(float a) {
    return a * a;
}

vec3 getEigenvaluesCardano(in vec3 diag, in vec3 offdiag) {
  const float M_SQRT3 = 1.73205080756887729352744634151;
  float de = offdiag.z * offdiag.x;
  float dd = sqr( offdiag.z );
  float ee = sqr( offdiag.x );
  float ff = sqr( offdiag.y );
  float m  = diag.x + diag.y + diag.z;
  float c1 = diag.x * diag.y + diag.x * diag.z + diag.y * diag.z
             - ( dd + ee + ff );
  float c0 = diag.z * dd + diag.x * ee + diag.y * ff - diag.x * diag.y * diag.z - 2. * offdiag.y * de;

  float p, sqrt_p, q, c, s, phi;
  p = sqr( m ) - 3. * c1;
  q = m * ( p - ( 3. / 2. ) * c1 ) - ( 27. / 2. ) * c0;
  sqrt_p = sqrt( abs( p ) );

  phi = 27. * ( 0.25 * sqr( c1 ) * ( p - c1 ) + c0 * ( q + 27. / 4. * c0 ) );
  phi = ( 1. / 3. ) * atan( sqrt( abs( phi ) ), q );

  c = sqrt_p * cos( phi );
  s = ( 1. / M_SQRT3 ) * sqrt_p * sin( phi );

  vec3 w;
  w[2] = ( 1. / 3. ) * ( m - c );
  w[1] = w[2] + s;
  w[0] = w[2] + c;
  w[2] -= s;
  return w;
}

// compute vector direction depending on information computed by getEigenvalues
// before (Hasan et al. 2001)
vec3 getEigenvector(vec3 ABC /*diag without eigenalue i*/, vec3 offdiag){
  vec3 vec;
  vec.x = ( offdiag.z * offdiag.x - ABC.y * offdiag.y ) * ( offdiag.y * offdiag.x - ABC.z * offdiag.z ); // FIXME
  //< last component is missing in the paper! there is only a Dx?
  vec.y = ( offdiag.y * offdiag.x - ABC.z * offdiag.z ) * ( offdiag.y * offdiag.z - ABC.x * offdiag.x );
  vec.z = ( offdiag.z * offdiag.x - ABC.y * offdiag.y ) * ( offdiag.y * offdiag.z - ABC.x * offdiag.x );

  return normalize(vec);
}
//--------
// Macros
#define SQR(x)      ((x)*(x))                        // x^2 


// ----------------------------------------------------------------------------
int dsyevj3(inout float3x3 A, inout float3x3 Q, inout float3 w)
// ----------------------------------------------------------------------------
// Calculates the eigenvalues and normalized eigenvectors of a symmetric 3x3
// matrix A using the Jacobi algorithm.
// The upper triangular part of A is destroyed during the calculation,
// the diagonal elements are read but not destroyed, and the lower
// triangular elements are not referenced at all.
// ----------------------------------------------------------------------------
// Parameters:
//   A: The symmetric input matrix
//   Q: Storage buffer for eigenvectors
//   w: Storage buffer for eigenvalues
// ----------------------------------------------------------------------------
// Return value:
//   0: Success
//  -1: Error (no convergence)
// ----------------------------------------------------------------------------
{
  const int n = 3;
  float sd, so;                  // Sums of diagonal resp. off-diagonal elements
  float s, c, t;                 // sin(phi), cos(phi), tan(phi) and temporary storage
  float g, h, z, theta;          // More temporary storage
  float thresh;
  
  // Initialize Q to the identitity matrix
#ifndef EVALS_ONLY
  for (int i=0; i < n; i++)
  {
    Q[i][i] = 1.0;
    for (int j=0; j < i; j++)
      Q[i][j] = Q[j][i] = 0.0;
  }
#endif

  // Initialize w to diag(A)
  for (int i=0; i < n; i++)
    w[i] = A[i][i];

  // Calculate SQR(tr(A))  
  sd = 0.0;
  for (int i=0; i < n; i++)
    sd += abs(w[i]);
  sd = SQR(sd);
 
  // Main iteration loop (16 is MAGIC - was 50)
  for (int nIter=0; nIter < 16; nIter++)
  {
    // Test for convergence 
    so = 0.0;
    for (int p=0; p < n; p++)
      for (int q=p+1; q < n; q++)
        so += abs(A[p][q]);
    if (so == 0.0)
      return 0;

    if (nIter < 4)
      thresh = 0.2 * so / SQR(n);
    else
      thresh = 0.0;

    // Do sweep
    for (int p=0; p < n; p++)
      for (int q=p+1; q < n; q++)
      {
        g = 100.0 * abs(A[p][q]);
        if (nIter > 4  &&  abs(w[p]) + g == abs(w[p])
                       &&  abs(w[q]) + g == abs(w[q]))
        {
          A[p][q] = 0.0;
        }
        else if (abs(A[p][q]) > thresh)
        {
          // Calculate Jacobi transformation
          h = w[q] - w[p];
          if (abs(h) + g == abs(h))
          {
            t = A[p][q] / h;
          }
          else
          {
            theta = 0.5 * h / A[p][q];
            if (theta < 0.0)
              t = -1.0 / (sqrt(1.0 + SQR(theta)) - theta);
            else
              t = 1.0 / (sqrt(1.0 + SQR(theta)) + theta);
          }
          c = 1.0/sqrt(1.0 + SQR(t));
          s = t * c;
          z = t * A[p][q];

          // Apply Jacobi transformation
          A[p][q] = 0.0;
          w[p] -= z;
          w[q] += z;
          for (int r=0; r < p; r++)
          {
            t = A[r][p];
            A[r][p] = c*t - s*A[r][q];
            A[r][q] = s*t + c*A[r][q];
          }
          for (int r=p+1; r < q; r++)
          {
            t = A[p][r];
            A[p][r] = c*t - s*A[r][q];
            A[r][q] = s*t + c*A[r][q];
          }
          for (int r=q+1; r < n; r++)
          {
            t = A[p][r];
            A[p][r] = c*t - s*A[q][r];
            A[q][r] = s*t + c*A[q][r];
          }

          // Update eigenvectors
#ifndef EVALS_ONLY          
          for (int r=0; r < n; r++)
          {
            t = Q[r][p];
            Q[r][p] = c*t - s*Q[r][q];
            Q[r][q] = s*t + c*Q[r][q];
          }
#endif
        }
      }
  }

  return -1;
}



//---

[numthreads(1024, 1, 1)]
void SkinningCS(	uint3 blockIdx : SV_GroupID, 
					uint3 DTid : SV_DispatchThreadID, 
					uint3 threadIdx : SV_GroupThreadID,
					uint GI : SV_GroupIndex ) {
	uint i = DTid.x;

	uint v = int(GI*1024)-1;

	uint lane = GI & 31u;
	uint warp = GI >> 5u;

	// DIAGONAL (D0 - D2)
	tmp[GI]  = v < N ? covarianceD[v] : 0.0f;

	float4 x = scanInclusiveIntraWarp(lane, GI);
	GroupMemoryBarrierWithGroupSync();

	if (lane == 31) tmp[warp] = x;
	GroupMemoryBarrierWithGroupSync();

	if (warp == 0) scanInclusiveIntraWarp(lane, lane);
	GroupMemoryBarrierWithGroupSync();

	if (warp > 0)
		x += tmp[warp-1];
	GroupMemoryBarrierWithGroupSync();
	float4 D = 0.0f;

	if (i == 0)
		D = getIncrValueD(int(N)-1);
		
	// LOWER (L0 - L1)
	tmp[GI]  = v < N ? covarianceL[v] : 0.0f;//texelFetch(inputL_, int(gl_LocalInvocationIndex*BLOCKSIZE)-1).xyzw;

	x = scanInclusiveIntraWarp(lane, GI);
	GroupMemoryBarrierWithGroupSync();

	if (lane == 31) tmp[warp] = x;
	GroupMemoryBarrierWithGroupSync();

	if (warp == 0) scanInclusiveIntraWarp(lane, lane);
	GroupMemoryBarrierWithGroupSync();

	if (warp > 0)
		x += tmp[warp-1];
	GroupMemoryBarrierWithGroupSync();
	
	float4 L = 0.0f;
	if (GI == 0) {
		L = getIncrValueL(int(N)-1).xyzw;
		//float3 offDiag = convertOffDiag(L.xyz+MAGIC_EPSILON);
		//float3 diag = D.xyz;
		
		float3x3 M = float3x3(
			D[0], L[0], L[1],
			L[0], D[1], L[2],
		    L[1], L[2], D[2]
		);
		
		float3x3 Q;
		float3 e;
		dsyevj3(M, Q, e);
		
		//float3 eigenvalues = getEigenvaluesCardano(diag, offDiag);
        //
		//float3 ABC0 = diag - eigenvalues.x;
		//float3 V0 = getEigenvector(ABC0, offDiag);
		//
		//float3 ABC1 = diag - eigenvalues.y;
		//float3 V1 = getEigenvector(ABC1, offDiag);
        //
		////vec3 ABC2 = diag - eigenvalues.z;
		////vec3 V2 = getEigenvector(ABC2, offDiag);
		//float3 V2 = normalize(cross(V0, V1));
        //
		//ONB[0] = float4(V0, 0.0f);
		//ONB[1] = float4(V1, 0.0f);
		//ONB[2] = float4(V2, 0.0f);
		//ONB[0] = float4(1,0,0,0);
		//ONB[1] = float4(0,1,0,0);
		//ONB[2] = float4(0,0,1,0);		
		
		//ONB[0] = float4(Q[0][0], Q[0][1], Q[0][2], 0.0f);
		//ONB[1] = float4(Q[1][0], Q[1][1], Q[1][2], 0.0f);
		//ONB[2] = float4(Q[2][0], Q[2][1], Q[2][2], 0.0f);

		ONB[0] = float4(Q[0][0], Q[1][0], Q[2][0], 0.0f);
		ONB[1] = float4(Q[0][1], Q[1][1], Q[2][1], 0.0f);
		ONB[2] = float4(Q[0][2], Q[1][2], Q[2][2], 0.0f);
	
		ONB[3] = float4(D);
		ONB[4] = float4(L);
		
	}	
}



////ONB[0] = float4(V0.x, V1.x, V2.x, 0.0f);
////ONB[1] = float4(V0.y, V1.y, V2.y, 0.0f);
////ONB[2] = float4(V0.z, V1.z, V2.z, 0.0f);
