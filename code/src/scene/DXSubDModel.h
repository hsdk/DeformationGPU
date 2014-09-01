#pragma once
#include <DXUT.h>

#include <osd/error.h>
#include <osd/vertex.h>
#include <osd/d3d11VertexBuffer.h>
#include <osd/d3d11ComputeContext.h>
#include <osd/d3d11ComputeController.h>
#include <osd/d3d11Mesh.h>

typedef OpenSubdiv::HbrMesh<OpenSubdiv::OsdVertex>     OsdHbrMesh;
typedef OpenSubdiv::HbrVertex<OpenSubdiv::OsdVertex>   OsdHbrVertex;
typedef OpenSubdiv::HbrFace<OpenSubdiv::OsdVertex>     OsdHbrFace;
typedef OpenSubdiv::HbrHalfedge<OpenSubdiv::OsdVertex> OsdHbrHalfedge;

#include <App.h>
#include <scene/DXMaterial.h>
#include <SDX/DXObjectOrientedBoundingBox.h>


class DXOSDMesh{
public:
	DXOSDMesh();
	~DXOSDMesh();

	HRESULT Create(	ID3D11Device1* pd3dDevice,
					OpenSubdiv::HbrMesh<OpenSubdiv::OsdVertex> *hbrMesh,
					const std::vector<DirectX::XMFLOAT4A>& verticesF4,
					bool hasTexcoords,
					bool isDeformable);
	void Destroy();

	void		SetName(const std::string name) { _name = name; }
	std::string GetName() const {return _name;}

	std::vector<SPtexNeighborData>& GetPtexNeighborDataREF() { return m_ptexNeighDataCPU; }

	void  SetMaterial(DXMaterial* material)		{	_material = material;	}
	const DXMaterial* GetMaterial() const		{	return _material;		}
		  DXMaterial* GetMaterial()				{	return _material;		}

	const	DXObjectOrientedBoundingBox& GetModelOBB() const { return _obb; }
	DXObjectOrientedBoundingBox& GetModelOBB()				{ return _obb; }

	bool GetRequiresOverlapUpdate() const {	return _bRequiresOverlapUpdate;	}
	void SetRequiresOverlapUpdate()		  {	_bRequiresOverlapUpdate = true;}

	OpenSubdiv::OsdD3D11MeshInterface* GetMesh() const { return _model; }
	int		GetNumVertexElements()		const	{ return _numOSDVertexElements;		}
	int		GetNumPTexFaces()			const	{ return _model->GetNumPTexFaces(); }
	UINT	GetNumExtraordinary()		const	{ return m_numExtraordinary;		}
	void	SetNumExtraordinary(UINT count)		{ m_numExtraordinary = count;		}

	ID3D11InputLayout** GetInputLayoutRef()		{ return &_inputLayout; }
	ID3D11InputLayout* GetInputLayout()		{ return _inputLayout; }
	const ID3D11InputLayout* GetInputLayout() const { return _inputLayout; }

	ID3D11ShaderResourceView* const GetPTexNeighborDataSRV()  const { return m_ptexNeighDataSRV; }
	ID3D11ShaderResourceView* const GetExtraordinaryInfoSRV() const { return m_extraordinaryInfoSRV; }
	ID3D11ShaderResourceView* const GetExtraordinaryDataSRV() const { return m_extraordinaryDataSRV; }
	bool GetHasTexcoords() const {return m_hasTexCoords;}
	


	std::vector<SExtraordinaryInfo>& GetExtraordinaryInfoCPURef() { return m_extraordinaryInfoCPU; }
	std::vector<SExtraordinaryData>& GetExtraordinaryDataCPURef() { return m_extraordinaryDataCPU; }

	ID3D11Buffer* const GetControlCageEdgeVertices() const { return m_cageEdgeVertices; }
	UINT GetNumCageEdgeVertices() const { return m_numCageEdgeVertices; }



protected:
	DXMaterial*				_material;	
	std::string				_name;
	ID3D11InputLayout*		_inputLayout;
	OpenSubdiv::OsdD3D11MeshInterface *_model;
	
	std::vector<SPtexNeighborData> m_ptexNeighDataCPU;
	int _numOSDVertexElements;

	// bounding volumes all in object space
	DirectX::XMFLOAT3A				_meshCenter;// center of mass object space
	float							_bsRadius;	// bounding sphere radius object space
	DirectX::XMFLOAT4A				_bbMax;		// axis aligned bounding box max in object space
	DirectX::XMFLOAT4A				_bbMin;		// axis aligned bounding box min in object space
	DXObjectOrientedBoundingBox	_obb;		// oriented bounding box in object space

	ID3D11Buffer					*m_ptexNeighDataBUF;
	ID3D11ShaderResourceView		*m_ptexNeighDataSRV;	

	std::vector<SExtraordinaryInfo>	m_extraordinaryInfoCPU;
	std::vector<SExtraordinaryData>	m_extraordinaryDataCPU;

	ID3D11Buffer					*m_extraordinaryInfoBUF;
	ID3D11ShaderResourceView		*m_extraordinaryInfoSRV;

	ID3D11Buffer					*m_extraordinaryDataBUF;
	ID3D11ShaderResourceView		*m_extraordinaryDataSRV;

	ID3D11Buffer					*m_cageEdgeVertices;
	//ID3D11ShaderResourceView		*m_cageEdgeVerticesSRV;

	bool m_hasTexCoords;

	UINT							 m_numExtraordinary;
	UINT							 m_numCageEdgeVertices;

	// ptex/tile stuff
	bool							_bRequiresOverlapUpdate;
};
