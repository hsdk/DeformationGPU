#pragma once

#include <DXUT.h>
#include <string>

// fwd decls
struct DXMaterial;
struct MeshData;
struct SubMeshData;

//#include "DXMaterial.h"
//#include "MovableObject.h"
//#include "Voxelization.h"
#include <SDX/DXObjectOrientedBoundingBox.h>
#include "dynamics/SkinningAnimation.h"

class TriangleSubmesh
{
public:
	TriangleSubmesh();
	~TriangleSubmesh();

	HRESULT Create(ID3D11Device1* pd3dDevice, const SubMeshData* data, const std::vector<DirectX::XMFLOAT4A>& vertices);
	void Destroy();

		    DXMaterial* GetMaterial()       { return _material; }
	const	DXMaterial* GetMaterial() const { return _material; }
	const	DXObjectOrientedBoundingBox& GetModelOBB() const { return m_obbObject; }
			DXObjectOrientedBoundingBox& GetModelOBB() { return m_obbObject; }

	UINT _baseVertex;
	UINT _numVertices;
	UINT _numFaces;
	ID3D11Buffer* _indexBuffer;

	// todo move to better place
	DXObjectOrientedBoundingBox m_obbObject;	
	
	void SetName(const std::string& name) { m_name =  name;}
	std::string GetName() const { return m_name;}
protected:

	std::string m_name;
	DXMaterial* _material;
};

struct DXModel// : public MovableObject,  Voxelizable
{
	DXModel();
	~DXModel();

	//HRESULT Create(	ID3D11Device1* pd3dDevice, 
	//				const std::vector<DirectX::XMFLOAT4A>& vertices,
	//				const std::vector<DirectX::XMUINT3>& indices,			// CHECKME
	//				const std::vector<DirectX::XMFLOAT3A>& normals,
	//				const std::vector<DirectX::XMFLOAT2A>& texcoords);

	HRESULT Create(	ID3D11Device1* pd3dDevice, 
					MeshData* meshData);
											
	// transforms object space obb with model matrix and writes result to obbWorldResult
	void UpdateOBBs(const DirectX::XMMATRIX& modelMatrix, DXObjectOrientedBoundingBox& obbWorldResult);


	//void SetMaterial(DXMaterial* mat)						{	_material		= mat;			}	
	//__forceinline const DXMaterial*			  GetMaterial()		const	{	return _material;		}

	ID3D11Buffer*				GetVertexBuffer()				const	{	return _vertexBuffer;						}
	ID3D11UnorderedAccessView*	GetVertexBufferUAV()			const	{	return g_pVertexBufferUAV;					}	
	ID3D11ShaderResourceView*	GetVertexBufferSRV()			const	{	return g_pVertexBufferSRV;					}
	ID3D11ShaderResourceView*	GetVertexBufferSRV4Components()const	{	return g_pVertexBufferSRV4Components;		}


	ID3D11Buffer*				GetNormalsBuffer()				const	{	return _normalsBuffer;						}
	ID3D11UnorderedAccessView*	GetNormalsBufferUAV()			const	{	return g_pNormalsBufferUAV;					}	
	ID3D11ShaderResourceView*	GetNormalsBufferSRV()			const	{	return g_pNormalsBufferSRV;					}
	ID3D11ShaderResourceView*	GetNormalsBufferSRV4Components()const	{	return g_pNormalsBufferSRV4Components;		}


	//bool IsSkinned() { return m_skinningMeshAnimationManager != NULL;}

	ID3D11Buffer*		 _vertexBuffer;
	ID3D11Buffer*		 _normalsBuffer;
	ID3D11Buffer*		 _texcoordBuffer;

	std::vector<TriangleSubmesh> submeshes;
	

	//---- begin skinning stuff

	ID3D11ShaderResourceView*		g_pVertexBufferSRV;
	ID3D11ShaderResourceView*		g_pVertexBufferSRV4Components;
	ID3D11UnorderedAccessView*		g_pVertexBufferUAV;

	ID3D11Buffer*					g_pVertexBufferBasePose;
	ID3D11ShaderResourceView*		g_pVertexBufferBasePoseSRV;
	ID3D11ShaderResourceView*		g_pVertexBufferBasePoseSRV4Components;

	ID3D11ShaderResourceView*		g_pNormalsBufferSRV;
	ID3D11ShaderResourceView*		g_pNormalsBufferSRV4Components;
	ID3D11UnorderedAccessView*		g_pNormalsBufferUAV;

	ID3D11Buffer*					g_pNormalsBufferBasePose;
	ID3D11ShaderResourceView*		g_pNormalsBufferBasePoseSRV;
	ID3D11ShaderResourceView*		g_pNormalsBufferBasePoseSRV4Components;

	UINT							_numVertices;
	
	//SkinningAnimationClasses::SkinningMeshAnimationManager* m_skinningMeshAnimationManager;
	//---- end skinning stuff		
};