// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"
// DXUT Headers
#include <DXUT.h>
#include <DXUTmisc.h>
#include <SDKMisc.h>

#if defined(DEBUG) | defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// C++ RunTime Header Files
#include <string>
#include <iostream>
#include <list>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cfloat>
#include <cassert>
#include <set>
#include <fstream>





// Assimp Mesh Importer Header
#define ASSIMP_DLL
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>

// Bullet Physics Headers
#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>


// TODO: reference additional headers your program requires here
