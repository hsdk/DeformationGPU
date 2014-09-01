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

#include "DXObjectOrientedBoundingBox.h"
#include <iostream>

using namespace DirectX;

DirectX::XMMATRIX DXObjectOrientedBoundingBox::getWorldToOOBB() const {	
	DirectX::XMMATRIX world2OBB2 = XMMatrixInverse(NULL, m_matObb2World);
	return world2OBB2;

	////  variant 2
	//Eigen::Vector3f axesScaled[3];
	//DirectX::XMStoreFloat3((XMFLOAT3*)&axesScaled[0], m_axesScaled[0] / scaleValues[0]);
	//DirectX::XMStoreFloat3((XMFLOAT3*)&axesScaled[1], m_axesScaled[1] / scaleValues[1]);
	//DirectX::XMStoreFloat3((XMFLOAT3*)&axesScaled[2], m_axesScaled[2] / scaleValues[2]);

	//Eigen::Matrix3f tmpWorld2OBB3x3;	
	////tmpWorld2OBB3x3 << axesScaled[0],
	////				axesScaled[1],
	////				axesScaled[2];

	//tmpWorld2OBB3x3.col(0) = axesScaled[0];
	//tmpWorld2OBB3x3.col(1) = axesScaled[1];
	//tmpWorld2OBB3x3.col(2) = axesScaled[2];

	//tmpWorld2OBB3x3(0, 0) /= scaleValues[0];	tmpWorld2OBB3x3(0, 1) /= scaleValues[0]; tmpWorld2OBB3x3(0, 2) /= scaleValues[0];
	//tmpWorld2OBB3x3(1, 0) /= scaleValues[1];	tmpWorld2OBB3x3(1, 1) /= scaleValues[1]; tmpWorld2OBB3x3(1, 2) /= scaleValues[1];
	//tmpWorld2OBB3x3(2, 0) /= scaleValues[2];	tmpWorld2OBB3x3(2, 1) /= scaleValues[2]; tmpWorld2OBB3x3(2, 2) /= scaleValues[2];

	//Eigen::Vector3f anchor;
	////DirectX::XMStoreFloat3((XMFLOAT3*)&anchor, m_anchor);
	//DirectX::XMStoreFloat3((XMFLOAT3*)&anchor, GetAnchor());
	//Eigen::Vector3f trans = tmpWorld2OBB3x3.transpose() * -anchor;

	//	
	//Eigen::Matrix4f tmpWorld2OBB4x4;// = tmpWorld2OBB3x3;
	//tmpWorld2OBB4x4.setZero();
	//tmpWorld2OBB4x4.block(0,0,3,3)<< tmpWorld2OBB3x3.transpose();
	//tmpWorld2OBB4x4(0, 3) = trans.x();
	//tmpWorld2OBB4x4(1, 3) = trans.y();
	//tmpWorld2OBB4x4(2, 3) = trans.z();
	//tmpWorld2OBB4x4(3, 3) = 1;
	//

	//DirectX::XMMATRIX world2OBB = XMLoadFloat4x4((DirectX::XMFLOAT4X4*)&tmpWorld2OBB4x4);
	////world2OBB = XMMatrixTranspose(world2OBB);
	//return world2OBB;
	////Matrix3x3<FloatType> scale(Matrix3x3<FloatType>::Scale, ((FloatType)1.0)/scaleValues[0], ((FloatType)1.0)/scaleValues[1], ((FloatType)1.0)/scaleValues[2]);
	////Matrix4x4<FloatType> worldToOOBB = scale * worldToOOBB3x3;
	////Matrix4x4<FloatType> trans = Matrix4x4<FloatType>(Matrix4x4<FloatType>::Translation, -m_Anchor);
	////return worldToOOBB * trans;

	////worldToOOBB3x3(0, 0) /= scaleValues[0];	worldToOOBB3x3(0, 1) /= scaleValues[0];	worldToOOBB3x3(0, 2) /= scaleValues[0];
	////worldToOOBB3x3(1, 0) /= scaleValues[1];	worldToOOBB3x3(1, 1) /= scaleValues[1];	worldToOOBB3x3(1, 2) /= scaleValues[1];
	////worldToOOBB3x3(2, 0) /= scaleValues[2];	worldToOOBB3x3(2, 1) /= scaleValues[2];	worldToOOBB3x3(2, 2) /= scaleValues[2];
	////
	////point3d<FloatType> trans = worldToOOBB3x3 * (-m_Anchor);
	////Matrix4x4<FloatType> worldToOOBB = worldToOOBB3x3;
	////worldToOOBB.at(0, 3) = trans.x;
	////worldToOOBB.at(1, 3) = trans.y;
	////worldToOOBB.at(2, 3) = trans.z;

	////return worldToOOBB;
}

void DXObjectOrientedBoundingBox::ComputeFromPCA(const DirectX::XMFLOAT3* pointsDX, unsigned int numPoints)
{

	if (numPoints < 4)
	{
		SetInvalid();
		return;
	}
	const Eigen::Vector3f* points = reinterpret_cast<const Eigen::Vector3f*>(pointsDX);

	//point3d<FloatType> mean(0.0, 0.0, 0.0);
	//Eigen::Vector3f mean;
	//
	//for (unsigned int i = 0; i < numPoints; i++) {
	//	mean += points[i];
	//}
	//mean /= static_cast<float>(numPoints);

	//Matrix3x3<FloatType> cov;
	//cov.setZero();

	//Eigen::Matrix3f cov;
	//cov.setZero();

	//for (unsigned int i = 0; i < numPoints; i++) {
	//	//point3d<FloatType> curr = points[i] - mean;
	//	//cov += Matrix3x3<FloatType>::tensorProduct(curr, curr);
	//	Eigen::Vector3f curr = points[i] - mean;
	//	SelfAdjointEigenSolver<Matrix3f> eig(cov);
	//	cov += Matrix3x3<FloatType>::tensorProduct(curr, curr);
	//}
	//cov /= (numPoints - 1);

	//FloatType lambda[3];
	////bool validEVs = cov.computeEigenvaluesAndEigenvectorsNR(lambda[0], lambda[1], lambda[2], m_AxesScaled[0], m_AxesScaled[1], m_AxesScaled[2]);
	////assert(validEVs);
	//cov.computeEigenvaluesAndEigenvectorsNR(lambda[0], lambda[1], lambda[2], m_AxesScaled[0], m_AxesScaled[1], m_AxesScaled[2]);

	/*Eigen::MatrixX3f mat2;
	mat2.resize()*/
	Eigen::MatrixXf mat(numPoints, 3);
	for (unsigned int i = 0; i < numPoints; i++) 
		mat.row(i) = points[i];
	
	Eigen::MatrixXf centered = mat.rowwise() - mat.colwise().mean();
	Eigen::Matrix3f cov = centered.adjoint() * centered;
	
	Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eig;
	eig.compute(cov);
	if (eig.info() != Eigen::Success)
	{
		std::cerr << "Eigen: matrix decomposition failed" << std::endl;	exit(1);
	}
	///eig.compute(cov);
	Eigen::Vector3f eValues(eig.eigenvalues());
	Eigen::Vector3f ev0(eig.eigenvectors().col(0));//*sqrt(eValues.x()));
	Eigen::Vector3f ev1(eig.eigenvectors().col(1));//*sqrt(eValues.y()));
	Eigen::Vector3f ev2(eig.eigenvectors().col(2));//*sqrt(eValues.z()));
	
	// sort descending, eigen stores EV starting with smalles Eigen value
	//m_axesScaled[2] = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&ev0));
	//m_axesScaled[1] = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&ev1));
	//m_axesScaled[0] = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&ev2));

	SetAxisScaled(2, DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&ev0));
	SetAxisScaled(1, DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&ev1));
	SetAxisScaled(0, DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&ev2));

	ComputeAnchorAndExtentsForGivenNormalizedAxis(pointsDX, numPoints);
}

void DXObjectOrientedBoundingBox::ComputeAnchorAndExtentsForGivenNormalizedAxis(const DirectX::XMFLOAT3* points, unsigned int numPoints)
{

	XMMATRIX world2OBB = m_matObb2World;
	world2OBB.r[3] = XMVectorZero();
	world2OBB = XMMatrixTranspose(world2OBB);
	XMMATRIX OBB2world = m_matObb2World;// XMMatrixTranspose(world2OBB);	//is an orthogonal matrix
	
	XMVECTOR minValues = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0);
	XMVECTOR maxValues = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0);// (-FLT_MAX, -FLT_MAX, -FLT_MAX);
	
	for (unsigned int i = 0; i < numPoints; i++)
	{
		XMVECTOR curr = XMVector3TransformNormal(XMLoadFloat3(&points[i]), world2OBB);
		minValues = XMVectorMin(minValues, curr);
		maxValues = XMVectorMax(maxValues, curr);
	}

	SetAnchor(XMVector3TransformNormal(minValues, OBB2world));

	XMVECTOR extent = XMVectorSubtract(maxValues, minValues);
	XMVECTOR eps = XMVectorSet(0.0001f, 0.0001f, 0.0001f, 0.0001f);	
	extent = XMVectorMax(extent, eps);

	//if bounding box has no extent; set invalid and return

	//if (DirectX::XMVectorGetX(extent) < (float)0.00001
	//	|| DirectX::XMVectorGetY(extent) < (float)0.00001
	//	|| DirectX::XMVectorGetZ(extent) < (float)0.00001)
	if (XMVector3Less(extent, eps))
	{
		SetInvalid();
		return;
	}

	//m_axesScaled[0] = DirectX::XMVectorGetX(extent) * m_axesScaled[0];
	//m_axesScaled[1] = DirectX::XMVectorGetY(extent) * m_axesScaled[1];
	//m_axesScaled[2] = DirectX::XMVectorGetZ(extent) * m_axesScaled[2];

	SetAxisScaled(0, DirectX::XMVectorGetX(extent) * GetAxisScaled(0));
	SetAxisScaled(1, DirectX::XMVectorGetY(extent) * GetAxisScaled(1));
	SetAxisScaled(2, DirectX::XMVectorGetZ(extent) * GetAxisScaled(2));
}


bool DXObjectOrientedBoundingBox::TestFace(DirectX::XMFLOAT3* points, float eps /*= (float)0.00001*/) const
{
	//return ObjectOrientedBoundingBox::testFace((vec3f*)points, eps);

	DirectX::XMVECTOR plane = DirectX::XMPlaneFromPoints(XMLoadFloat3(&points[0]), XMLoadFloat3(&points[1]), XMLoadFloat3(&points[2]));

	//Plane<FloatType> p(points);		
	//FloatType planeDist = planeDistance(p);
	XMFLOAT3 extent = GetExtent();

	//float r = 0.5 * XMVectorGetX(
	//	  extent.x*XMVectorAbs(XMPlaneDotNormal(plane, m_axesScaled[0] / extent.x))
	//	+ extent.y*XMVectorAbs(XMPlaneDotNormal(plane, m_axesScaled[1] / extent.y))
	//	+ extent.z*XMVectorAbs(XMPlaneDotNormal(plane, m_axesScaled[2] / extent.z)));
	
	float r = 0.5f * XMVectorGetX(
			extent.x*XMVectorAbs(XMPlaneDotNormal(plane, GetAxisScaled(0) / extent.x))
		+	extent.y*XMVectorAbs(XMPlaneDotNormal(plane, GetAxisScaled(1) / extent.y))
		+	extent.z*XMVectorAbs(XMPlaneDotNormal(plane, GetAxisScaled(2) / extent.z)));

	XMVECTOR center = getCenter();
	float s = std::abs(XMVectorGetX(XMPlaneDotCoord(plane, center)) - XMVectorGetW(center));
	//float planeDist = DirectX::XMVectorGetW(plane);

	float planeDist = r-s;

	if (planeDist <= -eps) {
		//Matrix4x4<FloatType> worldToOOBB = getWorldToOOBB();
		DirectX::XMMATRIX worldToOBB = getWorldToOOBB();
		DirectX::XMVECTOR pointsOBB[4];

		//point3d<FloatType> pointsOOBB[4];
		for (unsigned int i = 0; i < 4; i++)
		{
			//pointsOOBB[i] = worldToOOBB * points[i];
			pointsOBB[i] = DirectX::XMVector3Transform(XMLoadFloat3(&points[i]), worldToOBB);
		}
		DirectX::XMVECTOR vEps = DirectX::XMVectorSet(eps, eps, eps, 0);
		DirectX::XMVECTOR vOne = DirectX::XMVectorSet(1, 1, 1, 1);
		DirectX::XMVECTOR vOneMinusEps = DirectX::XMVectorSubtract(vOne, vEps);


		if (DirectX::XMVector3GreaterOrEqual(pointsOBB[0], vOneMinusEps)) return false;
		if (DirectX::XMVector3GreaterOrEqual(pointsOBB[1], vOneMinusEps)) return false;
		if (DirectX::XMVector3GreaterOrEqual(pointsOBB[2], vOneMinusEps)) return false;
		if (DirectX::XMVector3GreaterOrEqual(pointsOBB[3], vOneMinusEps)) return false;

		if (DirectX::XMVector3LessOrEqual(pointsOBB[0], vEps)) return false;
		if (DirectX::XMVector3LessOrEqual(pointsOBB[1], vEps)) return false;
		if (DirectX::XMVector3LessOrEqual(pointsOBB[2], vEps)) return false;
		if (DirectX::XMVector3LessOrEqual(pointsOBB[3], vEps)) return false;

		//if (pointsOOBB[0].x >= (FloatType)1.0 - eps && pointsOOBB[1].x >= (FloatType)1.0 - eps && pointsOOBB[2].x >= (FloatType)1.0 - eps && pointsOOBB[3].x >= (FloatType)1.0 - eps)	return false;
		//if (pointsOOBB[0].y >= (FloatType)1.0 - eps && pointsOOBB[1].y >= (FloatType)1.0 - eps && pointsOOBB[2].y >= (FloatType)1.0 - eps && pointsOOBB[3].y >= (FloatType)1.0 - eps)	return false;
		//if (pointsOOBB[0].z >= (FloatType)1.0 - eps && pointsOOBB[1].z >= (FloatType)1.0 - eps && pointsOOBB[2].z >= (FloatType)1.0 - eps && pointsOOBB[3].z >= (FloatType)1.0 - eps)	return false;
		//
		//if (pointsOOBB[0].x <= eps && pointsOOBB[1].x <= eps && pointsOOBB[2].x <= eps && pointsOOBB[3].x <= eps)	return false;
		//if (pointsOOBB[0].y <= eps && pointsOOBB[1].y <= eps && pointsOOBB[2].y <= eps && pointsOOBB[3].y <= eps)	return false;
		//if (pointsOOBB[0].z <= eps && pointsOOBB[1].z <= eps && pointsOOBB[2].z <= eps && pointsOOBB[3].z <= eps)	return false;

		return true;	//face intersects box
	}
	else {
		return false;	//outside
	}
}
