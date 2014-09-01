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

#define ASSIMP_DLL
//#define CHECKLEAKS

#include "stdafx.h"


#include <SDKMisc.h>
#include <assimp/cimport.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/mesh.h>

#include "ModelLoader.h"
#include "DXModel.h"
#include "DXMaterial.h"
#include "Scene.h"


#include "dynamics/Physics.h"
#include <SDX/StringConversion.h>
#include "utils/SpatialSort.h"
#include "scene/ModelInstance.h"
#include "dynamics/AnimationGroup.h"

#include "scene/DXSubDModel.h"

#include <iostream>
#include <fstream>
#include <unordered_map>



#ifdef CHECKLEAKS
#include "DbgNew.h" // has to be last include
#else
#include <Serialize/BulletWorldImporter/btBulletWorldImporter.h>
#endif


using namespace DirectX;
using namespace SkinningAnimationClasses;

static std::string gCurrModelPath = "";


static std::string ToUpperCase(std::string input) 
{
	for (std::string::iterator it = input.begin(); it != input.end(); ++ it)
		*it = toupper(*it);
	return input;
}

bool StringContains(const std::string& str, const std::string& search)
{
	std::string searchString = ToUpperCase(search);
	return ToUpperCase(std::string(str)).find(searchString) != std::string::npos;
}

// removes duplicates from position data, needed e.g. for catmull clark subd
// non sharable attributes such as uv coords are applied per face
bool MakeSharedVertex(const std::vector<XMFLOAT4A> &vertices, std::vector<XMFLOAT4A>& uniqueVertices, std::vector<unsigned int>& replaceIndex)
{
	uniqueVertices.reserve(vertices.size());
	replaceIndex.resize(vertices.size(), 0xffffffff);

	SpatialSort vertexFinder;
	vertexFinder.Fill(&vertices[0], static_cast<unsigned int>(vertices.size()),sizeof(XMFLOAT4A));

	std::vector<unsigned int> verticesFound;
	verticesFound.reserve(10);

	for(unsigned int a = 0; a < vertices.size(); ++a)
	{
		XMFLOAT4A v = vertices[a];
		vertexFinder.FindIdenticalPositions(v, verticesFound);
		unsigned int matchIndex = 0xffffffff;

		// check unique indices close enough if vertex is already present
		for(unsigned int b = 0; b < verticesFound.size();++b)
		{
			const unsigned int vIdx = verticesFound[b];	
			const unsigned int uIdx = replaceIndex[vIdx];

			if(uIdx & 0x80000000) continue;

			matchIndex = uIdx;
			break;
		}

		if(matchIndex != 0xffffffff)
		{
			// store mapping to unique vertex
			replaceIndex[a] = matchIndex | 0x80000000;		
		}
		else
		{			
			// no unique vertex found, so add it
			replaceIndex[a] = (unsigned int) uniqueVertices.size();
			uniqueVertices.push_back(v);
		}		
	}

	for(int i = 0 ; i < replaceIndex.size(); ++i)
		replaceIndex[i] &= ~0x80000000;

	return S_OK;
}


HRESULT CreateOSDModel(const MeshData* meshData, DXOSDMesh* model, bool isDeformable)
{	
	HRESULT hr = S_OK;
	assert(meshData->meshes.size() == 1);

	std::cout << "create osd model" << std::endl;

	// make shared vertex set using only position attribute
	std::vector<XMFLOAT4A>	uniqueVertices;
	std::vector<UINT>		mapSepToShared;
	MakeSharedVertex(meshData->vertices, uniqueVertices, mapSepToShared);

	UINT numV	= static_cast<UINT>(uniqueVertices.size());	
	UINT numF	= static_cast<UINT>(meshData->meshes[0].indicesQuad.size());

	// create OpenSubdiv CPU mesh data, optionally with uv coordinates	
	const int fvarwith = meshData->texcoords.empty() ? 0 : 2; 
	static int indices[2] = {0, 1};
	static int widths[2] = {1, 1};
	int const fvarcount    = fvarwith > 0 ? 2 : 0;
	int const *fvarindices = fvarwith > 0 ? indices : 0;
	int const *fvarwidths  = fvarwith > 0 ? widths  : 0;


	OpenSubdiv::HbrCatmarkSubdivision<OpenSubdiv::OsdVertex> _catmark;
	OpenSubdiv::HbrMesh<OpenSubdiv::OsdVertex> *hmesh = new OpenSubdiv::HbrMesh<OpenSubdiv::OsdVertex>(&_catmark,
																									   fvarcount,		// fvar = optional float variables 
																									   fvarindices,		// available per vertex
																									   fvarwidths,		// propagated down the subd 
																									   fvarwith);		// hierarchy 


	// TODO create OBB	
	std::vector<XMUINT4> uniqueFaceIndices;
	uniqueFaceIndices.reserve(numF);
	for(auto origIndices : meshData->meshes[0].indicesQuad)
	{
		XMUINT4 sharedIndices;		
		sharedIndices.x =  mapSepToShared[origIndices.x];
		sharedIndices.y =  mapSepToShared[origIndices.y];
		sharedIndices.z =  mapSepToShared[origIndices.z];
		sharedIndices.w =  mapSepToShared[origIndices.w];

		uniqueFaceIndices.push_back(sharedIndices);
	}

	bool hasTexcoords = !meshData->texcoords.empty();


	// alloc vertex data
	for(UINT i = 0 ; i < numV; ++i)
	{		
		auto vtx = hmesh->NewVertex(i, OpenSubdiv::OsdVertex());
	}

	// create hbr faces 		
	for(UINT i = 0; i < numF; ++i)
	{
		XMUINT4& faceVertexIndices = uniqueFaceIndices[i]; 
		OpenSubdiv::HbrFace<OpenSubdiv::OsdVertex > *face = hmesh->NewFace(4, (int*)&faceVertexIndices.x,0);

		face->SetPtexIndex(i);		
	}

	hmesh->SetInterpolateBoundaryMethod(OpenSubdiv::HbrMesh<OpenSubdiv::OsdVertex>::k_InterpolateBoundaryEdgeOnly);
	//hmesh->SetInterpolateBoundaryMethod(OpenSubdiv::HbrMesh<OpenSubdiv::OsdVertex>::k_InterpolateBoundaryEdgeAndCorner);
	hmesh->Finish();

	// set face uvs
	if(fvarwith>0)
	{	
		const auto& uvData = meshData->texcoords;
		const auto& indexData = meshData->meshes[0].indicesQuad;
		for(UINT i = 0 ; i < numF; ++i)
		{
			OpenSubdiv::HbrFace<OpenSubdiv::OsdVertex > *f = hmesh->GetFace(i);
			OpenSubdiv::HbrHalfedge<OpenSubdiv::OsdVertex> * e = f->GetFirstEdge();

			const XMUINT4& vIdx = indexData[i];
			const int vIndices[4] = {vIdx.x, vIdx.y, vIdx.z, vIdx.w};
			for(int j = 0; j < 4; ++j, e=e->GetNext())
			{

				OpenSubdiv::HbrFVarData<OpenSubdiv::OsdVertex> & fvt = e->GetOrgVertex()->GetFVarData(f);
				XMFLOAT2 uvCoord = XMFLOAT2(uvData[vIndices[j]].x, uvData[vIndices[j]].y);
				const float *fvdata = &uvCoord.x;

				if(!fvt.IsInitialized())
				{
					fvt.SetAllData(2, fvdata);
				}
				else if(! fvt.CompareAll(2, fvdata))
				{
					OpenSubdiv::HbrFVarData<OpenSubdiv::OsdVertex> & nfvt = e->GetOrgVertex()->NewFVarData(f);
					nfvt.SetAllData(2, fvdata);
				}
			}
		}
	}


	std::cout << "numCoarseFaces: " << hmesh->GetNumCoarseFaces() << std::endl;


	// Tile Overlap Updater: Regular Case requires neighboring
	// tile IDs and edge on that neighboring tile
	// that is one int4 to store neighbors to each edge in a tile,
	// edges are ordered by ordered by winding (ccw in our case)
	// further we need one int4 to store which edge on neighbor is connected to 
	// current tiles edges 
		
	std::vector<SPtexNeighborData>& ptexNeighborData = model->GetPtexNeighborDataREF();
	ptexNeighborData.clear();
	ptexNeighborData.resize(hmesh->GetNumCoarseFaces());

	UINT cntExtraordinaryVertices = 0;
	UINT cntFacesWithExtraordinaryVertices = 0;
	for(int i = 0; i <  hmesh->GetNumCoarseFaces(); ++i)
	{
		auto face = hmesh->GetFace(i);
		auto ownPtexID = face->GetPtexIndex();

		// get neighbors
		auto startEdge = face->GetFirstEdge();
		//std::cout << "PTex IDs: self: " << ownPtexID << std::endl;

		bool faceHasExtrardinary  = false;
		for(int vID = 0; vID < face->GetNumVertices(); ++vID)
		{
			if(face->GetVertex(vID)->IsExtraordinary())
			{
				//std::cout << "face " << i << " has extraordinary vertex at id: " << vID << std::endl;
				faceHasExtrardinary  = true;
				cntExtraordinaryVertices++;
			}
		}
		if(faceHasExtrardinary)
			++cntFacesWithExtraordinaryVertices;


		int eIdx = 0;
		auto currEdge = startEdge;		
		do{

			if(currEdge->IsBoundary())
			{
				//std::cout << "boundary" << std::endl;
				ptexNeighborData[ownPtexID].ptexIDNeighbor[eIdx] = -1;
				ptexNeighborData[ownPtexID].neighEdgeID[eIdx] = -1;
			}
			else
			{
				auto neighEdge = currEdge->GetOpposite();
				auto neighFace = neighEdge->GetFace();
				auto neighPtexID = neighFace->GetPtexIndex();
				auto neighStartEdge = neighFace->GetFirstEdge();

				UINT nEdgeIdx = 0;
				auto currNeighEdge = neighStartEdge;	
				do{

					if(!currNeighEdge->IsBoundary() && currNeighEdge->GetOpposite()->GetFace()->GetPtexIndex() == ownPtexID)
					{
						break;
					}

					nEdgeIdx++;
					currNeighEdge = currNeighEdge->GetPrev();
				} while(currNeighEdge != neighStartEdge);


				//std::cout << "e" << eIdx << " neigh id: "<< neighPtexID << " we are neighEdgeID " << nEdgeIdx << std::endl;

				ptexNeighborData[ownPtexID].ptexIDNeighbor[eIdx] = neighPtexID;
				ptexNeighborData[ownPtexID].neighEdgeID[eIdx] = nEdgeIdx;			
			}

			eIdx++;
			currEdge = currEdge->GetPrev();		// WRONG? use getPrev for counter clockwise  ordering
		} while (currEdge != startEdge);		
	}



#if 0
	std::vector<PtexNeighborData>& ptexNeighborDataCheck = model->GetPtexNeighborDataREF();
	for(int ptxID = 0; ptxID < 5; ++ptxID)
	{
		for(int e = 0; e < 4; ++e)
		{
			auto neighPtexID = ptexNeighborDataCheck[ptxID].ptexIDNeighbor[e];
			auto nEdgeIdx = ptexNeighborDataCheck[ptxID].neighEdgeID[e];
			std::cout << "e" << e << " neigh id: "<< neighPtexID << " we are neighEdgeID " << nEdgeIdx << std::endl;
		}
	}
	//	std::cout << "face: " << i << " ptex ID: " << hmesh->GetFace(i)->GetPtexIndex() << "\t\t";
#endif

	UINT numFaces = hmesh->GetNumFaces();
	std::cout << "num Faces: " << numFaces << std::endl;
	std::cout << "numFaces with extraordinary Vertices: " << cntFacesWithExtraordinaryVertices << " numExtraordinary Vertices: " << cntExtraordinaryVertices << std::endl;

	//for(int i = 0; i < hmesh->GetNumCoarseFaces(); ++i)
	//	std::cout << "face: " << i << " ptex ID: " << hmesh->GetFace(i)->GetPtexIndex() << "\t\t";

	// Tile Overlap Updater Data:	
	// for each extraordinary vertex extract neighboring face IDs
	// and the next adjacent edge on that neighboring face edge
	// to later exqualize the tile data around an extraordinary vertex
	{
		std::vector<SExtraordinaryInfo>& eInfo = model->GetExtraordinaryInfoCPURef();
		eInfo.clear();

		std::vector<SExtraordinaryData>& eData = model->GetExtraordinaryDataCPURef();
		eData.clear();

		UINT cntExtraordinary = 0;
		UINT cntValences = 0;

		UINT numVertices = hmesh->GetNumVertices();
		/// TODO BOUNDARY CASES
		for (unsigned int i = 0; i < numVertices; ++i)
		{
			auto v = hmesh->GetVertex(i);

			if(v->IsExtraordinary())
			{		
				cntExtraordinary++;
				int valence =  v->GetValence();

				cntValences+= valence;

				SExtraordinaryInfo info;
				info.valence = valence;
				info.startIndex = static_cast<UINT>(eData.size());  // start index in data array // checkme here or in shader * 2 
				eInfo.push_back(info);


				// traverse around vertex to get ptex face ids and vertex in face number

				auto startEdge = v->GetIncidentEdge();

				auto currEdge = startEdge;	

				int cnt = 0;
				do 
				{
					// quick fix, does not work 
					if(currEdge->IsBoundary())
						break;
					auto vID    = currEdge->GetVertex()->GetID();
					auto face   = currEdge->GetLeftFace();
					auto ptexID = face->GetPtexIndex();					

					// vertex 0,1,2,3 in face 					
					for(int vNumber = 0; vNumber < face->GetNumVertices(); ++vNumber)
					{					
						if(vID == face->GetVertexID(vNumber))
						{
							SExtraordinaryData data;
							data.ptexFaceID = ptexID;
							data.vertexInFace = vNumber;
							eData.push_back(data);
							break;
							//std::cout << "im vertex number " << vNumber << " in face " << face->GetID() << std::endl;
						}
					}
					cnt++;
					if(vID  != startEdge->GetVertex()->GetID())
					{
						std::cerr << "ERROR" << std::endl;
						exit(1);
					}

					//currEdge = currEdge->GetPrev()->GetOpposite(); // checkme ccw
					currEdge = currEdge->GetOpposite()->GetNext(); // checkme ccw
				} while (currEdge != startEdge);
			}
		}

		model->SetNumExtraordinary(static_cast<UINT>(eInfo.size()));

		std::cout << "numExtraordinary " << eInfo.size() << "should be: " << cntExtraordinary << std::endl;
		std::cout << "numDataEntries " << eData.size() << "should match sumValences: " << cntValences << std::endl;
		std::cout << "num vertices in mesh: " << hmesh->GetNumVertices() << " unique vertices from loader: " << uniqueVertices.size() << std::endl;
	}

	model->SetMaterial(meshData->meshes[0].material);
	model->SetName(meshData->name);

	V(model->Create(DXUTGetD3D11Device(),hmesh, uniqueVertices, hasTexcoords, isDeformable));
	
	delete hmesh;

	return hr;
}


//void ParseCamera(const aiCamera* cam, Scene* scene)
//{
//	aiVector3D camPos = cam->mPosition;
//	std::cout << "camPos: " << camPos.x << ", " << camPos.y << ", " << camPos.z << std::endl;
//	
//}

HRESULT CreateMaterial(const aiMaterial* aimat, Scene* scene, DXMaterial*& mat) 
{
	HRESULT hr = S_OK;

	aiString aiMatName;
	if(aimat->Get(AI_MATKEY_NAME, aiMatName) != AI_SUCCESS)
	{
		std::cerr << "[ERROR] material has no name" << std::endl;
		aiMatName = "default1337";
	}

	// check if material is cached
	if(scene->GetMaterials().IsCached(aiMatName.C_Str()))
	{
		//std::cout << "use cached material" << std::endl;
		mat = scene->GetMaterials().GetCachedMaterial(aiMatName.C_Str());
		return S_OK;
	}
	else
	{
		std::string displacementTileFile = "";
		mat = new DXMaterial();
		mat->_name = aiMatName.C_Str();

		aiColor3D col;
		if(aimat->Get(AI_MATKEY_COLOR_AMBIENT, col) == AI_SUCCESS)
		{
			//std::cout << "amb: " << mat->_kDiffuse.x << ", " << mat->_kDiffuse.y << ", " << mat->_kDiffuse.z << std::endl;
			mat->_kAmbient = XMFLOAT3A(&col[0]);				
		}

		if(aimat->Get(AI_MATKEY_COLOR_DIFFUSE, col) == AI_SUCCESS)
		{
			mat->_kDiffuse = XMFLOAT3A(col.r, col.g, col.b);
			//std::cout << mat->_kDiffuse.x << ", " << mat->_kDiffuse.y << ", " << mat->_kDiffuse.z << std::endl;
		}

		if(aimat->Get(AI_MATKEY_COLOR_SPECULAR, col) == AI_SUCCESS)
		{
			mat->_kSpecular = XMFLOAT3A(&col[0]);
		}

		//used for special material flag like snow
		if(aimat->Get(AI_MATKEY_COLOR_EMISSIVE, col) == AI_SUCCESS)
		{
			//std::cout << "emmisive col: " << col.r << ", " << col.g  << ", " << col.b << std::endl;

			mat->_specialMaterialFlag = col.r;	
		}

		float shininess;
		if(aimat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
			mat->_shininess = shininess;	

		float deformationHardness;
		if(aimat->Get(AI_MATKEY_REFRACTI, deformationHardness) == AI_SUCCESS)
		{
			float hardness = deformationHardness - 1;
            // set default value for non tagged meshes
			if(hardness < 0.001)
				hardness = 0.25;

			mat->_smoothness = hardness;	
			//std::cout << "hardness: " << mat->_smoothness << std::endl;

		}



		aiString aiTexPath;
		if(aimat->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS)
		{
			std::string texPath = gCurrModelPath;
			texPath.append(std::string(aiTexPath.C_Str()));
			mat->SetDiffuseSRV(g_textureManager.AddTexture(texPath));

			//std::cout << "has diffuse texture" <<texPath << std::endl;			
		}
		else
		{
			// always add a default texture
			// mat->SetDiffuseSRV(g_textureManager.AddTexture("../../media/models/white.dds"));
		}

		if(aimat->GetTexture(aiTextureType_NORMALS, 0, &aiTexPath) == AI_SUCCESS)
		{				
			std::string texPath = gCurrModelPath;
			texPath.append(std::string(aiTexPath.C_Str()));
			mat->SetNormalsSRV(g_textureManager.AddTexture(texPath));		
			//std::cout << "has normal texture" <<texPath << std::endl;			
		}

		if(aimat->GetTexture(aiTextureType_SPECULAR, 0, &aiTexPath) == AI_SUCCESS)
		{
			std::string texPath = gCurrModelPath;
			texPath.append(std::string(aiTexPath.C_Str()));
			mat->SetSpecularTexSRV(g_textureManager.AddTexture(texPath));

			//std::cout << "has specular texture" <<texPath << std::endl;			
		}

		if(aimat->GetTexture(aiTextureType_OPACITY, 0, &aiTexPath) == AI_SUCCESS)
		{
			std::string texPath = gCurrModelPath;
			texPath.append(std::string(aiTexPath.C_Str()));
			mat->SetAlphaTexSRV(g_textureManager.AddTexture(texPath));

			//std::cout << "has alpha texture" <<texPath << std::endl;			
		}

		if(aimat->GetTexture(aiTextureType_DISPLACEMENT, 0, &aiTexPath) == AI_SUCCESS)
		{
			//std::cout << "has displacement texture" << std::endl;
			std::string texPath = gCurrModelPath;
			texPath.append(std::string(aiTexPath.C_Str()));
			mat->SetDisplacementSRV(g_textureManager.AddTexture(texPath));
			displacementTileFile = texPath;
		}

		if(aimat->GetTexture(aiTextureType_HEIGHT, 0, &aiTexPath) == AI_SUCCESS)
		{
			std::string texPath = gCurrModelPath;
			texPath.append(std::string(aiTexPath.C_Str()));
			mat->SetDisplacementSRV(g_textureManager.AddTexture(texPath));		

			displacementTileFile = texPath;
			//std::cout << "has displacement texture (disguised as aiTextureType_HEIGHT) " << std::endl;
		}

		if(aimat->GetTexture(aiTextureType_EMISSIVE, 0, &aiTexPath) == AI_SUCCESS)
		{			
			//std::cout << "has displacement texture (we use aiTextureType_EMISSIVE) " << std::endl;
			std::string texPath = gCurrModelPath;
			texPath.append(std::string(aiTexPath.C_Str()));
			mat->SetDisplacementSRV(g_textureManager.AddTexture(texPath));			
			displacementTileFile = texPath;
		}


		displacementTileFile = ReplaceFileExtension(displacementTileFile, ".disp");  
		WIN32_FIND_DATA findDisp;
		HANDLE hFind = FindFirstFile(s2ws(displacementTileFile).c_str(),&findDisp);

		if(hFind != INVALID_HANDLE_VALUE)
		{
			std::cerr << "found disp" << displacementTileFile.c_str() << std::endl;
			mat->_dispTileFileName = displacementTileFile;
		}

		mat->initCB();

		scene->GetMaterials().AddMaterial(mat);
	}

	return hr;
}


void TraverseAndStoreAssimpNodeHierachy(const aiNode* assimpNode, Node*& node, UINT animationID, SkinningMeshAnimationManager* mgr) 
{
	// Handle root node
	if (node == NULL) 
	{
		XMMATRIX nodeTransformation(assimpNode->mTransformation[0]);
		node = new Node(std::string(assimpNode->mName.data));
		node->setTransformation(nodeTransformation);
		UINT boneID = mgr->findBoneID(*node);
		if(boneID != INDEX_NOT_FOUND)
			node->setBoneID(boneID);
		else
		{
			//exit(1);
		}
		node->setAnimationAndNodeAnimationIDs(animationID, mgr->findNodeAnimationID(animationID, *node));
	}

	// Traverse and store children
	for (UINT i = 0; i < assimpNode->mNumChildren; ++i) 
	{
		XMMATRIX nodeTransformation(assimpNode->mChildren[i]->mTransformation[0]);		
		Node*& newNode = node->addChild(new Node(std::string(assimpNode->mChildren[i]->mName.data)));
		newNode->setTransformation(nodeTransformation);
		UINT boneID = mgr->findBoneID(*newNode);
		if(boneID != INDEX_NOT_FOUND)
			newNode->setBoneID(boneID);
		else
		{
			//exit(1);
		}
		newNode->setAnimationAndNodeAnimationIDs(animationID, mgr->findNodeAnimationID(animationID, *newNode));
		TraverseAndStoreAssimpNodeHierachy(assimpNode->mChildren[i], newNode, animationID, mgr);
	}
}



static void PrintMeshes(const aiNode* node, const aiScene* scene,  int level)
{
	for(UINT  i = 0; i< node->mNumMeshes; ++i)
	{
		for(int j = 0 ; j < level; ++j)
		{
			std::cout << "*";
		}
		std::cout << scene->mMeshes[node->mMeshes[i]]->mName.C_Str() << std::endl;
	}
}
static void PrintAssimpHierarchy(const aiNode* node, const aiScene* scene, int level)
{
	for(int j = 0 ; j < level; ++j)
	{
		std::cout << "*";
	}
	std::cout << "numMeshes "<< node->mNumMeshes << std::endl; 

	PrintMeshes(node, scene, level);
	for(UINT i = 0; i < node->mNumChildren; ++i)
	{
		for(int j = 0 ; j < level; ++j)
		{
			std::cout << "*";
		}

		std::cout << node->mName.C_Str() << " numMeshes " << node->mNumMeshes << " children: " << node->mNumChildren <<std::endl;


		PrintAssimpHierarchy(node->mChildren[i],scene, (level+1));
	}
}


void GetMeshesInSubtree(const aiNode* node, std::vector<UINT>& meshIDs, const aiScene* aiscene)
{
	node->mTransformation[0];
	for(UINT i = 0; i < node->mNumMeshes; ++i)
	{
		const UINT meshID  = node->mMeshes[i];
		std::string meshName = aiscene->mMeshes[meshID]->mName.C_Str();
		if(StringContains(meshName, "phy"))
			continue;
		else
			meshIDs.push_back(meshID);
	}

	for(UINT i = 0; i < node->mNumChildren; ++i)
	{
		aiNode* childNode =  node->mChildren[i];
		if(StringContains(childNode->mName.C_Str(), "phy"))
			continue;
		else
			GetMeshesInSubtree(childNode, meshIDs, aiscene);
	}
}

static void TraverseAssimpNodeHierachy(const aiNode* assimpNode, std::map<UINT,const aiNode*>& meshIDtoNode, const std::string& fileName) 
{
	std::string nodeName = assimpNode->mName.C_Str();

	if(assimpNode->mNumMeshes > 1)
		std::cerr << "[WARNING] model node " << nodeName << " mNumMeshes > 1 !" << std::endl; 

	if(assimpNode->mNumMeshes > 0) 
	{
		const UINT meshID = assimpNode->mMeshes[0];
		meshIDtoNode[meshID] = assimpNode;
	}

	// Traverse children
	for (UINT i = 0; i < assimpNode->mNumChildren; ++i) 
	{
		TraverseAssimpNodeHierachy(assimpNode->mChildren[i],  meshIDtoNode, fileName);
	}
}



HRESULT ParseAnimation(const aiScene* aiscene, AnimationGroup* animGroup)
{
	HRESULT hr = S_OK;
	if (animGroup) 
	{		
		std::cout << "has " << aiscene->mNumAnimations << " animations " << std::endl;
		auto* skinningMgr =  animGroup->GetAnimationManager();
		skinningMgr->printBones();

		// Check animations and get all the node data.
		UINT numAnimations = aiscene->HasAnimations() ? aiscene->mNumAnimations : 0;
		fprintf(stderr, "\n----\n#Animations: %d \n----\n", numAnimations);
		skinningMgr->globalInverseTransformationRef()= XMMatrixInverse(NULL, XMMATRIX(aiscene->mRootNode->mTransformation[0]));

		UINT numExistingAnimations = (UINT)skinningMgr->GetAnimationsRef().size();
		skinningMgr->GetRootNodesRef().resize(skinningMgr->GetRootNodesRef().size() + numAnimations, NULL);
		skinningMgr->GetAnimationsRef().resize(skinningMgr->GetAnimationsRef().size() + numAnimations, NULL);

		// Load and store animation data
		for (UINT i = 0; i < numAnimations; ++i) 
		{		
			aiAnimation* assimpAnimation = aiscene->mAnimations[i];
			int animID = i + numExistingAnimations;
			skinningMgr->GetAnimationsRef()[animID] = new Animation();
			skinningMgr->GetAnimationsRef()[animID]->setNumChannels(assimpAnimation->mNumChannels);
			skinningMgr->GetAnimationsRef()[animID]->setDuration((float)assimpAnimation->mDuration);
			skinningMgr->GetAnimationsRef()[animID]->setTicksPerSecond((float)assimpAnimation->mTicksPerSecond);
			//std::cout << "anim duration: " << assimpAnimation->mDuration << ", ticks per second: " << assimpAnimation->mTicksPerSecond << ", numChannels: " << assimpAnimation->mNumChannels << std::endl;


			for (UINT j = 0; j < assimpAnimation->mNumChannels; ++j) {
				aiNodeAnim* assimpNodeAnimation = assimpAnimation->mChannels[j];				
				NodeAnimation*& nodeAnimation = skinningMgr->GetAnimationsRef()[animID]->getChannelRef(j);
				nodeAnimation = new NodeAnimation(std::string(assimpNodeAnimation->mNodeName.data));

				// rotation
				for (UINT k = 0; k < assimpNodeAnimation->mNumRotationKeys; ++k) 
				{
					const aiQuaternion& quat = assimpNodeAnimation->mRotationKeys[k].mValue;	// assimp quaternion layout w, x, y, z
					XMVECTOR R = XMVectorSet( quat.x, quat.y,	quat.z,	quat.w);
					nodeAnimation->AddRotation(RotationKey((float)assimpNodeAnimation->mRotationKeys[k].mTime, R));
				}

				// translation
				for (UINT k = 0; k < assimpNodeAnimation->mNumPositionKeys; ++k) 
				{					
					const aiVector3D& trans = assimpNodeAnimation->mPositionKeys[k].mValue;					
					XMVECTOR T = XMVectorSet(trans.x, trans.y, trans.z, 1);
					nodeAnimation->AddTranslation(TranslationKey((float)assimpNodeAnimation->mPositionKeys[k].mTime, T));				
				}

				// scaling
				for (UINT k = 0; k < assimpNodeAnimation->mNumScalingKeys; ++k) 
				{				
					XMVECTOR S = XMVectorSet(assimpNodeAnimation->mScalingKeys[k].mValue.x, assimpNodeAnimation->mScalingKeys[k].mValue.y, assimpNodeAnimation->mScalingKeys[k].mValue.z, 1);
					nodeAnimation->AddScaling(ScalingKey((float)assimpNodeAnimation->mScalingKeys[k].mTime, S));
				}

			}
		}

		// Traverse assimp node hierachy and copy it into the local datastructures
		//if(numExistingAnimations == 0)
		{
			for (UINT i = 0; i < numAnimations; ++i) 
			{
				int animID = i + numExistingAnimations;
				TraverseAndStoreAssimpNodeHierachy(aiscene->mRootNode, skinningMgr->GetRootNodesRef()[animID], animID, skinningMgr);
			}
		}
	}

	return hr;
}


// load non animated meshes
HRESULT ModelLoader::Load( std::string fileName, Scene* scene, AnimationGroup* animGroup, ModelGroup* loadToGroup)
{	
	HRESULT hr = S_OK;

	const struct aiScene* aiscene = NULL;
	unsigned int ppFlags = 	0			// import post-processing flags
		//| aiProcess_Triangulate
		//| aiProcess_GenSmoothNormals 
		| aiProcess_ValidateDataStructure 
		| aiProcess_FindInstances 
		| aiProcess_JoinIdenticalVertices
		//| aiProcess_Debone // BE CAREFUL WITH THIS.
		//| aiProcess_SortByPType
		//| aiProcess_PreTransformVertices
		//| aiProcess_ConvertToLeftHanded				// for directx 
		//| aiProcess_MakeLeftHanded
		//| aiProcess_FlipUVs
		//| aiProcess_FlipWindingOrder
		//| aiProcess_Debone
		//| aiProcess_GenUVCoords               // convert spherical, cylindrical, box and planar mapping to proper UVs
		//| aiProcess_TransformUVCoords         // preprocess UV transformations (scaling, translation ...)
		| aiProcess_LimitBoneWeights
		;

	aiPropertyStore* props = aiCreatePropertyStore();
	aiSetImportPropertyInteger(props,AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT ); // we dont want to load lines and points

    std::cout << "Load: " << fileName.c_str() << std::endl;
	aiscene = (aiScene*)aiImportFileExWithProperties(fileName.c_str(),ppFlags, NULL, props);
	aiReleasePropertyStore(props);

	if(!aiscene)
	{
		MessageBoxA(0, aiGetErrorString(), "ERROR", MB_OK);	
		return S_FALSE;
	}

	gCurrModelPath = ExtractPath(fileName).append("/");	


	// physics loader, check if physics component is available
	bool withPhysics = scene->GetPhysics() != NULL;	
#ifndef CHECKLEAKS
	btBulletWorldImporter* physicsLoader = withPhysics ? new btBulletWorldImporter(scene->GetPhysics()->GetDynamicsWorld()) :  NULL;

	//std::cout << physicsLoader << std::endl;

	std::string physicsFile = ReplaceFileExtension(fileName, ".bullet");
	withPhysics = withPhysics && physicsLoader->loadFile(physicsFile.c_str());
	if(!withPhysics) std::cerr << "cannot find " << physicsFile << std::endl;
#else
	withPhysics = false;
#endif

	std::cout << "num meshes in scene: " << aiscene->mNumMeshes << std::endl;
	//PrintAssimpHierarchy(aiscene->mRootNode, aiscene, 0);

	//std::map<UINT,const aiNode*> meshIDtoNode;
	//traverseAssimpNodeHierachy(aiscene->mRootNode,  meshIDtoNode, fileName);

	UINT numAnimations = aiscene->HasAnimations() ? aiscene->mNumAnimations : 0;
	fprintf(stderr, "\n----\n#Animations: %d \n----\n", numAnimations);

	std::unordered_map<std::string, int>  sceneCamNameToId;
	if(animGroup == NULL && loadToGroup == NULL && aiscene->HasCameras())
	{	
		for(UINT i = 0; i < aiscene->mNumCameras; ++i)
		{
			sceneCamNameToId[aiscene->mCameras[i]->mName.C_Str()] =  i;						
		}
	}

	bool parseGeometry = animGroup== NULL || (animGroup != NULL && animGroup->GetAnimationManager()->GetAnimationsRef().size() == 0);
	if( parseGeometry )
	{
		// multimesh - one animation manager, multiple vertex groups
		//std::cout << "numMeshes " << aiscene->mNumMeshes << std::endl;	
		//std::cout << " num Lights " << aiscene->mNumLights << std::endl;


		std::unordered_map<std::string, int> nameToIDTri;		// map assimp mesh name to our mesh	
		std::unordered_map<std::string, int> nameToIDQuad;		// map assimp mesh name to our mesh	

		struct MultiMesh
		{
			bool isQuadMesh;
			int meshID;
			int submeshID;
		};


		std::vector<MultiMesh> idToMesh(aiscene->mNumMeshes);
		std::vector<MeshData> meshDataTri;		// hold meshdata, create tri or subd meshes from that data later
		std::vector<MeshData> meshDataQuad;		// hold meshdata, create tri or subd meshes from that data later

		for(UINT i = 0; i < aiscene->mNumAnimations; ++i)
		{
			std::cout << "animation: " <<  aiscene->mAnimations[i]->mChannels[0]->mNodeName.C_Str() << std::endl;
		}





		// load geometry data, try to combine submeshes
		for(UINT i = 0; i < aiscene->mNumMeshes; ++i)
		{		
			const aiMesh* aimesh = aiscene->mMeshes[i];		
			const std::string name = aimesh->mName.C_Str();

			if(ToUpperCase(std::string(name)).find("PHY") != std::string::npos )
			{
				std::cout << "skip physics mesh" << std::endl;			
				continue;
			}

			UINT primitiveType = aimesh->mPrimitiveTypes;
			if(primitiveType != aiPrimitiveType_TRIANGLE && primitiveType != aiPrimitiveType_POLYGON)					
				continue;

			bool isQuadMesh = aimesh->mPrimitiveTypes == aiPrimitiveType_POLYGON;				
			auto& it = isQuadMesh ? nameToIDQuad.find(name) : nameToIDTri.find(name);		

			// always create new entry for subd models
			if(isQuadMesh)
			{	
				nameToIDQuad[aimesh->mName.C_Str()] = (int)meshDataQuad.size();

				idToMesh[i].meshID = (int)meshDataQuad.size();
				idToMesh[i].submeshID = 0;
				idToMesh[i].isQuadMesh = true;			
				MeshData mData;
				mData.name = name;
				meshDataQuad.push_back(mData);						
			}
			else if(!isQuadMesh && it == nameToIDTri.end())
			{
				nameToIDTri[aimesh->mName.C_Str()] = (int)meshDataTri.size();

				idToMesh[i].meshID = (int)meshDataTri.size();
				idToMesh[i].submeshID = 0;
				idToMesh[i].isQuadMesh = false;
				MeshData mData;
				mData.name = name;
				meshDataTri.push_back(mData);
			}
			else 
			{
				idToMesh[i].meshID = it->second;			
			}


			// fill mesh data		
			int meshID = idToMesh[i].meshID;		
			auto& mesh = isQuadMesh ? meshDataQuad[meshID] : meshDataTri[meshID];

			int submeshID = (int)mesh.meshes.size();
			idToMesh[i].submeshID = submeshID;



			UINT baseVertexIndex = mesh.numVertices;
			UINT numV = aimesh->mNumVertices;
			UINT numF = aimesh->mNumFaces;

			SubMeshData subdata;
			subdata.baseVertex = baseVertexIndex;
			subdata.numVertices = numV;		
			subdata.name = name;		
			mesh.numVertices += aimesh->mNumVertices;		
			mesh.vertices.resize(mesh.numVertices);

			for(UINT i = 0; i < numV; ++i)
			{
				const aiVector3D& v = aimesh->mVertices[i];
				mesh.vertices[baseVertexIndex+i] = XMFLOAT4A(v.x, v.y, v.z, 1.f);
			}

			// TODO check if existing mesh also has normals otherwise vertex,normals buffer dont match indices		
			if(aimesh->HasNormals())
			{			
				assert(submeshID ==0 || (mesh.normals.size() > 0 && submeshID > 0) );
				mesh.normals.resize(mesh.numVertices);
				for(UINT i = 0; i < numV; ++i)
				{
					const aiVector3D& n = aimesh->mNormals[i];
					mesh.normals[baseVertexIndex+i] = XMFLOAT3A(n.x, n.y, n.z);			
				}	
			}

			// TODO check if existing mesh also has texcoords otherwise vertex, texcoord buffers dont match indices		
			if( aimesh->HasTextureCoords(0) )
			{		
				assert(submeshID ==0 || (mesh.texcoords.size() > 0 && submeshID > 0));
				mesh.texcoords.resize(mesh.numVertices);			
				for(UINT i = 0; i < numV; ++i)
				{
					const aiVector3D& uv = aimesh->mTextureCoords[0][i];
					mesh.texcoords[baseVertexIndex+i] = XMFLOAT2A(uv.x, uv.y);
				}
			}


			if(isQuadMesh)
			{			
				subdata.indicesQuad.resize(numF);
				for(UINT i = 0; i < numF; ++i)
				{
					const aiFace& face = aimesh->mFaces[i];		
					subdata.indicesQuad[i] = XMUINT4(face.mIndices[0],face.mIndices[1] ,face.mIndices[2],face.mIndices[3]);		
				}

			}
			else
			{
				subdata.indicesTri.resize(numF);
				for(UINT i = 0; i < numF; ++i)
				{
					const aiFace& face = aimesh->mFaces[i];		
					subdata.indicesTri[i] = XMUINT3(face.mIndices[0] + baseVertexIndex,face.mIndices[1] + baseVertexIndex ,face.mIndices[2] + baseVertexIndex);		
				}			
			}

			// skinning
			if(animGroup != NULL && aimesh->HasBones())
			{
				mesh.withSkinning = true;
				//if(mesh.skinning == NULL)	mesh.skinning = new SkinningMeshAnimationManager();
				auto* skinningMgr = animGroup->GetAnimationManager();
				auto& boneData = skinningMgr->GetBoneDataRef();

				boneData.resize(mesh.numVertices);
				UINT numBones = aimesh->mNumBones;
				UINT boneID = 0;
				for(UINT j = 0; j < numBones; ++j)
				{
					const aiBone* bone = aimesh->mBones[j];
					const std::string boneName = bone->mName.C_Str();

					std::cout << "processing bone \" " << boneName << "\" "<< std::endl;				
					boneID = skinningMgr->addBone(boneName, XMMATRIX(bone->mOffsetMatrix[0]));
					for(UINT w = 0; w < bone->mNumWeights; ++w)
					{
						UINT vertexID = bone->mWeights[w].mVertexId + baseVertexIndex;
						boneData[vertexID].setBoneData(boneID, bone->mWeights[w].mWeight);
					}
				}
			}

			// material
			DXMaterial* mat = NULL;
			CreateMaterial(aiscene->mMaterials[aimesh->mMaterialIndex], scene, mat);
			subdata.material = mat;

			mesh.meshes.push_back(subdata);
		}


		// create triangle meshes
		std::vector<DXModel*> triModels;	
		for(auto& mesh : meshDataTri)
		{
			DXModel* model = new DXModel();		
			model->Create(DXUTGetD3D11Device(), &mesh);
			triModels.push_back(model);
			scene->AddModel(model);		
		}

		for(auto& mesh: meshDataQuad)
		{
			if(mesh.meshes.size() > 1)
				std::cerr << "subd models should never be compound meshes" << std::endl;
		}

		// now all models are created, next we traverse assimp hierarchy to find model instances and add the models to the scene
		std::vector<std::vector<const aiNode*>> meshToSubmesh(aiscene->mNumMeshes);

		UINT countGroups = 0;
		std::vector<ModelInstance*>& globalIDToMesh = scene->GetGlobalIDToInstanceMap();
		for(UINT i = 0; i < aiscene->mRootNode->mNumChildren; ++i)
		{
			const aiNode*  node = aiscene->mRootNode->mChildren[i];

			// parse scene cam positions
			if(animGroup == NULL && loadToGroup == NULL && aiscene->HasCameras())
			{			
				auto it = sceneCamNameToId.find(node->mName.C_Str());
				if(it != sceneCamNameToId.end())
				{
					const aiCamera* cam = aiscene->mCameras[it->second];
					aiVector3D camPos = node->mTransformation * cam->mPosition;

					scene->AddStadiumCamPosition(XMFLOAT3(camPos.x, camPos.y, camPos.z));			
				}
			}

			if(node->mNumMeshes == 0) continue;
			std::cout << "num meshes: " << node->mNumMeshes << std::endl;



			const char*	   name = node->mName.C_Str();
			//std::cout << "name: " << name << std::endl;
			if(ToUpperCase(std::string(name)).find("PHY") != std::string::npos )
			{
				std::cout << "skip phy node: " << name << std::endl;
				continue;
			}



			bool hasDeformables = ToUpperCase(std::string(name)).find("DEFORMABLE") != std::string::npos;
			//hasDeformables = false; // DEBUG

			//if(hasDeformables) std::cout << "has deformable " << std::endl;
			ModelGroup* group = NULL;
			if(animGroup)
				group = animGroup;
			else if(loadToGroup)
				group = loadToGroup;
			else
				group = new ModelGroup();

			group->SetName(name);		
			group->SetHasDeformables(hasDeformables);	
			countGroups +=1;
			std::vector<UINT> meshIDs;
			GetMeshesInSubtree(node, meshIDs, aiscene);
			//std::cout << "num meshIDs for group: " << name << ": " << meshIDs.size() << std::endl;

			std::set<int> uniqueTri;		
#if 0
			for(auto meshID : meshIDs) 	// iterate over all submeshes in assimp group node
			{			
				auto model = idToMesh[meshID];

				if(model.isQuadMesh)
				{
					std::cout << "IS QUAD MESH" << std::endl;
					// always create subd mesh instance
					const auto& data = meshDataQuad[model.meshID];
					assert(data.meshes.size() == 1);

					DXOSDMesh* subdModel = new DXOSDMesh();
					V_RETURN(CreateOSDModel(&data, subdModel, hasDeformables));
					subdModel->SetMaterial(data.meshes[0].material);
					group->osdModels.push_back(subdModel);	



					ModelInstance* instance = new ModelInstance(subdModel);
					instance->SetIsDeformable(hasDeformables);
		
					instance->EnableDynamicDisplacement();
					instance->EnableDynamicColor();

					bool isCollider = ToUpperCase(std::string(subdModel->GetName())).find("COLLIDER") != std::string::npos;
					instance->SetIsCollider(isCollider);
					if(isCollider)
						V_RETURN(instance->Create(DXUTGetD3D11Device()));

					instance->SetGlobalInstanceID((UINT)globalIDToMesh.size());
					globalIDToMesh.push_back(instance);

					group->modelInstances.push_back(instance);

					scene->AddModel(subdModel);
				}
				else			
				{
					if(uniqueTri.find(model.meshID) == uniqueTri.end())
					{
						DXModel* triModel = triModels[model.meshID];

						for(auto& submesh : triModel->submeshes)
						{					
							ModelInstance* instance = new ModelInstance(triModel, &submesh);
							bool isCollider = ToUpperCase(std::string(submesh.GetName())).find("COLLIDER") != std::string::npos;						
							instance->SetIsCollider(isCollider);
							if(isCollider)
								V_RETURN(instance->Create(DXUTGetD3D11Device()));	

							instance->SetGlobalInstanceID((UINT)globalIDToMesh.size());
							globalIDToMesh.push_back(instance);

							group->modelInstances.push_back(instance);

						}

						group->triModels.push_back(triModels[model.meshID]);						
					}
				}
			}
#else
			for(auto meshID : meshIDs) 	// iterate over all submeshes in assimp group node
			{			
				auto model = idToMesh[meshID];

				if(model.isQuadMesh)
				{
					//std::cout << "IS QUAD MESH" << std::endl;
					// always create subd mesh instance
					const auto& data = meshDataQuad[model.meshID];
					assert(data.meshes.size() == 1);

					DXOSDMesh* subdModel = new DXOSDMesh();
					V_RETURN(CreateOSDModel(&data, subdModel, hasDeformables));
					subdModel->SetMaterial(data.meshes[0].material);
					group->osdModels.push_back(subdModel);	
					
					ModelInstance* instance = new ModelInstance(subdModel);
					instance->SetIsDeformable(hasDeformables);

					instance->EnableDynamicDisplacement();
					instance->EnableDynamicColor();

					bool isCollider = ToUpperCase(std::string(subdModel->GetName())).find("COLLIDER") != std::string::npos;					
					if(isCollider || hasDeformables)
					{
						//
						std::cout << "WARN WARN" << std::endl;
						//MessageBoxA(0, "FIX ME", "FIX", MB_ICONERROR|MB_OK);
						std::cout << "Deine mudda fixt das..." << std::endl;
						V_RETURN(instance->Create(DXUTGetD3D11Device()));						
						instance->SetIsCollider(isCollider); // CHECKME if deformable
					}

					bool isTool = ToUpperCase(std::string(subdModel->GetName())).find("TOOL") != std::string::npos;					
					instance->SetIsTool(isTool);					

					instance->SetGlobalInstanceID((UINT)globalIDToMesh.size());
					globalIDToMesh.push_back(instance);

					instance->SetGroup(group);
					group->modelInstances.push_back(instance);

					scene->AddModel(subdModel);
				}
			}

			std::set<int> group_triModels;
			std::map<int,std::vector<int> > group_triModelSubmeshes;

			for(auto meshID : meshIDs) 	
			{
				auto model = idToMesh[meshID];
				if(!model.isQuadMesh)
				{
					group_triModels.insert(model.meshID);
					group_triModelSubmeshes[model.meshID].push_back(model.submeshID);					
				}
			}

			for(auto m : group_triModels)
			{
				DXModel* triModel = triModels[m];

				for(auto smID : group_triModelSubmeshes[m])
				{
					auto submesh = &triModel->submeshes[smID];

					ModelInstance* instance = new ModelInstance(triModel, submesh);
					bool isCollider = ToUpperCase(std::string(submesh->GetName())).find("COLLIDER") != std::string::npos;						
					instance->SetIsCollider(isCollider);
					if(isCollider)
						V_RETURN(instance->Create(DXUTGetD3D11Device()));

					bool isTool = ToUpperCase(std::string(submesh->GetName())).find("TOOL") != std::string::npos;					
					instance->SetIsTool(isTool);	

					instance->SetGlobalInstanceID((UINT)globalIDToMesh.size());
					globalIDToMesh.push_back(instance);

					instance->SetGroup(group);
					group->modelInstances.push_back(instance);					
				}
				group->triModels.push_back(triModels[m]);		



				//DXModel* triModel = triModels[model.meshID];

				//for(auto& submesh : triModel->submeshes)
				//{					
				//	ModelInstance* instance = new ModelInstance(triModel, &submesh);
				//	bool isCollider = ToUpperCase(std::string(submesh.GetName())).find("COLLIDER") != std::string::npos;						
				//	instance->SetIsCollider(isCollider);
				//	if(isCollider)
				//		V_RETURN(instance->Create(DXUTGetD3D11Device()));	

				//	instance->SetGlobalInstanceID((UINT)globalIDToMesh.size());
				//	globalIDToMesh.push_back(instance);

				//	group->modelInstances.push_back(instance);

				//}

				//group->triModels.push_back(triModels[model.meshID]);	
			}
#endif			


			
			//for(auto meshID : meshIDs) 	// iterate over all submeshes in assimp group node
			//{			
			//	auto model = idToMesh[meshID];
			//	
			//	if(!model.isQuadMesh)				
			//	{
			//		DXModel* triModel = triModels[model.meshID];

			//		auto submesh = triModel->submeshes[model.submeshID];
			//							
			//		ModelInstance* instance = new ModelInstance(triModel, &submesh);
			//		bool isCollider = ToUpperCase(std::string(submesh.GetName())).find("COLLIDER") != std::string::npos;						
			//		instance->SetIsCollider(isCollider);
			//		if(isCollider)
			//			V_RETURN(instance->Create(DXUTGetD3D11Device()));	

			//		instance->SetGlobalInstanceID((UINT)globalIDToMesh.size());
			//		globalIDToMesh.push_back(instance);

			//		group->modelInstances.push_back(instance);					
			//	}
			//}
			//group->triModels.push_back(triModels[model.meshID]);	

			//std::cout << "NUM GROUPS " << countGroups << std::endl;



			XMMATRIX  modelMatrix = XMMatrixTranspose(DirectX::XMMATRIX(node->mTransformation[0]));	
			//XMMATRIX flipX = XMMATRIX(  1,0,0,0,
			//	0,1,0,0,
			//	0,0,1,0,
			//	0,0,0,1);

			////modelMatrix =switchYZ* modelMatrix * switchYZ;
			////modelMatrix =  flipX * modelMatrix * flipX;
#ifndef CHECKLEAKS				
			btRigidBody* phyBody = withPhysics ? physicsLoader->getRigidBodyByName(name) : NULL;
			if(phyBody)
			{				
				//std::cout << "have phybody for " << name << std::endl;
				//if(group->HasDeformables())
				//	phyBody->getCollisionShape()->gets
				// if object has a physics representation bullet handles orientation and transformation
				// hence we are only interested in initial scale		
				XMVECTOR dQrot;
				XMVECTOR dTrans;
				XMVECTOR dScale;
				XMMatrixDecompose(&dScale, &dQrot, &dTrans, modelMatrix);

				// CHECKME ugly hack to make phy objects smaller
				// if(loadToGroup == NULL) for frog
				bool doScale = false;
				if(loadToGroup == NULL)
				{						
					for(auto gm : group->modelInstances)
					{
						if(gm->IsCollider())
						{
							//dScale += XMVectorSet(0.3f, 0.3f, 0.3f,0);
							doScale = true;
							break;
						}
					}
				}

				// end hack
				group->SetPhysicsScaleMatrix(XMMatrixScalingFromVector(dScale) );
				group->SetModelMatrix(XMMatrixIdentity() );
				group->SetPhysicsObject(phyBody);

				if (doScale) {
					// phybody has to be set before calling this function
					group->SetPhysicsScalingFactor(0.95f);
					//group->SetPhysicsScalingFactor(0.8f);
				}

				phyBody->setUserPointer((void*)group);

				if(phyBody->getMotionState() == NULL)
					phyBody->setMotionState(new btDefaultMotionState(phyBody->getWorldTransform()));			
			}
			else
			{			
				group->SetModelMatrix( modelMatrix );
			}
#else
			group->SetModelMatrix( modelMatrix );
#endif
			if(!animGroup && !loadToGroup)
			{			
				scene->AddGroup(group);						
			}			

		}
	}

	ParseAnimation(aiscene, animGroup);


#ifndef CHECKLEAKS
	delete physicsLoader; 
#endif

	aiReleaseImport(aiscene);

	return hr;
}

//HRESULT ModelLoader::Load2( std::string fileName, Scene* scene )
//{	
//	HRESULT hr = S_OK;
//
//	Assimp::Importer importer;
//	const struct aiScene* aiscene = NULL;
//	aiscene = importer.ReadFile(fileName.c_str(),   0	// import post-processing flags
//		//| aiProcess_Triangulate
//		//| aiProcess_GenSmoothNormals 
//		| aiProcess_JoinIdenticalVertices 
//		//| aiProcess_SortByPType
//		//| aiProcess_PreTransformVertices
//		//| aiProcess_ConvertToLeftHanded				// for directx 
//		//| aiProcess_Debone
//		  | aiProcess_LimitBoneWeights
//		);
//
//	if(!aiscene)
//	{
//		MessageBoxA(0, importer.GetErrorString(), "ERROR", MB_OK);
//		return S_FALSE;
//	}
//
//	gCurrModelPath = ExtractPath(fileName).append("/");	
//
//	// physics loader, check if physics component is available
//	bool withPhysics = scene->GetPhysics() == NULL;	
//	btBulletWorldImporter* physicsLoader = withPhysics ? new btBulletWorldImporter(scene->GetPhysics()->GetDynamicsWorld()) :  NULL;
//
//
//	std::string physicsFile = ReplaceFileExtension(fileName, ".bullet");
//	withPhysics = physicsLoader->loadFile(physicsFile.c_str());
//	if(!withPhysics) std::cerr << "cannot find " << physicsFile << std::endl;
//
//
//	std::cout << "num meshes in scene: " << aiscene->mNumMeshes << std::endl;
//	printHierarchy(aiscene->mRootNode, aiscene, 0);
//
//	//std::map<UINT,const aiNode*> meshIDtoNode;
//	//traverseAssimpNodeHierachy(aiscene->mRootNode,  meshIDtoNode, fileName);
//
//	UINT numAnimations = aiscene->HasAnimations() ? aiscene->mNumAnimations : 0;
//	fprintf(stderr, "\n----\n#Animations: %d \n----\n", numAnimations);
//	
//
//	// todo reuse existing manager
//	//SkinningMeshAnimationManager* skinningMgr = numAnimations != 0 ? new SkinningMeshAnimationManager() : NULL;
//
//	//UINT baseVertexIndex = 0;
//
//	// multimesh - one animation manager, multiple vertex groups
//	std::cout << "numMeshes " << aiscene->mNumMeshes << std::endl;	
//
//	std::cout << " num Lights " << aiscene->mNumLights << std::endl;
//
//
//	std::unordered_map<std::string, int> nameToIDTri;		// map assimp mesh name to our mesh	
//	std::unordered_map<std::string, int> nameToIDQuad;		// map assimp mesh name to our mesh	
//	
//	struct MultiMesh
//	{
//		bool isQuadMesh;
//		int meshID;
//		int submeshID;
//	};
//
//	std::vector<MultiMesh> idToMesh(aiscene->mNumMeshes);
//	std::vector<MeshData> meshDataTri;		// hold meshdata, create tri or subd meshes from that data later
//	std::vector<MeshData> meshDataQuad;		// hold meshdata, create tri or subd meshes from that data later
//	
//	for(UINT i =0; i < aiscene->mNumAnimations; ++i)
//	{
//		 std::cout << "animation: " <<  aiscene->mAnimations[i]->mChannels[0]->mNodeName.C_Str() << std::endl;
//	}
//
//	
//
//	// load geometry data, try to combine submeshes
//	for(UINT i = 0; i < aiscene->mNumMeshes; ++i)
//	{		
//		const aiMesh* aimesh = aiscene->mMeshes[i];		
//		const std::string name = aimesh->mName.C_Str();
//
//		UINT primitiveType = aimesh->mPrimitiveTypes;
//		if(!(primitiveType == aiPrimitiveType_TRIANGLE || primitiveType == aiPrimitiveType_POLYGON))					
//			continue;
//		
//
//
//		bool isQuadMesh = aimesh->mPrimitiveTypes == aiPrimitiveType_POLYGON;				
//		auto& it = isQuadMesh ? nameToIDQuad.find(name) : nameToIDTri.find(name);		
//
//		if(isQuadMesh && it == nameToIDQuad.end())
//		{	
//			nameToIDQuad[aimesh->mName.C_Str()] = meshDataQuad.size();
//						
//			idToMesh[i].meshID = meshDataQuad.size();
//			idToMesh[i].submeshID = 0;
//			idToMesh[i].isQuadMesh = true;
//						
//			meshDataQuad.push_back(MeshData());						
//		}
//		else if(!isQuadMesh && it == nameToIDTri.end())
//		{
//			nameToIDTri[aimesh->mName.C_Str()] = meshDataTri.size();
//
//			idToMesh[i].meshID = meshDataTri.size();
//			idToMesh[i].submeshID = 0;
//			idToMesh[i].isQuadMesh = false;
//
//			meshDataTri.push_back(MeshData());
//		}
//		else 
//		{
//			idToMesh[i].meshID = it->second;			
//		}
//
//
//		// fill mesh data		
//		int meshID = idToMesh[i].meshID;		
//		auto& mesh = isQuadMesh ? meshDataQuad[meshID] : meshDataTri[meshID];
//
//		int submeshID = mesh.meshes.size();
//		idToMesh[i].submeshID = submeshID;
//
//
//		UINT baseVertexIndex = mesh.numVertices;	
//		SubMeshData subdata;
//		subdata.baseVertex = baseVertexIndex;
//
//		UINT numV = aimesh->mNumVertices;
//		UINT numF = aimesh->mNumFaces;
//
//		mesh.numVertices += aimesh->mNumVertices;
//		mesh.vertices.resize(mesh.numVertices);
//						
//		for(UINT i = 0; i < numV; ++i)
//		{
//			const aiVector3D& v = aimesh->mVertices[i];
//			mesh.vertices[baseVertexIndex+i] = XMFLOAT4A(v.x, v.y, v.z, 1.f);
//		}
//
//		// TODO check if existing mesh also has normals otherwise vertex,normals buffer dont match indices		
//		if(aimesh->HasNormals())
//		{
//			assert(mesh.normals.size() > 0 && submeshID > 0);
//			mesh.normals.resize(mesh.numVertices);
//			for(UINT i = 0; i < numV; ++i)
//			{
//				const aiVector3D& n = aimesh->mNormals[i];
//				mesh.normals[baseVertexIndex+i] = XMFLOAT3A(n.x, n.y, n.z);			
//			}	
//		}
//
//		// TODO check if existing mesh also has texcoords otherwise vertex, texcoord buffers dont match indices		
//		if( aimesh->HasTextureCoords(0) )
//		{
//			assert(mesh.texcoords.size() > 0 && submeshID > 0);
//			mesh.texcoords.resize(mesh.numVertices);			
//			for(UINT i = 0; i < numV; ++i)
//			{
//				const aiVector3D& uv = aimesh->mTextureCoords[0][i];
//				mesh.texcoords[baseVertexIndex+i] = XMFLOAT2A(uv.x, uv.y);
//			}
//		}
//
//
//		if(isQuadMesh)
//		{			
//			subdata.indicesQuad.resize(numF);
//			for(UINT i = 0; i < numF; ++i)
//			{
//				const aiFace& face = aimesh->mFaces[i];		
//				subdata.indicesQuad[i] = XMUINT4(face.mIndices[0] + baseVertexIndex,face.mIndices[1] + baseVertexIndex ,face.mIndices[2] + baseVertexIndex,face.mIndices[3] + baseVertexIndex);		
//			}
//			
//		}
//		else
//		{
//			subdata.indicesTri.resize(numF);
//			for(UINT i = 0; i < numF; ++i)
//			{
//				const aiFace& face = aimesh->mFaces[i];		
//				subdata.indicesTri[i] = XMUINT3(face.mIndices[0] + baseVertexIndex,face.mIndices[1] + baseVertexIndex ,face.mIndices[2] + baseVertexIndex);		
//			}			
//		}
//
//		// skinning
//		if(aimesh->HasBones())
//		{
//			if(mesh.skinning == NULL)	mesh.skinning = new SkinningMeshAnimationManager();
//			auto* skinningMgr = mesh.skinning;
//			auto& boneData = skinningMgr->getBoneDataRef();
//
//			boneData.resize(mesh.numVertices);
//			UINT numBones = aimesh->mNumBones;
//			UINT boneID = 0;
//			for(UINT j = 0; j < numBones; ++j)
//			{
//				const aiBone* bone = aimesh->mBones[j];
//				const std::string boneName = bone->mName.C_Str();
//
//				std::cout << "processing bone \" " << boneName << "\" "<< std::endl;				
//				boneID = skinningMgr->addBone(boneName, XMMATRIX(bone->mOffsetMatrix[0]));
//				for(UINT w = 0; w < bone->mNumWeights; ++w)
//				{
//					UINT vertexID = bone->mWeights[w].mVertexId + baseVertexIndex;
//					boneData[vertexID].setBoneData(boneID, bone->mWeights[w].mWeight);
//				}
//			}
//		}
//
//		// material
//		DXMaterial* mat = NULL;
//		CreateMaterial(aiscene->mMaterials[aimesh->mMaterialIndex], scene, mat);
//		subdata.material = mat;
//
//		mesh.meshes.push_back(subdata);
//	}
//
//	std::map<std::string, SkinningMeshAnimationManager*> boneNameToAnimationManager;		// bones are asigned to 
//	std::vector<DXModel*> triModels;
//	// create triangle meshes
//	for(auto& mesh : meshDataTri)
//	{
//		DXModel* model = new DXModel();		
//		//model->Create(DXUTGetD3D11DeviceContext(), mesh.vertices,mesh.meshes[0].)
//		V_RETURN(CreateTriangleModel(model, mesh));
//		triModels.push_back(model);
//	}
//
//	// create adaptive (quad) meshes
//	std::vector<AdaptiveMesh*> subdModels;
//	for(auto& mesh: meshDataQuad)
//	{
//
//	}
//
//
//	// now all models are created, next we traverse assimp hierarchy to find model instances and add the models to the scene
//
//	std::cout << "found "  << meshDataTri.size() << " triangle meshes " << std::endl;
//	for(int i =0 ; i < meshDataTri.size();++i)
//	{
//		std::cout << "mesh " << i << " contains " << meshDataTri[i].meshes.size() << " submeshes " << std::endl;
//		std::cout << "\t " << " numN " << meshDataTri[i].normals.size() << " texcoords "  << meshDataTri[i].texcoords.size()<< std::endl;
//		for(auto& submesh :  meshDataTri[i].meshes)
//		{
//			std::cout << "base vertices: " << submesh.baseVertex << std::endl;			
//		}
//	}
//
//	std::cout << "found "  << meshDataQuad.size() << " quad meshes " << std::endl;
//	for(int i =0 ; i < meshDataQuad.size();++i)
//	{
//		std::cout << "mesh " << i << " contains " << meshDataQuad[i].meshes.size() << " submeshes " << std::endl;
//		std::cout << "\t " << " numN " << meshDataQuad[i].normals.size() << " texcoords "  << meshDataQuad[i].texcoords.size()<< std::endl;
//		for(auto& submesh :  meshDataQuad[i].meshes)
//		{
//			std::cout << "base vertices: " << submesh.baseVertex << std::endl;			
//		}
//	}
//
//	return S_OK;
//	std::vector<std::vector<const aiNode*>> meshToSubmesh(aiscene->mNumMeshes);
//
//	UINT baseVertexIndex = 0;
//	for(UINT i = 0; i < aiscene->mRootNode->mNumChildren; ++i)
//	{
//		const aiNode*  node = aiscene->mRootNode->mChildren[i];
//		const char*	   name = node->mName.C_Str();
//
//		if(node->mNumChildren > 0)
//			std::cerr << "[WARNING] scene contains hierarchies" << fileName << std::endl;
//
//		if(node->mNumMeshes > 1)
//			std::cerr << "[WARNING] model " << name << " contains multiple child meshes" << std::endl; 
//
//		if(node->mNumMeshes == 0) continue;
//
//		//meshToSubmesh[node->mNumMeshes] = n;		
//		std::cout << "num meshes: " << node->mMeshes << std::endl;
//		
//
//		std::cout << "children" << std::endl;
//		for(int j = 0;j <node->mNumChildren;++j)
//		{
//			std::cout << node->mChildren[j]->mName.C_Str() << std::endl;
//		}
//		std::cout << "-----------------" << std::endl;
//		const UINT meshID  = node->mMeshes[0];
//		const aiMesh* mesh = aiscene->mMeshes[meshID];
//
//		if(!mesh->HasPositions() || !mesh->HasFaces())		
//			continue;
//
//		//if (skinningMgr) 
//		//{ // Fetch bones
//		//	// Increase the size for the new vertices
//		//	skinningMgr->getBoneDataRef().resize(skinningMgr->getBoneDataRef().size() + mesh->mNumVertices);
//
//		//	UINT numMeshBones = mesh->HasBones() ? mesh->mNumBones : 0;
//		//	fprintf(stderr, "Found %d bones", numMeshBones);
//		//	UINT boneID = 0;
//		//	for (UINT j = 0; j < numMeshBones; j++) {
//		//		std::string boneName(mesh->mBones[j]->mName.data);
//		//		fprintf(stderr, "Processing bone \"%s\"\n", boneName.c_str());
//
//		//		XMMATRIX O(mesh->mBones[j]->mOffsetMatrix[0]);
//		//		boneID = skinningMgr->addBone(boneName, O);
//
//		//		// Set per vertex bone information: boneID & weight
//		//		UINT numBoneWeights = mesh->mBones[j]->mNumWeights; 
//		//		for (UINT k = 0; k < numBoneWeights; ++k) {
//		//			UINT vertexID = mesh->mBones[j]->mWeights[k].mVertexId + baseVertexIndex; // mVertexID is "local to the current mesh"
//		//			skinningMgr->getBoneDataRef().at(vertexID).setBoneData(boneID, mesh->mBones[j]->mWeights[k].mWeight);					
//		//		}
//		//	}
//
//		//	skinningMgr->getBaseVertexIndicesRef().push_back(baseVertexIndex);
//		//}
//
//
//		// physics
//		btRigidBody* phyBody = withPhysics ? physicsLoader->getRigidBodyByName(name) : NULL;
//
//		if(phyBody && phyBody->getMotionState() == NULL)
//		{
//			std::cerr << "[WARNING]: object without motion state: " << name << std::endl;
//			phyBody->setMotionState(new btDefaultMotionState(phyBody->getWorldTransform()));
//		}
//
//
//		// material
//		DXMaterial* mat = NULL;
//		V_RETURN(CreateMaterial(aiscene->mMaterials[mesh->mMaterialIndex], scene, mat));
//
//		// Model Matrix
//
//		std::cout << node->mName.C_Str() << std::endl;
//		XMMATRIX  modelMatrix = XMMatrixTranspose(DirectX::XMMATRIX(node->mTransformation[0]));	
//
//
//		XMMATRIX flipX = XMMATRIX(  1,0,0,0,
//			0,1,0,0,
//			0,0,1,0,
//			0,0,0,1);
//
//		//modelMatrix =switchYZ* modelMatrix * switchYZ;
//		//modelMatrix =  flipX * modelMatrix * flipX;
//
//
//		//XMMATRIX  modelMatrix = (DirectX::XMMATRIX(node->mTransformation[0]));		
//		XMVECTOR dScale;
//		if(phyBody)
//		{		
//			// if object has a physics representation bullet handles orientation and transformation
//			// hence we are only interested in initial scale		
//			XMVECTOR dQrot;
//			XMVECTOR dTrans;
//			XMMatrixDecompose(&dScale, &dQrot, &dTrans, modelMatrix);		
//		}
//
//
//		// add models to scene
//		if(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE)
//		{			
//			DXModel* model = new DXModel();
//			model->SetName(name);
//			model->SetIsDeformable(false);
//			model->SetIsSubD(false);
//			//model->SetIsDeformable(upperCase(std::string(name)).find("DEFORMABLE") != std::string::npos);			
//			//if (upperCase(std::string(name)).find("DEFORMABLE") != std::string::npos)
//			//	MessageBoxA(0,"","",MB_OK);
//			model->SetMaterial(mat);
//			model->SetPhysicsObject(phyBody);
//
//			model->_baseVertex = baseVertexIndex;
//			model->m_skinningMeshAnimationManager = NULL; //skinningMgr;
//
//			V_RETURN(CreateTriangleModel(mesh, model));			
//
//			if(phyBody)	
//			{
//				// checkme store as scale matrix
//				model->SetModelMatrix(XMMatrixIdentity() );
//				model->SetPhysicsScaleMatrix(XMMatrixScalingFromVector(dScale) );
//				phyBody->setUserPointer((void*)model);
//			}
//			else
//			{
//				model->SetModelMatrix( modelMatrix );
//			}
//
//			scene->AddModel(model);	
//			baseVertexIndex += mesh->mNumVertices;
//		}
//		else if(mesh->mPrimitiveTypes == aiPrimitiveType_POLYGON)
//		{
//			AdaptiveMesh* model = new AdaptiveMesh();
//			model->SetName(name);			
//			model->SetIsDeformable(ToUpperCase(std::string(name)).find("DEFORMABLE") != std::string::npos);			
//			model->SetMaterial(mat);
//			model->GetMaterial()->_hasDiffuseTex = true; // CHECKME needed for rendering 
//			model->SetPhysicsObject(phyBody);
//			//model->SetIsSubD(true);
//
//			// try to find disp file
//			attachDispFile(model, aiscene->mMaterials[mesh->mMaterialIndex]);
//
//			V_RETURN(CreateCatmulClarkModel(mesh, model));
//
//			if(phyBody)	
//			{
//				// checkme store as scale matrix
//				model->SetModelMatrix(XMMatrixIdentity() );
//				model->SetPhysicsScaleMatrix(XMMatrixScalingFromVector(dScale) );
//				phyBody->setUserPointer((void*)model);
//			}
//			else
//			{
//				model->SetModelMatrix( modelMatrix );				
//			}
//
//			scene->AddModel(model);				
//		}					
//	}
//
//	std::cout << "nMeshes: " << meshToSubmesh.size() << std::endl;
//	for(auto m : meshToSubmesh)
//	{
////		std::cout << "num submeshes: " << m.second.size() << std::endl;
//	}
//
//	//if (skinningMgr) {
//	//	skinningMgr->printBones();
//
//	//	// Check animations and get all the node data.
//	//	UINT numAnimations = aiscene->HasAnimations() ? aiscene->mNumAnimations : 0;
//	//	fprintf(stderr, "\n----\n#Animations: %d \n----\n", numAnimations);
//	//	//XMStoreFloat4x4A(&skinningMgr->globalInverseTransformationRef(), XMMatrixInverse(NULL, XMMATRIX(aiscene->mRootNode->mTransformation[0])));
//	//	skinningMgr->globalInverseTransformationRef()= XMMatrixInverse(NULL, XMMATRIX(aiscene->mRootNode->mTransformation[0]));
//
//	//	//m_globalInverseTransformation = glm::inverse(aiMatrix4x4ToMat4(scene->mRootNode->mTransformation));	
//	//	//
//	//	skinningMgr->getRootNodesRef().resize(numAnimations, NULL);
//	//	skinningMgr->getAnimationsRef().resize(numAnimations, NULL);
//
//	//	// Load and store animation data
//	//	for (UINT i = 0; i < numAnimations; ++i) {
//	//		aiAnimation* assimpAnimation = aiscene->mAnimations[i];
//	//		skinningMgr->getAnimationsRef()[i] = new Animation();
//	//		skinningMgr->getAnimationsRef()[i]->setNumChannels(assimpAnimation->mNumChannels);
//	//		skinningMgr->getAnimationsRef()[i]->setDuration((float)assimpAnimation->mDuration);
//	//		skinningMgr->getAnimationsRef()[i]->setTicksPerSecond((float)assimpAnimation->mTicksPerSecond);
//
//	//		for (UINT j = 0; j < assimpAnimation->mNumChannels; ++j) {
//	//			aiNodeAnim* assimpNodeAnimation = assimpAnimation->mChannels[j];				
//	//			NodeAnimation*& nodeAnimation = skinningMgr->getAnimationsRef()[i]->getChannelRef(j);
//	//			nodeAnimation = new NodeAnimation(std::string(assimpNodeAnimation->mNodeName.data));
//
//	//			UINT numRotations = assimpNodeAnimation->mNumRotationKeys;
//	//			UINT numTranslations = assimpNodeAnimation->mNumPositionKeys;
//	//			UINT numScalings = assimpNodeAnimation->mNumScalingKeys;
//
//	//			for (UINT k = 0; k < numRotations; ++k) {
//	//				XMFLOAT4A R(
//	//					assimpNodeAnimation->mRotationKeys[k].mValue.x,
//	//					assimpNodeAnimation->mRotationKeys[k].mValue.y,
//	//					assimpNodeAnimation->mRotationKeys[k].mValue.z,
//	//					assimpNodeAnimation->mRotationKeys[k].mValue.w);
//
//	//				nodeAnimation->addRotation(RotationKey((float)assimpNodeAnimation->mRotationKeys[k].mTime, R));
//	//			}
//
//	//			for (UINT k = 0; k < numTranslations; ++k) {
//	//				XMFLOAT3A T(
//	//					assimpNodeAnimation->mPositionKeys[k].mValue.x,
//	//					assimpNodeAnimation->mPositionKeys[k].mValue.y,
//	//					assimpNodeAnimation->mPositionKeys[k].mValue.z);
//
//	//				nodeAnimation->addTranslation(TranslationKey((float)assimpNodeAnimation->mPositionKeys[k].mTime, T));				
//	//			}
//
//	//			for (UINT k = 0; k < numScalings; ++k) {
//	//				XMFLOAT3A S(
//	//					assimpNodeAnimation->mScalingKeys[k].mValue.x,
//	//					assimpNodeAnimation->mScalingKeys[k].mValue.y,
//	//					assimpNodeAnimation->mScalingKeys[k].mValue.z);
//
//	//				nodeAnimation->addScaling(ScalingKey((float)assimpNodeAnimation->mScalingKeys[k].mTime, S));
//	//			}
//
//	//		}
//	//	}
//
//	//	// Traverse assimp node hierachy and copy it into the local datastructures
//	//	for (UINT i = 0; i < numAnimations; ++i) {
//	//		traverseAndStoreAssimpNodeHierachy(aiscene->mRootNode, skinningMgr->getRootNodesRef()[i], i, skinningMgr);
//	//	}
//
//	//	skinningMgr->setupBuffers();
//	//	scene->GetSkinningAnimationManagers().push_back(skinningMgr);
//	//}
//	
//	delete physicsLoader; 
//
//	return hr;
//}

//HRESULT ModelLoader::LoadSkinned( std::string fileName, Scene* scene )
//{	
//	HRESULT hr = S_OK;
//
//	//// mesh loader
//	//Assimp::Importer importer;
//	//const struct aiScene* aiscene = NULL;
//	//aiscene = importer.ReadFile(fileName.c_str(),   0 // import post-process flags
//	//	//| aiProcess_Triangulate
//	//	//| aiProcess_GenSmoothNormals 
//	//	//| aiProcess_JoinIdenticalVertices 
//	//	//| aiProcess_SortByPType
//	//	//| aiProcess_PreTransformVertices
//	//	);
//	//if(!aiscene) {
//	//	MessageBoxA(0, importer.GetErrorString(), "ERROR", MB_OK);
//	//	return S_FALSE;
//	//}
//
//	//// physics loader
//	//// check if physics component is available
//	//bool withPhysics = scene->GetPhysics() == NULL ? false : true;	
//	//btBulletWorldImporter* physicsLoader = NULL;
//	//if(withPhysics)
//	//	physicsLoader = new btBulletWorldImporter(scene->GetPhysics()->GetDynamicsWorld());
//
//	//std::string physicsFile = ReplaceFileExtension(fileName, ".bullet");
//	//withPhysics = physicsLoader->loadFile(physicsFile.c_str());
//
//	//if(!withPhysics)
//	//	std::cerr << "cannot find " <<physicsFile << std::endl;
//
//	//// HS: we use the objects name to connect physics and geometry representation
//	//// assimp does not store names in the global mesh list (aiScene->mMeshes[])
//	//// names are only stored for instances
//	//// hence, we have to traverse the node tree
//
//	//// BK: well since your approach does only work for flat hierachies...
//	//// I traverse the node tree.
//	//std::map<UINT,const aiNode*> meshIDtoNode;
//
//	//traverseAssimpNodeHierachy(aiscene->mRootNode,  meshIDtoNode, fileName);
//
//	//std::vector<UINT> indices;
//	//std::vector<UINT> origPositionIndices;
//	//std::vector<UINT> faceSizes;
//
//	//UINT baseIndexIndices = 0;
//	//UINT baseIndexOrigPositionIndices = 0;
//
//	//for (UINT i = 0; i < aiscene->mNumMeshes; ++i) {
//	//	aiMesh* mesh = aiscene->mMeshes[i];
//
//	//	if(!mesh->HasPositions() || !mesh->HasFaces())
//	//		continue;
//
//	//	UINT numMeshFaces = mesh->mNumFaces;
//	//	for (UINT j = 0; j < numMeshFaces; ++j) {
//
//	//		const aiFace& face = mesh->mFaces[j];
//	//		UINT numFaceIndices = face.mNumIndices;
//	//		faceSizes.push_back(numFaceIndices);
//
//	//		for (UINT v = 0; v < numFaceIndices; ++v) {
//	//			indices.push_back(face.mIndices[v] + baseIndexIndices);
//	//			origPositionIndices.push_back(face.mOrigPosIndices[v] + baseIndexOrigPositionIndices);
//	//			//if (face.mOrigPosIndices[v] + baseIndexOrigPositionIndices > 0x00ffffff) {
//	//			//	UINT f = face.mOrigPosIndices[v] + baseIndexOrigPositionIndices;
//	//			//	LogMessage("%d %d %d", j, f,  face.mOrigPosIndices[v]);
//	//			//	DebugBreak();
//	//			//}
//	//		}
//	//	}
//	//	
//	//	auto it = meshIDtoNode.find(i);
//	//	std::string meshName("");
//	//	if (it != meshIDtoNode.end()) {
//	//		meshName = std::string(it->second->mName.C_Str());
//	//		std::cout << "Mesh["<< i <<"]: \"" << meshName <<"\"" << std::endl;
//	//	} else {
//	//		std::cout << "No node for mesh found... skipping mesh[" << i << "]" << std::endl;
//	//		continue;
//	//	}
//
//	//	const aiNode* node = it->second;
//
//	//	// physics
//	//	btRigidBody* phyBody = NULL;
//	//	if(withPhysics)
//	//		phyBody = physicsLoader->getRigidBodyByName(meshName.c_str());
//
//	//	if(phyBody && phyBody->getMotionState() == NULL) {
//	//		std::cerr << "[WARNING]: object without motion state: " << meshName << std::endl;
//	//		phyBody->setMotionState(new btDefaultMotionState(phyBody->getWorldTransform()));
//	//	}
//
//	//	gCurrModelPath = ExtractPath(fileName);
//	//	gCurrModelPath.append("/");
//	//	std::cout << "modelpath: " << gCurrModelPath << std::endl;
//
//	//	DXMaterial* mat = NULL;
//	//	V_RETURN(createMaterial(aiscene->mMaterials[mesh->mMaterialIndex], scene, mat));
//	//	std::cout << mat->_name << std::endl;		
//
//	//	// Model Matrix
//	//	using namespace DirectX;
//
//	//	XMMATRIX  modelMatrix = XMMatrixTranspose(DirectX::XMMATRIX(node->mTransformation[0]));		
//	//	XMVECTOR dScale;
//	//	if(phyBody)	{		
//	//		// if object has a physics representation bullet handles orientation and transformation
//	//		// hence we are only interested in initial scale		
//	//		XMVECTOR dQrot;
//	//		XMVECTOR dTrans;
//	//		XMMatrixDecompose(&dScale, &dQrot, &dTrans, modelMatrix);		
//	//	}
//
//	//	// add models to scene
//	//	if(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE)
//	//	{			
//	//		DXModel* model = new DXModel();
//	//		model->SetName(meshName);			
//	//		model->SetMaterial(mat);
//	//		model->SetPhysicsObject(phyBody);
//	//		model->SetIsSubD(false);
//	//		V_RETURN(createModel(mesh, model));			
//
//	//		if(phyBody)	
//	//		{
//	//			// checkme store as scale matrix
//	//			model->SetModelMatrix(XMMatrixIdentity() );
//	//			model->SetPhysicsScaleMatrix(XMMatrixScalingFromVector(dScale) );
//	//			phyBody->setUserPointer((void*)model);
//	//		}
//	//		else
//	//		{
//	//			model->SetModelMatrix( modelMatrix );
//	//			//model->SetModelMatrix( XMMatrixIdentity() );
//	//		}
//	//		scene->AddModel(model);	
//	//	} else {
//	//		AdaptiveMesh* model = new AdaptiveMesh();
//	//		model->SetIsSkinned(true); // Skinned mesh!
//	//		model->SetName(meshName);			
//	//		model->SetMaterial(mat);
//	//		model->SetPhysicsObject(phyBody);
//	//		model->SetIsSubD(true);
//
//	//		{
//	//			using namespace SkinningAnimationClasses;
//	//			if (model->m_skinningMeshAnimationManager  == NULL)
//	//				model->m_skinningMeshAnimationManager = new SkinningMeshAnimationManager();
//	//			SkinningMeshAnimationManager& skinningMgr = *(model->m_skinningMeshAnimationManager);
//
//	//			skinningMgr.getBoneDataRef().resize(mesh->mNumOrigVertices + baseIndexIndices);
//	//			// Fetch bones
//	//			UINT numMeshBones = mesh->HasBones() ? mesh->mNumBones : 0;
//	//			fprintf(stderr, "Found %d bones", numMeshBones);
//	//			UINT boneID = 0;
//	//			for (UINT j = 0; j < numMeshBones; j++) {
//	//				std::string boneName(mesh->mBones[j]->mName.data);
//	//				fprintf(stderr, "Processing bone \"%s\"\n", boneName.c_str());
//
//	//				XMFLOAT4X4A O;
//	//				XMStoreFloat4x4A(&O, XMMATRIX(mesh->mBones[j]->mOffsetMatrix[0]));
//	//				boneID = skinningMgr.addBone(boneName, O);
//
//	//				// Set per vertex bone information: boneID & weight
//	//				UINT numBoneWeights = mesh->mBones[j]->mNumWeights; 
//	//				for (UINT k = 0; k < numBoneWeights; ++k) {
//	//					if (origPositionIndices.size() <= baseIndexIndices + mesh->mBones[j]->mWeights[k].mVertexId) {
//	//						DebugBreak();
//	//					}
//	//					UINT vertexID = origPositionIndices.at(baseIndexIndices + mesh->mBones[j]->mWeights[k].mVertexId);
//	//					skinningMgr.getBoneDataRef().at(vertexID).setBoneData(boneID, mesh->mBones[j]->mWeights[k].mWeight);					
//	//				}
//	//			}
//	//			baseIndexIndices += mesh->mNumOrigVertices;
//
//	//			skinningMgr.setupBuffers();
//
//	//		}
//
//	//		// try to find disp file
//	//		attachDispFile(model, aiscene->mMaterials[mesh->mMaterialIndex]);
//
//	//		V_RETURN(createCatmulClarkModel(mesh, model));
//
//
//	//		if(phyBody)	
//	//		{
//	//			// checkme store as scale matrix
//	//			model->SetModelMatrix(XMMatrixIdentity() );
//	//			model->SetPhysicsScaleMatrix(XMMatrixScalingFromVector(dScale) );
//	//			phyBody->setUserPointer((void*)model);
//	//		}
//	//		else
//	//		{
//	//			model->SetModelMatrix( modelMatrix );
//	//			//model->SetModelMatrix( XMMatrixIdentity() );
//	//		}
//
//	//		{
//	//			UINT numAnimations = aiscene->HasAnimations() ? aiscene->mNumAnimations : 0;
//	//			fprintf(stderr, "\n----\n#Animations: %d \n----\n", numAnimations);
//	//			SkinningMeshAnimationManager& skinningMgr = *(model->m_skinningMeshAnimationManager);
//	//			XMStoreFloat4x4A(&skinningMgr.globalInverseTransformationRef(), XMMatrixInverse(NULL, XMMATRIX(aiscene->mRootNode->mTransformation[0])));
//
//	//			//m_globalInverseTransformation = glm::inverse(aiMatrix4x4ToMat4(scene->mRootNode->mTransformation));	
//	//			//
//	//			skinningMgr.getRootNodesRef().resize(numAnimations, NULL);
//	//			skinningMgr.getAnimationsRef().resize(numAnimations, NULL);
//	//		
//	//			// Load and store animation data
//	//			for (UINT i = 0; i < numAnimations; ++i) {
//	//				aiAnimation* assimpAnimation = aiscene->mAnimations[i];
//	//				skinningMgr.getAnimationsRef()[i] = new Animation();
//	//				skinningMgr.getAnimationsRef()[i]->setNumChannels(assimpAnimation->mNumChannels);
//	//				skinningMgr.getAnimationsRef()[i]->setDuration((float)assimpAnimation->mDuration);
//	//				skinningMgr.getAnimationsRef()[i]->setTicksPerSecond((float)assimpAnimation->mTicksPerSecond);
//	//			
//	//				for (UINT j = 0; j < assimpAnimation->mNumChannels; ++j) {
//	//					aiNodeAnim* assimpNodeAnimation = assimpAnimation->mChannels[j];				
//	//					NodeAnimation*& nodeAnimation = skinningMgr.getAnimationsRef()[i]->getChannelRef(j);
//	//					nodeAnimation = new NodeAnimation(std::string(assimpNodeAnimation->mNodeName.data));
//	//			
//	//					UINT numRotations = assimpNodeAnimation->mNumRotationKeys;
//	//					UINT numTranslations = assimpNodeAnimation->mNumPositionKeys;
//	//					UINT numScalings = assimpNodeAnimation->mNumScalingKeys;
//	//			
//	//					for (UINT k = 0; k < numRotations; ++k) {
//	//						XMFLOAT4A R(
//	//							assimpNodeAnimation->mRotationKeys[k].mValue.x,
//	//							assimpNodeAnimation->mRotationKeys[k].mValue.y,
//	//							assimpNodeAnimation->mRotationKeys[k].mValue.z,
//	//							assimpNodeAnimation->mRotationKeys[k].mValue.w);
//	//						
//	//						nodeAnimation->addRotation(RotationKey((float)assimpNodeAnimation->mRotationKeys[k].mTime, R));
//	//					}
//	//			
//	//					for (UINT k = 0; k < numTranslations; ++k) {
//	//						XMFLOAT3A T(
//	//								assimpNodeAnimation->mPositionKeys[k].mValue.x,
//	//								assimpNodeAnimation->mPositionKeys[k].mValue.y,
//	//								assimpNodeAnimation->mPositionKeys[k].mValue.z);
//
//	//						nodeAnimation->addTranslation(TranslationKey((float)assimpNodeAnimation->mPositionKeys[k].mTime, T));				
//	//					}
//
//	//					for (UINT k = 0; k < numScalings; ++k) {
//	//						XMFLOAT3A S(
//	//								assimpNodeAnimation->mScalingKeys[k].mValue.x,
//	//								assimpNodeAnimation->mScalingKeys[k].mValue.y,
//	//								assimpNodeAnimation->mScalingKeys[k].mValue.z);
//
//	//						nodeAnimation->addScaling(ScalingKey((float)assimpNodeAnimation->mScalingKeys[k].mTime, S));
//	//					}
//	//			
//	//				}
//	//			}
//
//	//			// Traverse assimp node hierachy and copy it into the local datastructures
//	//			for (UINT i = 0; i < numAnimations; ++i) {
//	//				traverseAndStoreAssimpNodeHierachy(aiscene->mRootNode, skinningMgr.getRootNodesRef()[i], i, &skinningMgr);
//	//			}
//	//		}
//
//
//	//		scene->AddModel(model);			
//
//	//	}
//	//}
//
//	//delete physicsLoader; 
//
//	return hr;
//}
//
//
//HRESULT ModelLoader::LoadSkinnedTriangle( std::string fileName, Scene* scene, bool skipUnnamedMeshes /* = false */)
//{	
//	HRESULT hr = S_OK;
//
//	//// mesh loader
//	//Assimp::Importer importer;
//	//const struct aiScene* aiscene = NULL;
//	//aiscene = importer.ReadFile(fileName.c_str(),   0 // import post-process flags
//	//	//| aiProcess_Triangulate
//	//	//| aiProcess_GenSmoothNormals 
//	//	//| aiProcess_JoinIdenticalVertices 
//	//	//| aiProcess_ConvertToLeftHanded
//	//	//| aiProcess_FlipWindingOrder
//	//	//| aiProcess_SortByPType
//	//	//| aiProcess_PreTransformVertices
//	//	//| aiProcess_LimitBoneWeights
//	//	//| aiProcess_Debone
//	//	//| aiProcess_ConvertToLeftHanded
//	//	);
//	//if(!aiscene) {
//	//	MessageBoxA(0, importer.GetErrorString(), "ERROR", MB_OK);
//	//	return S_FALSE;
//	//}
//
//	//// physics loader
//	//// check if physics component is available
//	//bool withPhysics = scene->GetPhysics() == NULL ? false : true;	
//	//btBulletWorldImporter* physicsLoader = NULL;
//	//if(withPhysics)
//	//	physicsLoader = new btBulletWorldImporter(scene->GetPhysics()->GetDynamicsWorld());
//
//	//std::string physicsFile = ReplaceFileExtension(fileName, ".bullet");
//	//withPhysics = physicsLoader->loadFile(physicsFile.c_str());
//
//	//if(!withPhysics)
//	//	std::cerr << "cannot find " <<physicsFile << std::endl;
//
//	//// HS: we use the objects name to connect physics and geometry representation
//	//// assimp does not store names in the global mesh list (aiScene->mMeshes[])
//	//// names are only stored for instances
//	//// hence, we have to traverse the node tree
//
//	//// BK: well since your approach does only work for flat hierachies...
//	//// I traverse the node tree.
//	//std::map<UINT,const aiNode*> meshIDtoNode;
//
//	//traverseAssimpNodeHierachy(aiscene->mRootNode,  meshIDtoNode, fileName);
//
//	//std::cout << "num Animations: " << aiscene->mNumAnimations << std::endl;
//	//std::cout << importer.GetErrorString() << std::endl;
//	//UINT numAnimations = aiscene->HasAnimations() ? aiscene->mNumAnimations : 0;
//	//fprintf(stderr, "\n----\n#Animations: %d \n----\n", numAnimations);
//	//SkinningMeshAnimationManager* skinningMgr = numAnimations != 0 ? new SkinningMeshAnimationManager() : NULL;
//	//
//	//
//
//	////skinningMgr = NULL;
//	//UINT baseVertexIndex = 0;
//
//	//for (UINT i = 0; i < aiscene->mNumMeshes; ++i) {
//	//	aiMesh* mesh = aiscene->mMeshes[i];
//
//	//	if(!mesh->HasPositions() || !mesh->HasFaces())
//	//		continue;
//
//	//	auto it = meshIDtoNode.find(i);
//	//	const aiNode* node = NULL;
//	//	std::string meshName("");
//	//	if (it != meshIDtoNode.end()) {
//	//		meshName = std::string(it->second->mName.C_Str());
//	//		node = it->second;
//	//		std::cout << "Mesh["<< i <<"]: \"" << meshName <<"\"" << std::endl;
//	//	} else if (skipUnnamedMeshes) {
//	//		std::cout << "No node for mesh found... skipping mesh[" << i << "]" << std::endl;
//	//	}
//	//	
//	//	if (skinningMgr) 
//	//	{ // Fetch bones
//	//		// Increase the size for the new vertices
//	//		skinningMgr->getBoneDataRef().resize(skinningMgr->getBoneDataRef().size() + mesh->mNumVertices);
//	//	
//	//		UINT numMeshBones = mesh->HasBones() ? mesh->mNumBones : 0;
//	//		fprintf(stderr, "Found %d bones", numMeshBones);
//	//		UINT boneID = 0;
//	//		for (UINT j = 0; j < numMeshBones; j++) {
//	//			std::string boneName(mesh->mBones[j]->mName.data);
//	//			fprintf(stderr, "Processing bone \"%s\"\n", boneName.c_str());
//
//	//			//XMFLOAT4X4A O;
//	//			//XMStoreFloat4x4A(&O, XMMATRIX(mesh->mBones[j]->mOffsetMatrix[0]));
//	//			XMMATRIX O(mesh->mBones[j]->mOffsetMatrix[0]);
//	//			boneID = skinningMgr->addBone(boneName, O);
//
//	//			// Set per vertex bone information: boneID & weight
//	//			UINT numBoneWeights = mesh->mBones[j]->mNumWeights; 
//	//			for (UINT k = 0; k < numBoneWeights; ++k) {
//	//				UINT vertexID = mesh->mBones[j]->mWeights[k].mVertexId + baseVertexIndex; // mVertexID is "local to the current mesh"
//	//				skinningMgr->getBoneDataRef().at(vertexID).setBoneData(boneID, mesh->mBones[j]->mWeights[k].mWeight);					
//	//			}
//	//		}
//
//	//		skinningMgr->getBaseVertexIndicesRef().push_back(baseVertexIndex);
//	//	}
//
//	//	// physics
//	//	btRigidBody* phyBody = NULL;
//	//	if(withPhysics)
//	//		phyBody = physicsLoader->getRigidBodyByName(meshName.c_str());
//
//	//	if(phyBody && phyBody->getMotionState() == NULL) 
//	//	{
//	//		std::cerr << "[WARNING]: object without motion state: " << meshName << std::endl;
//	//		phyBody->setMotionState(new btDefaultMotionState(phyBody->getWorldTransform()));
//	//	}
//
//	//	gCurrModelPath = ExtractPath(fileName);
//	//	gCurrModelPath.append("/");
//	//	std::cout << "modelpath: " << gCurrModelPath << std::endl;
//
//	//	DXMaterial* mat = NULL;
//	//	V_RETURN(CreateMaterial(aiscene->mMaterials[mesh->mMaterialIndex], scene, mat));
//	//	std::cout << mat->_name << std::endl;		
//
//	//	// Model Matrix
//	//	using namespace DirectX;
//
//	//	//XMMATRIX  m = XMMatrixTranspose(DirectX::XMMATRIX(node->mTransformation[0]));		
//	//	XMMATRIX  modelMatrix = XMMatrixIdentity(); // Fix for chris
//	//	// OLD XMMATRIX  modelMatrix =XMMatrixTranspose(NULL, node != NULL ? DirectX::XMMATRIX(node->mTransformation[0]) : DirectX::XMMatrixIdentity());		
//	//	XMVECTOR dScale;
//	//	if(phyBody)	{		
//	//		// if object has a physics representation bullet handles orientation and transformation
//	//		// hence we are only interested in initial scale		
//	//		XMVECTOR dQrot;
//	//		XMVECTOR dTrans;
//	//		XMMatrixDecompose(&dScale, &dQrot, &dTrans, modelMatrix);		
//	//	}
//
//	//	// add models to scene
//	//	if(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE)
//	//	{			
//	//		DXModel* model = new DXModel();
//	//		model->SetName(meshName);			
//	//		model->SetMaterial(mat);
//	//		model->SetPhysicsObject(phyBody);
//	//		model->SetIsSubD(false);
//	//		model->_baseVertex = baseVertexIndex;
//	//		model->m_skinningMeshAnimationManager = skinningMgr;
//
//	//		V_RETURN(CreateTriangleModel(mesh, model));			
//
//	//		if(phyBody)	{
//	//			// checkme store as scale matrix
//	//			model->SetModelMatrix(XMMatrixIdentity() );
//	//			model->SetPhysicsScaleMatrix(XMMatrixScalingFromVector(dScale) );
//	//			phyBody->setUserPointer((void*)model);
//	//		} else {
//	//			model->SetModelMatrix( modelMatrix );
//	//			//model->SetModelMatrix( XMMatrixIdentity() );
//	//		}
//
//	//		scene->AddModel(model);	
//	//		baseVertexIndex += mesh->mNumVertices;
//	//	}
//	//}
//
//	//if (skinningMgr) {
//	//	skinningMgr->printBones();
//
//	//	// Check animations and get all the node data.
//	//	UINT numAnimations = aiscene->HasAnimations() ? aiscene->mNumAnimations : 0;
//	//	fprintf(stderr, "\n----\n#Animations: %d \n----\n", numAnimations);
//	//	//XMStoreFloat4x4A(&skinningMgr->globalInverseTransformationRef(), XMMatrixInverse(NULL, XMMATRIX(aiscene->mRootNode->mTransformation[0])));
//	//	skinningMgr->globalInverseTransformationRef()= XMMatrixInverse(NULL, XMMATRIX(aiscene->mRootNode->mTransformation[0]));
//
//	//	//m_globalInverseTransformation = glm::inverse(aiMatrix4x4ToMat4(scene->mRootNode->mTransformation));	
//	//	//
//	//	skinningMgr->getRootNodesRef().resize(numAnimations, NULL);
//	//	skinningMgr->getAnimationsRef().resize(numAnimations, NULL);
//	//	
//	//	// Load and store animation data
//	//	for (UINT i = 0; i < numAnimations; ++i) {
//	//		aiAnimation* assimpAnimation = aiscene->mAnimations[i];
//	//		skinningMgr->getAnimationsRef()[i] = new Animation();
//	//		skinningMgr->getAnimationsRef()[i]->setNumChannels(assimpAnimation->mNumChannels);
//	//		skinningMgr->getAnimationsRef()[i]->setDuration((float)assimpAnimation->mDuration);
//	//		skinningMgr->getAnimationsRef()[i]->setTicksPerSecond((float)assimpAnimation->mTicksPerSecond);
//	//		
//	//		for (UINT j = 0; j < assimpAnimation->mNumChannels; ++j) {
//	//			aiNodeAnim* assimpNodeAnimation = assimpAnimation->mChannels[j];				
//	//			NodeAnimation*& nodeAnimation = skinningMgr->getAnimationsRef()[i]->getChannelRef(j);
//	//			nodeAnimation = new NodeAnimation(std::string(assimpNodeAnimation->mNodeName.data));
//	//		
//	//			UINT numRotations = assimpNodeAnimation->mNumRotationKeys;
//	//			UINT numTranslations = assimpNodeAnimation->mNumPositionKeys;
//	//			UINT numScalings = assimpNodeAnimation->mNumScalingKeys;
//	//		
//	//			for (UINT k = 0; k < numRotations; ++k) {
//	//				XMFLOAT4A R(
//	//					assimpNodeAnimation->mRotationKeys[k].mValue.x,
//	//					assimpNodeAnimation->mRotationKeys[k].mValue.y,
//	//					assimpNodeAnimation->mRotationKeys[k].mValue.z,
//	//					assimpNodeAnimation->mRotationKeys[k].mValue.w);
//	//					
//	//				nodeAnimation->addRotation(RotationKey((float)assimpNodeAnimation->mRotationKeys[k].mTime, R));
//	//			}
//	//		
//	//			for (UINT k = 0; k < numTranslations; ++k) {
//	//				XMFLOAT3A T(
//	//						assimpNodeAnimation->mPositionKeys[k].mValue.x,
//	//						assimpNodeAnimation->mPositionKeys[k].mValue.y,
//	//						assimpNodeAnimation->mPositionKeys[k].mValue.z);
//
//	//				nodeAnimation->addTranslation(TranslationKey((float)assimpNodeAnimation->mPositionKeys[k].mTime, T));				
//	//			}
//
//	//			for (UINT k = 0; k < numScalings; ++k) {
//	//				XMFLOAT3A S(
//	//						assimpNodeAnimation->mScalingKeys[k].mValue.x,
//	//						assimpNodeAnimation->mScalingKeys[k].mValue.y,
//	//						assimpNodeAnimation->mScalingKeys[k].mValue.z);
//
//	//				nodeAnimation->addScaling(ScalingKey((float)assimpNodeAnimation->mScalingKeys[k].mTime, S));
//	//			}
//	//		
//	//		}
//	//	}
//
//	//	// Traverse assimp node hierachy and copy it into the local datastructures
//	//	for (UINT i = 0; i < numAnimations; ++i) {
//	//		traverseAndStoreAssimpNodeHierachy(aiscene->mRootNode, skinningMgr->getRootNodesRef()[i], i, skinningMgr);
//	//	}
//	//	
//	//	skinningMgr->setupBuffers();
//	//	scene->GetSkinningAnimationManagers().push_back(skinningMgr);
//	//}
//
//	//delete physicsLoader; 
//
//	return hr;
//}

//HRESULT ModelLoader::Load( std::string fileName, Scene* scene )
//{	
//	HRESULT hr = S_OK;
//
//	// mesh loader
//	Assimp::Importer importer;
//	const struct aiScene* aiscene = NULL;
//	aiscene = importer.ReadFile(fileName.c_str(),   0	// import post-processing flags
//		//| aiProcess_Triangulate
//		//| aiProcess_GenSmoothNormals 
//		  | aiProcess_JoinIdenticalVertices 
//		//| aiProcess_SortByPType
//		//| aiProcess_PreTransformVertices
//		//| aiProcess_ConvertToLeftHanded				// for directx 
//		//| aiProcess_Debone
//		  | aiProcess_LimitBoneWeights
//		);
//	
//	if(!aiscene)
//	{
//		MessageBoxA(0, importer.GetErrorString(), "ERROR", MB_OK);
//		return S_FALSE;
//	}
//
//	gCurrModelPath = ExtractPath(fileName);
//	gCurrModelPath.append("/");
//	std::cout << "modelpath: " << gCurrModelPath << std::endl;
//
//	// physics loader, check if physics component is available
//	bool withPhysics = scene->GetPhysics() == NULL;	
//	btBulletWorldImporter* physicsLoader = withPhysics ? new btBulletWorldImporter(scene->GetPhysics()->GetDynamicsWorld()) :  NULL;
//
//		
//	std::string physicsFile = ReplaceFileExtension(fileName, ".bullet");
//	withPhysics = physicsLoader->loadFile(physicsFile.c_str());
//
//	if(!withPhysics) std::cerr << "cannot find " << physicsFile << std::endl;
//
//	std::map<UINT,const aiNode*> meshIDtoNode;
//	traverseAssimpNodeHierachy(aiscene->mRootNode,  meshIDtoNode, fileName);
//
//	UINT numAnimations = aiscene->HasAnimations() ? aiscene->mNumAnimations : 0;
//	fprintf(stderr, "\n----\n#Animations: %d \n----\n", numAnimations);
//
//	// todo reuse existing manager
//	SkinningMeshAnimationManager* skinningMgr = numAnimations != 0 ? new SkinningMeshAnimationManager() : NULL;
//
//
//	// HS: we use the objects name to connect physics and geometry representation
//	// assimp does not store names in the global mesh list (aiScene->mMeshes[])
//	// names are only stored for instances
//	// hence, we have to traverse the node tree
//	
//	UINT baseVertexIndex = 0;
//	for(UINT i = 0; i < aiscene->mRootNode->mNumChildren; ++i)
//	{
//		const aiNode*  node = aiscene->mRootNode->mChildren[i];
//		const char*	   name = node->mName.C_Str();
//		
//		if(node->mNumChildren > 0)
//			std::cerr << "[WARNING] scene contains hierarchies" << fileName << std::endl;
//
//		if(node->mNumMeshes > 1)
//			std::cerr << "[WARNING] model " << name << " contains multiple child meshes" << std::endl; 
//
//		if(node->mNumMeshes == 0) continue;
//
//		std::cout << "children" << std::endl;
//		for(int j = 0;j <node->mNumChildren;++j)
//		{
//			std::cout << node->mChildren[j]->mName.C_Str() << std::endl;
//		}
//		std::cout << "-----------------" << std::endl;
//		const UINT meshID  = node->mMeshes[0];
//		const aiMesh* mesh = aiscene->mMeshes[meshID];
//
//		if(!mesh->HasPositions() || !mesh->HasFaces())		
//			continue;
//
//		if (skinningMgr) 
//		{ // Fetch bones
//			// Increase the size for the new vertices
//			skinningMgr->getBoneDataRef().resize(skinningMgr->getBoneDataRef().size() + mesh->mNumVertices);
//
//			UINT numMeshBones = mesh->HasBones() ? mesh->mNumBones : 0;
//			fprintf(stderr, "Found %d bones", numMeshBones);
//			UINT boneID = 0;
//			for (UINT j = 0; j < numMeshBones; j++) {
//				std::string boneName(mesh->mBones[j]->mName.data);
//				fprintf(stderr, "Processing bone \"%s\"\n", boneName.c_str());
//
//				XMMATRIX O(mesh->mBones[j]->mOffsetMatrix[0]);
//				boneID = skinningMgr->addBone(boneName, O);
//
//				// Set per vertex bone information: boneID & weight
//				UINT numBoneWeights = mesh->mBones[j]->mNumWeights; 
//				for (UINT k = 0; k < numBoneWeights; ++k) {
//					UINT vertexID = mesh->mBones[j]->mWeights[k].mVertexId + baseVertexIndex; // mVertexID is "local to the current mesh"
//					skinningMgr->getBoneDataRef().at(vertexID).setBoneData(boneID, mesh->mBones[j]->mWeights[k].mWeight);					
//				}
//			}
//
//			skinningMgr->getBaseVertexIndicesRef().push_back(baseVertexIndex);
//		}
//
//
//		// physics
//		btRigidBody* phyBody = withPhysics ? physicsLoader->getRigidBodyByName(name) : NULL;
//
//		if(phyBody && phyBody->getMotionState() == NULL)
//		{
//			std::cerr << "[WARNING]: object without motion state: " << name << std::endl;
//			phyBody->setMotionState(new btDefaultMotionState(phyBody->getWorldTransform()));
//		}
//		
//
//		// material
//		DXMaterial* mat = NULL;
//		V_RETURN(CreateMaterial(aiscene->mMaterials[mesh->mMaterialIndex], scene, mat));
//		
//		// Model Matrix
//	
//		std::cout << node->mName.C_Str() << std::endl;
//		XMMATRIX  modelMatrix = XMMatrixTranspose(DirectX::XMMATRIX(node->mTransformation[0]));	
//			
//
//		XMMATRIX flipX = XMMATRIX(  1,0,0,0,
//									0,1,0,0,
//									0,0,1,0,
//									0,0,0,1);
//
//		//modelMatrix =switchYZ* modelMatrix * switchYZ;
//		//modelMatrix =  flipX * modelMatrix * flipX;
//		
//		
//		//XMMATRIX  modelMatrix = (DirectX::XMMATRIX(node->mTransformation[0]));		
//		XMVECTOR dScale;
//		if(phyBody)
//		{		
//			// if object has a physics representation bullet handles orientation and transformation
//			// hence we are only interested in initial scale		
//			XMVECTOR dQrot;
//			XMVECTOR dTrans;
//			XMMatrixDecompose(&dScale, &dQrot, &dTrans, modelMatrix);		
//		}
//
//
//		// add models to scene
//		if(mesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE)
//		{			
//			DXModel* model = new DXModel();
//			model->SetName(name);
//			model->SetIsDeformable(false);
//			model->SetIsSubD(false);
//			//model->SetIsDeformable(upperCase(std::string(name)).find("DEFORMABLE") != std::string::npos);			
//			//if (upperCase(std::string(name)).find("DEFORMABLE") != std::string::npos)
//			//	MessageBoxA(0,"","",MB_OK);
//			model->SetMaterial(mat);
//			model->SetPhysicsObject(phyBody);
//
//			model->_baseVertex = baseVertexIndex;
//			model->m_skinningMeshAnimationManager = skinningMgr;
//
//			V_RETURN(CreateTriangleModel(mesh, model));			
//
//			if(phyBody)	
//			{
//				// checkme store as scale matrix
//				model->SetModelMatrix(XMMatrixIdentity() );
//				model->SetPhysicsScaleMatrix(XMMatrixScalingFromVector(dScale) );
//				phyBody->setUserPointer((void*)model);
//			}
//			else
//			{
//				model->SetModelMatrix( modelMatrix );
//			}
//
//			scene->AddModel(model);	
//			baseVertexIndex += mesh->mNumVertices;
//		}
//		else if(mesh->mPrimitiveTypes == aiPrimitiveType_POLYGON)
//		{
//			AdaptiveMesh* model = new AdaptiveMesh();
//			model->SetName(name);			
//			model->SetIsDeformable(ToUpperCase(std::string(name)).find("DEFORMABLE") != std::string::npos);			
//			model->SetMaterial(mat);
//			model->GetMaterial()->_hasDiffuseTex = true; // CHECKME needed for rendering 
//			model->SetPhysicsObject(phyBody);
//			//model->SetIsSubD(true);
//
//			// try to find disp file
//			attachDispFile(model, aiscene->mMaterials[mesh->mMaterialIndex]);
//
//			V_RETURN(CreateCatmulClarkModel(mesh, model));
//
//			if(phyBody)	
//			{
//				// checkme store as scale matrix
//				model->SetModelMatrix(XMMatrixIdentity() );
//				model->SetPhysicsScaleMatrix(XMMatrixScalingFromVector(dScale) );
//				phyBody->setUserPointer((void*)model);
//			}
//			else
//			{
//				model->SetModelMatrix( modelMatrix );				
//			}
//
//			scene->AddModel(model);				
//		}					
//	}
//
//	if (skinningMgr) {
//		skinningMgr->printBones();
//
//		// Check animations and get all the node data.
//		UINT numAnimations = aiscene->HasAnimations() ? aiscene->mNumAnimations : 0;
//		fprintf(stderr, "\n----\n#Animations: %d \n----\n", numAnimations);
//		//XMStoreFloat4x4A(&skinningMgr->globalInverseTransformationRef(), XMMatrixInverse(NULL, XMMATRIX(aiscene->mRootNode->mTransformation[0])));
//		skinningMgr->globalInverseTransformationRef()= XMMatrixInverse(NULL, XMMATRIX(aiscene->mRootNode->mTransformation[0]));
//
//		//m_globalInverseTransformation = glm::inverse(aiMatrix4x4ToMat4(scene->mRootNode->mTransformation));	
//		//
//		skinningMgr->getRootNodesRef().resize(numAnimations, NULL);
//		skinningMgr->getAnimationsRef().resize(numAnimations, NULL);
//
//		// Load and store animation data
//		for (UINT i = 0; i < numAnimations; ++i) {
//			aiAnimation* assimpAnimation = aiscene->mAnimations[i];
//			skinningMgr->getAnimationsRef()[i] = new Animation();
//			skinningMgr->getAnimationsRef()[i]->setNumChannels(assimpAnimation->mNumChannels);
//			skinningMgr->getAnimationsRef()[i]->setDuration((float)assimpAnimation->mDuration);
//			skinningMgr->getAnimationsRef()[i]->setTicksPerSecond((float)assimpAnimation->mTicksPerSecond);
//
//			for (UINT j = 0; j < assimpAnimation->mNumChannels; ++j) {
//				aiNodeAnim* assimpNodeAnimation = assimpAnimation->mChannels[j];				
//				NodeAnimation*& nodeAnimation = skinningMgr->getAnimationsRef()[i]->getChannelRef(j);
//				nodeAnimation = new NodeAnimation(std::string(assimpNodeAnimation->mNodeName.data));
//
//				UINT numRotations = assimpNodeAnimation->mNumRotationKeys;
//				UINT numTranslations = assimpNodeAnimation->mNumPositionKeys;
//				UINT numScalings = assimpNodeAnimation->mNumScalingKeys;
//
//				for (UINT k = 0; k < numRotations; ++k) {
//					XMFLOAT4A R(
//						assimpNodeAnimation->mRotationKeys[k].mValue.x,
//						assimpNodeAnimation->mRotationKeys[k].mValue.y,
//						assimpNodeAnimation->mRotationKeys[k].mValue.z,
//						assimpNodeAnimation->mRotationKeys[k].mValue.w);
//
//					nodeAnimation->addRotation(RotationKey((float)assimpNodeAnimation->mRotationKeys[k].mTime, R));
//				}
//
//				for (UINT k = 0; k < numTranslations; ++k) {
//					XMFLOAT3A T(
//						assimpNodeAnimation->mPositionKeys[k].mValue.x,
//						assimpNodeAnimation->mPositionKeys[k].mValue.y,
//						assimpNodeAnimation->mPositionKeys[k].mValue.z);
//
//					nodeAnimation->addTranslation(TranslationKey((float)assimpNodeAnimation->mPositionKeys[k].mTime, T));				
//				}
//
//				for (UINT k = 0; k < numScalings; ++k) {
//					XMFLOAT3A S(
//						assimpNodeAnimation->mScalingKeys[k].mValue.x,
//						assimpNodeAnimation->mScalingKeys[k].mValue.y,
//						assimpNodeAnimation->mScalingKeys[k].mValue.z);
//
//					nodeAnimation->addScaling(ScalingKey((float)assimpNodeAnimation->mScalingKeys[k].mTime, S));
//				}
//
//			}
//		}
//
//		// Traverse assimp node hierachy and copy it into the local datastructures
//		for (UINT i = 0; i < numAnimations; ++i) {
//			traverseAndStoreAssimpNodeHierachy(aiscene->mRootNode, skinningMgr->getRootNodesRef()[i], i, skinningMgr);
//		}
//
//		skinningMgr->setupBuffers();
//		scene->GetSkinningAnimationManagers().push_back(skinningMgr);
//	}
//
//	delete physicsLoader; 
//
//	return hr;
//}