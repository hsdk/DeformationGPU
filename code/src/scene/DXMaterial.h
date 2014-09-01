#pragma once

#include <DXUT.h>
#include <string>


// fwd decls
struct ID3D11ShaderResourceView;


struct DXMaterial
{
	DXMaterial();
	~DXMaterial();

	HRESULT initCB();
	bool HasDiffuseTex()		const { return _hasDiffuseTex; }
	bool HasDisplacementTex()	const { return _hasDisplacementTex; }
	bool HasNormalsTex()		const { return _hasNormalsTex; }
	bool HasAlphaTex()			const { return _hasAlphaTex; }
	bool HasSpecularTex()		const { return _hasSpecularTex; }

	ID3D11ShaderResourceView*	const GetDiffuseTexSRV()		const {return _diffuseTexSRV; }
	ID3D11ShaderResourceView*	const GetNormalsTexSRV()		const {return _normalsTexSRV; }
	ID3D11ShaderResourceView*	const GetDisplacementTexSRV()	const {return _displTexSRV; }
	ID3D11ShaderResourceView*	const GetAlphaTexSRV()			const {return _alphaTexSRV; }
	ID3D11ShaderResourceView*	const GetSpecularTexSRV()		const {return _specularTexSRV; }

	const std::string&			GetDispTileFile()  const {return _dispTileFileName; }

	void SetDiffuseSRV(ID3D11ShaderResourceView* srv)
	{
		_diffuseTexSRV = srv;		
		if(srv != NULL)		_hasDiffuseTex = true;
	}

	void SetNormalsSRV(ID3D11ShaderResourceView* srv)
	{
		_normalsTexSRV = srv;		
		if(srv != NULL)		_hasNormalsTex = true;	
	}

	void SetDisplacementSRV(ID3D11ShaderResourceView* srv)
	{
		_displTexSRV = srv;		
		if(srv != NULL)		_hasDisplacementTex = true;		
	}

	void SetAlphaTexSRV(ID3D11ShaderResourceView* srv)
	{
		_alphaTexSRV = srv;		
		if(srv != NULL)		_hasAlphaTex = true;
	}

	void SetSpecularTexSRV(ID3D11ShaderResourceView* srv)
	{
		_specularTexSRV = srv;		
		if(srv != NULL)		_hasSpecularTex = true;
	}


	float GetDisplacementScalar() const { return _displacementScalar; }
		
	std::string					_name;
	DirectX::XMFLOAT3			_kAmbient;
	float						_specialMaterialFlag;
	DirectX::XMFLOAT3			_kDiffuse;
	DirectX::XMFLOAT3			_kSpecular;
	float						_shininess;
	float						_smoothness;
	ID3D11Buffer*				_cbMat;		
	
	bool						_hasDiffuseTex;	
	bool						_hasNormalsTex;
	bool						_hasDisplacementTex;
	bool						_hasAlphaTex;
	bool						_hasSpecularTex;

	ID3D11ShaderResourceView*	_diffuseTexSRV;
	ID3D11ShaderResourceView*	_normalsTexSRV;
	ID3D11ShaderResourceView*	_displTexSRV;
	ID3D11ShaderResourceView*	_alphaTexSRV;
	ID3D11ShaderResourceView*	_specularTexSRV;

	float						_displacementScalar;

	std::string					_dispTileFileName;


};