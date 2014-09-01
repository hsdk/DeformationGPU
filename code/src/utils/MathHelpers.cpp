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

#include "stdafx.h"

//#include <DXUT.h>
#include "MathHelpers.h"

#include <cassert>

template<class T>
T round( const T &f )
{
	return (f > (T)0.0) ? floor(f + (T)0.5) : ceil(f - (T)0.5);
}
template FLOAT round<FLOAT>( const FLOAT &f );
template DOUBLE round<DOUBLE>( const DOUBLE &f );



template<class T>
bool isPower2( const T &x )
{
	return (x & (x-1)) == 0;
}
template bool isPower2<INT>(const INT &i);
template bool isPower2<UINT>(const UINT &i);


template<class T> 
T nextLargeestPow2(T x) {
	assert(sizeof(T) == 4);
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return(x+1);
}

template INT nextLargeestPow2<INT>(INT x);
template UINT nextLargeestPow2<UINT>(UINT x);


template<class T>
T log2Integer( T x )
{
	T r = 0;
	while (x >>= 1)	r++;
	return r;
}

template INT log2Integer<INT>(INT x);
template UINT log2Integer<UINT>(UINT x);
