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
#pragma once

#ifndef D3D11_OBJECTORIENTED_BOUNDING_BOX
#define D3D11_OBJECTORIENTED_BOUNDING_BOX

#include <DXUT.h>
#include <Eigen/Eigen>

static const DirectX::XMVECTORF32 g_vBBFLTMIN = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };

class DXObjectOrientedBoundingBox  {
public:
	DXObjectOrientedBoundingBox()
	{
		SetInvalid();
	}

	DXObjectOrientedBoundingBox(const DirectX::XMFLOAT3* points, unsigned int numPoints, const DXObjectOrientedBoundingBox& other)
	{
		SetAxisScaled(0,DirectX::XMVector3Normalize(other.GetAxisScaled(0)));
		SetAxisScaled(1,DirectX::XMVector3Normalize(other.GetAxisScaled(1)));
		SetAxisScaled(2,DirectX::XMVector3Normalize(other.GetAxisScaled(2)));

		ComputeAnchorAndExtentsForGivenNormalizedAxis(points, numPoints);
	}

	~DXObjectOrientedBoundingBox(){}

	DXObjectOrientedBoundingBox operator* (const DirectX::XMMATRIX& mat)
	{
		return Transformed(mat);
	}

	DXObjectOrientedBoundingBox& operator*= (const DirectX::XMMATRIX &mat)
	{
		TransformInPlace(mat);
		return *this;
	}

	DXObjectOrientedBoundingBox& TransformInPlace(const DirectX::XMMATRIX& mat)
	{
		//SetAnchor(DirectX::XMVector3TransformCoord(GetAnchor(), mat));
		//SetAxisScaled(0, DirectX::XMVector3TransformNormal(GetAxisScaled(0), mat));
		//SetAxisScaled(1, DirectX::XMVector3TransformNormal(GetAxisScaled(1), mat));
		//SetAxisScaled(2, DirectX::XMVector3TransformNormal(GetAxisScaled(2), mat));

		m_matObb2World = XMMatrixMultiply(m_matObb2World, mat);
		return *this;
	}

	DXObjectOrientedBoundingBox Transformed(const DirectX::XMMATRIX &mat) const
	{
		DXObjectOrientedBoundingBox result(*this);
		return result.TransformInPlace(mat);
	}

	void GetTransformedRotateAxesOnly(const DirectX::XMMATRIX& mat, DXObjectOrientedBoundingBox& result) const
	{
		//result.m_anchor = m_anchor;

		//result.m_matWorld2OBB = XMMatrixMultiply(m_matWorld2OBB, mat);
		result.SetAnchor(GetAnchor());
			
		result.SetAxisScaled(0,DirectX::XMVector3TransformNormal(GetAxisScaled(0), mat));
		result.SetAxisScaled(1,DirectX::XMVector3TransformNormal(GetAxisScaled(1), mat));
		result.SetAxisScaled(2,DirectX::XMVector3TransformNormal(GetAxisScaled(2), mat));
	}

	DirectX::XMVECTOR getCenter() const {
		using namespace DirectX;
		//return m_anchor + DirectX::XMVectorScale((m_axesScaled[0] + m_axesScaled[1] + m_axesScaled[2]), 0.5);
		return GetAnchor() + DirectX::XMVectorScale((GetAxisScaled(0) + GetAxisScaled(1) + GetAxisScaled(2)), 0.5);
	}

	DXObjectOrientedBoundingBox &scale(const float scale) {
		using namespace DirectX;
		DirectX::XMVECTOR center = getCenter();
		SetAxisScaled(0, scale * GetAxisScaled(0));
		SetAxisScaled(1, scale * GetAxisScaled(1));
		SetAxisScaled(2, scale * GetAxisScaled(2));
		
		//m_anchor = DirectX::XMVectorSubtract(center, DirectX::XMVectorScale((m_axesScaled[0] + m_axesScaled[1] + m_axesScaled[2]), 0.5));
		SetAnchor(DirectX::XMVectorSubtract(center, DirectX::XMVectorScale((GetAxisScaled(0) + GetAxisScaled(1) + GetAxisScaled(2)), 0.5)));
		return *this;
	}

	void ComputeAnchorAndExtentsForGivenNormalizedAxis(const DirectX::XMFLOAT3* points, unsigned int numPoints);


	__forceinline void Transform(const DirectX::XMMATRIX& mModel, DXObjectOrientedBoundingBox& obbToTransform) const
	{
		obbToTransform = Transformed(mModel);
	}

	// TODO
	////! returns the matrix that transforms from OOBB-unit-cube-space to world space
	//__forceinline DirectX::XMMATRIX getOOBBToWorld() const 
	//{
	//	//const mat4f& tmp = ObjectOrientedBoundingBox::getOOBBToWorld();		
	//	//DirectX::XMMATRIX res = DirectX::XMMatrixTranspose(XMLoadFloat4x4((DirectX::XMFLOAT4X4*)&tmp));		
	//	//return res;
	//	Eigen::Vector3f axesScaled[3];
	//	DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&axesScaled[0], m_axesScaled[0]);
	//	DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&axesScaled[1], m_axesScaled[1]);
	//	DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&axesScaled[2], m_axesScaled[2]);

	//	//assert((axesScaled[0] | axesScaled[1]) < 0.001);
	//	//assert((axesScaled[1] | axesScaled[2]) < 0.001);
	//	//assert((axesScaled[2] | axesScaled[0]) < 0.001);

	//	Eigen::Matrix3f tmpWorld2OBB;	tmpWorld2OBB << axesScaled[0], axesScaled[1] << axesScaled[2];
	//	DirectX::XMMATRIX world2OBB = XMLoadFloat3x3((DirectX::XMFLOAT3X3*)&tmpWorld2OBB);



	//}

	//! returns the matrix that transforms from word space to OOBB-unit-cube-space
	DirectX::XMMATRIX getWorldToOOBB() const;

	void setAnchorAndScaledAxes(const DirectX::XMFLOAT3* anchor, const DirectX::XMFLOAT3* a0, const DirectX::XMFLOAT3* a1, const DirectX::XMFLOAT3* a2) {
		//m_anchor = DirectX::XMLoadFloat3(anchor);
		SetAnchor(DirectX::XMLoadFloat3(anchor));
		SetAxisScaled(0, DirectX::XMLoadFloat3(a0));
		SetAxisScaled(1, DirectX::XMLoadFloat3(a1));
		SetAxisScaled(2, DirectX::XMLoadFloat3(a2));

		//m_Anchor = anchor;
		//m_AxesScaled[0] = a0;
		//m_AxesScaled[1] = a1;
		//m_AxesScaled[2] = a2;

	}

	////! returns the matrix that transforms from world space to OOBB: [0;extentX]x[0;extentY]x[0;extentZ]
	//__forceinline DirectX::XMMATRIX getWorldToOOBBNormalized() const 
	//{
	//	const mat4f& tmp = ObjectOrientedBoundingBox::getWorldToOOBBNormalized();		
	//	DirectX::XMMATRIX res = DirectX::XMMatrixTranspose(XMLoadFloat4x4((DirectX::XMFLOAT4X4*)&tmp));

	//	return res;
	//}

	////! returns true if all points are contained by the bounding box (eps tends to return true)
	//bool contains(const DirectX::XMFLOAT3* points, unsigned int numPoints, float eps = (float)0.00001) const 
	//{
	//	return ObjectOrientedBoundingBox::contains((vec3f*)points, numPoints, eps);
	//}

	////! returns true if all points are outside of the bounding box (eps tends to return true)
	//bool outside(const DirectX::XMFLOAT3* points, unsigned int numPoints, float eps = (float)0.00001) const 
	//{
	//	return ObjectOrientedBoundingBox::outside((vec3f*)points, numPoints, eps);
	//}

	DirectX::XMFLOAT3 GetExtent() const 
	{		
		//return point3d<FloatType>(m_AxesScaled[0].length(), m_AxesScaled[1].length(), m_AxesScaled[2].length());
		//vec3f ext = ObjectOrientedBoundingBox::getExtent();
		//return DirectX::XMFLOAT3(ext.x, ext.y, ext.z);

		return DirectX::XMFLOAT3(DirectX::XMVectorGetX(DirectX::XMVector3Length(GetAxisScaled(0))),
								 DirectX::XMVectorGetX(DirectX::XMVector3Length(GetAxisScaled(1))),
								 DirectX::XMVectorGetX(DirectX::XMVector3Length(GetAxisScaled(2))));
	}

	//DirectX::XMFLOAT3 getCenter() const {
	//	vec3f center = ObjectOrientedBoundingBox::getCenter();
	//	return DirectX::XMFLOAT3(center.x, center.y, center.z);
	//}

	void GetCornerPoints(DirectX::XMFLOAT3* points) const 
	{
		using namespace DirectX;
		DirectX::XMStoreFloat3(&points[0], GetAnchor());
		DirectX::XMStoreFloat3(&points[1], GetAnchor() + GetAxisScaled(0));
		DirectX::XMStoreFloat3(&points[2], GetAnchor() + GetAxisScaled(0) + GetAxisScaled(1));
		DirectX::XMStoreFloat3(&points[3], GetAnchor() + GetAxisScaled(1));
								  
		DirectX::XMStoreFloat3(&points[4], GetAnchor() + GetAxisScaled(2));
		DirectX::XMStoreFloat3(&points[5], GetAnchor() + GetAxisScaled(0) + GetAxisScaled(2));
		DirectX::XMStoreFloat3(&points[6], GetAnchor() + GetAxisScaled(0) + GetAxisScaled(1) + GetAxisScaled(2));
		DirectX::XMStoreFloat3(&points[7], GetAnchor() + GetAxisScaled(1) + GetAxisScaled(2));
	}
	
	//! returns the four corner pointds of the z = 0; front plane
	void GetFaceZFront(DirectX::XMFLOAT3* points) const 
	{
		using namespace DirectX;
		DirectX::XMStoreFloat3(&points[0], GetAnchor());
		DirectX::XMStoreFloat3(&points[1], GetAnchor() + GetAxisScaled(0));
		DirectX::XMStoreFloat3(&points[2], GetAnchor() + GetAxisScaled(0) + GetAxisScaled(1));
		DirectX::XMStoreFloat3(&points[3], GetAnchor() + GetAxisScaled(1));
	}

	//! returns the four corner pointds of the z = 1; back plane
	void GetFaceZBack(DirectX::XMFLOAT3* points) const 
	{
		using namespace DirectX;
		DirectX::XMStoreFloat3(&points[0], GetAnchor() + GetAxisScaled(2));
		DirectX::XMStoreFloat3(&points[1], GetAnchor() + GetAxisScaled(0) + GetAxisScaled(2));
		DirectX::XMStoreFloat3(&points[2], GetAnchor() + GetAxisScaled(0) + GetAxisScaled(1) + GetAxisScaled(2));
		DirectX::XMStoreFloat3(&points[3], GetAnchor() + GetAxisScaled(1) + GetAxisScaled(2));

	}

	//! tests whether a plane (consists of 4 points is outside of the bounding box or not
	bool TestFace(DirectX::XMFLOAT3* points, float eps = (float)0.00001) const;


	void GetObjectVerticesAndIndices(std::vector<DirectX::XMFLOAT3>& verts, std::vector<UINT>& indices) const
	{

		verts.clear();
		indices.clear();		

		//if the bounding box is not valid return no vertices/indices/normals
		if (!IsValid())	return;


		verts.resize(8);
		indices.resize(24);
		GetCornerPoints(&verts[0]);
		GetEdgeIndices(&indices[0]);
	}

	bool IsValid() const
	{		
		return DirectX::XMVector3NotEqual(GetAnchor(), g_vBBFLTMIN);
	}


	void ComputeFromPCA(const std::vector<DirectX::XMFLOAT3>& points)
	{
		if (points.size() < 4)
			SetInvalid();
		else
			ComputeFromPCA(&points[0], static_cast<unsigned int>(points.size()));
	}

	void ComputeFromPCA(const DirectX::XMFLOAT3* pointsDX, unsigned int numPoints);
	

private:
	void SetInvalid()
	{		
		SetAnchor(g_vBBFLTMIN);
	}

	void GetEdgeIndices(unsigned int* edgeIndexList) const {
		//floor
		edgeIndexList[0] = 0;
		edgeIndexList[1] = 1;

		edgeIndexList[2] = 1;
		edgeIndexList[3] = 2;

		edgeIndexList[4] = 2;
		edgeIndexList[5] = 3;

		edgeIndexList[6] = 3;
		edgeIndexList[7] = 0;

		//verticals
		edgeIndexList[8] = 0;
		edgeIndexList[9] = 4;

		edgeIndexList[10] = 1;
		edgeIndexList[11] = 5;

		edgeIndexList[12] = 2;
		edgeIndexList[13] = 6;

		edgeIndexList[14] = 3;
		edgeIndexList[15] = 7;

		//ceiling
		edgeIndexList[16] = 4;
		edgeIndexList[17] = 5;

		edgeIndexList[18] = 5;
		edgeIndexList[19] = 6;

		edgeIndexList[20] = 6;
		edgeIndexList[21] = 7;

		edgeIndexList[22] = 7;
		edgeIndexList[23] = 4;
	}


	const DirectX::XMVECTOR& GetAxisScaled(int i) const
	{
		return m_matObb2World.r[i];
	}

	void SetAxisScaled(int i, DirectX::XMVECTOR axis)
	{
		m_matObb2World.r[i] = axis;
	}

	const DirectX::XMVECTOR& GetAnchor() const
	{
		return m_matObb2World.r[3];
	}

	void SetAnchor(DirectX::XMVECTOR anchor)
	{
		m_matObb2World.r[3] = DirectX::XMVectorSetW(anchor, 1);
	}


	DirectX::XMMATRIX m_matObb2World; // axes scaled xyz, anchor

};


#endif