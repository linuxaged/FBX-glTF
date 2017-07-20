//
// Copyright (c) Autodesk, Inc. All rights reserved 
//
// C++ glTF FBX importer/exporter plug-in
// by Cyrille Fauvel - Autodesk Developer Network (ADN)
// January 2015
//
// Permission to use, copy, modify, and distribute this software in
// object code form for any purpose and without fee is hereby granted, 
// provided that the above copyright notice appears in all copies and 
// that both that copyright notice and the limited warranty and
// restricted rights notice below appear in all supporting 
// documentation.
//
// AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS. 
// AUTODESK SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTY OF
// MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE.  AUTODESK, INC. 
// DOES NOT WARRANT THAT THE OPERATION OF THE PROGRAM WILL BE
// UNINTERRUPTED OR ERROR FREE.
//
#include "StdAfx.h"
#include "gltfWriter.h"
#include "gltfwriterVBO.h"
#include <string.h> // for memcmp

namespace _IOglTF_NS_ {

//-----------------------------------------------------------------------------
web::json::value gltfWriter::WriteMesh (FbxNode *pNode) {
	web::json::value meshDef =web::json::value::object () ;
	meshDef [U("name")] =web::json::value::string (nodeId (pNode, true)) ;

	//if ( _json [U("meshes")].has_field (meshDef [U("name")].as_string ()) ) {
	if ( isKnownId (pNode->GetNodeAttribute ()->GetUniqueID ()) ) {
		// The mesh/material/... were already exported, create only the transform node
		web::json::value node =WriteNode (pNode) ;
		web::json::value ret =web::json::value::object ({ { U("nodes"), node } }) ;
		return (ret) ;
	}

	web::json::value meshPrimitives =web::json::value::array () ;
	web::json::value accessorsAndBufferViews =web::json::value::object ({
		{ U("accessors"), web::json::value::object () },
		{ U("bufferViews"), web::json::value::object () }
	}) ;
	web::json::value materials =web::json::value::object ({ { U("materials"), web::json::value::object () } }) ;
	web::json::value techniques =web::json::value::object ({ { U("techniques"), web::json::value::object () } }) ;
	web::json::value programs =web::json::value::object ({ { U("programs"), web::json::value::object () } }) ;
	web::json::value images =web::json::value::object ({ { U("images"), web::json::value::object () } }) ;
	web::json::value samplers =web::json::value::object ({ { U("samplers"), web::json::value::object () } }) ;
	web::json::value textures =web::json::value::object ({ { U("textures"), web::json::value::object () } }) ;

	FbxMesh *pMesh =pNode->GetMesh () ; //FbxCast<FbxMesh>(pNode->GetNodeAttribute ()) ;
	pMesh->ComputeBBox () ;

	// FBX Layers works like Photoshop layers
	// - Vertices, Polygons and Edges are not on layers, but every other elements can
	// - Usually Normals are on Layer 0 (FBX default) but can eventually be on other layers
	// - UV, vertex colors, etc... can have multiple layer representation or be on different layers,
	//   the code below will try to retrieve each element recursevily to merge all elements from 
	//   different layer.

	// Ensure we covered all layer elements of layer 0
	// - Normals (all layers... does GLTF support this?)
	// - UVs on all layers
	// - Vertex Colors on all layers.
	// - Materials and Textures are covered when the mesh is exported.
	// - Warnings for unsupported layer element types: polygon groups, undefined

	int nbLayers =pMesh->GetLayerCount () ;
	web::json::value primitive=web::json::value::object ({
		{ U("attributes"), web::json::value::object () },
		{ U("mode"), IOglTF::TRIANGLES } // Allowed values are 0 (POINTS), 1 (LINES), 2 (LINE_LOOP), 3 (LINE_STRIP), 4 (TRIANGLES), 5 (TRIANGLE_STRIP), and 6 (TRIANGLE_FAN).
	}) ;

////web::json::value elts =web::json::value::object ({
////	{ U("normals"), web::json::value::object () },
////	{ U("uvs"), web::json::value::object () },
////	{ U("colors"), web::json::value::object () }
////}) ;
	web::json::value localAccessorsAndBufferViews =web::json::value::object () ;
	std::vector<utility::string_t> jointNames;
	if (pMesh->GetDeformerCount(FbxDeformer::eSkin))
	{
		assert(_skinJointNames.is_array());
		if (_skinJointNames.is_array() == false)
		{
			MessageBox(NULL, L"There is no joint in the skins node!", L"_jointNames is NULL", MB_OK);
		}
		web::json::array joints = _skinJointNames.as_array();
		for (int i = 0; i < joints.size(); i++)
		{
			if (joints[i].is_string())
			{
				utility::string_t temp = joints[i].as_string();
				jointNames.push_back( temp );
			}
			//else
			//{
			//	jointNames.push_back(U("UNKNOWN"));
			//}
		}
	}
	

	gltfwriterVBO vbo (pMesh) ;
	if (pMesh->GetDeformerCount(FbxDeformer::eSkin))
	{
		assert(!jointNames.empty());
		vbo.setJointNames(jointNames);
	}
	
	vbo.GetLayerElements (true) ;
	vbo.indexVBO () ;

	std::vector<unsigned short> out_indices =vbo.getIndices () ;
	std::vector<FbxDouble3> out_positions =vbo.getPositions () ;
	std::vector<FbxDouble3> out_normals =vbo.getNormals () ;
	std::vector<FbxDouble2> out_uvs =vbo.getUvs () ;
	std::vector<FbxDouble3> out_tangents =vbo.getTangents () ;
	std::vector<FbxDouble3> out_binormals =vbo.getBinormals () ;
	std::vector<FbxColor> out_vcolors =vbo.getVertexColors () ;

	_uvSets =vbo.getUvSets () ;

	//skin
	if (pMesh->GetDeformerCount(FbxDeformer::eSkin)) {
		std::vector<FbxDouble4> out_joints = vbo.getJoints();
		std::vector<FbxDouble4> out_weights = vbo.getWeights();
		std::vector<FbxAMatrix> inverseBindMatrices = vbo.getInverseBindMatrices();

		web::json::value skins = web::json::value::object ();
		FbxAMatrix globalPosition;
		web::json::value shapeMat;
		for (int row=0; row<4; row++) 
			for(int col=0;col<4;col++) 
				shapeMat[shapeMat.size()] = web::json::value::number((int)globalPosition.Get(row,col));

		utility::string_t skinName;
		skinName  = utility::conversions::to_string_t (nodeId(pMesh->GetNode()));
		skinName += utility::conversions::to_string_t (U("_skin")) ;
		skins[skinName][U("bindShapeMatrix")] = shapeMat;
		skins[skinName][U("inverseBindMatrices")] = WriteSkinArray(pMesh->GetNode(), inverseBindMatrices, 1, U("skin"));
		
		skins[skinName][U("jointNames")] = _skinJointNames;
		_json[U("skins")] = skins;

		web::json::value vertexJoints =WriteArrayWithMinMax<FbxDouble4, float> (out_joints, pMesh->GetNode (), U("_Joints")) ;
		MergeJsonObjects (localAccessorsAndBufferViews, vertexJoints);
		primitive [U("attributes")] [U("JOINT")] =web::json::value::string (GetJsonFirstKey (vertexJoints [U("accessors")])) ;

		web::json::value vertexWeights =WriteArrayWithMinMax<FbxDouble4, float> (out_weights, pMesh->GetNode (), U("_Weights")) ;
		MergeJsonObjects (localAccessorsAndBufferViews, vertexWeights);
		primitive [U("attributes")] [U("WEIGHT")] =web::json::value::string (GetJsonFirstKey (vertexWeights [U("accessors")])) ;
	}

	web::json::value vertex =WriteArrayWithMinMax<FbxDouble3, float> (out_positions, pMesh->GetNode (), U("_Positions")) ;
	MergeJsonObjects (localAccessorsAndBufferViews, vertex);
	primitive [U("attributes")] [U("POSITION")] =web::json::value::string (GetJsonFirstKey (vertex [U("accessors")])) ;

	if ( out_normals.size () ) {
		utility::string_t st (U("_Normals")) ;
		web::json::value ret =WriteArrayWithMinMax<FbxDouble3, float> (out_normals, pMesh->GetNode (), st.c_str ()) ;
		MergeJsonObjects (localAccessorsAndBufferViews, ret) ;
		st=U("NORMAL") ;
		primitive [U("attributes")] [st] =web::json::value::string (GetJsonFirstKey (ret [U("accessors")])) ;
	}

	if ( out_uvs.size () ) { // todo more than 1
		std::map<utility::string_t, utility::string_t>::iterator iter =_uvSets.begin () ;
		utility::string_t st (U("_") + iter->second) ;
		web::json::value ret =WriteArrayWithMinMax<FbxDouble2, float> (out_uvs, pMesh->GetNode (), st.c_str ()) ;
		MergeJsonObjects (localAccessorsAndBufferViews, ret) ;
		primitive [U("attributes")] [iter->second] =web::json::value::string (GetJsonFirstKey (ret [U("accessors")])) ;
	}

	int nb=(int)out_vcolors.size () ;
	if ( nb ) {
		std::vector<FbxDouble4> vertexColors_ (nb);
		for ( int i =0 ; i < nb ; i++ )
			vertexColors_.push_back (FbxDouble4 (out_vcolors [i].mRed, out_vcolors [i].mGreen, out_vcolors [i].mBlue, out_vcolors [i].mAlpha)) ;
		utility::string_t st (U("_Colors0")) ;
		web::json::value ret =WriteArrayWithMinMax<FbxDouble4, float> (vertexColors_, pMesh->GetNode (), st.c_str ()) ;
		MergeJsonObjects (localAccessorsAndBufferViews, ret) ;
		st =U("COLOR_0") ;
		primitive [U("attributes")] [st] =web::json::value::string (GetJsonFirstKey (ret [U("accessors")])) ;
	}

	// Get mesh face indices
	web::json::value polygons =WriteArray<unsigned short> (out_indices, 1, pMesh->GetNode (), U("_Polygons")) ;
	primitive [U("indices")] =web::json::value::string (GetJsonFirstKey (polygons [U("accessors")])) ;

	MergeJsonObjects (accessorsAndBufferViews, polygons) ;
	MergeJsonObjects (accessorsAndBufferViews, localAccessorsAndBufferViews) ;

	// Get material
	FbxLayer *pLayer =gltfwriterVBO::getLayer (pMesh, FbxLayerElement::eMaterial) ;
	if ( pLayer == nullptr ) {
		//ucout << U("Info: (") << utility::conversions::to_string_t (pNode->GetTypeName ())
		//	<< U(") ") << utility::conversions::to_string_t (pNode->GetName ())
		//	<< U(" no material on Layer: ")
		//	<< iLayer
		//	<< std::endl ;
		// Create default material
		web::json::value ret =WriteDefaultMaterial (pNode) ;
		if ( ret.is_string () ) {
			primitive [U("material")] =ret ;
		} else {
			primitive [U("material")] =web::json::value::string (GetJsonFirstKey (ret [U("materials")])) ;

			MergeJsonObjects (materials [U("materials")], ret [U("materials")]) ;

			utility::string_t techniqueName =GetJsonFirstKey (ret [U("techniques")]) ;
			web::json::value techniqueParameters =ret [U("techniques")] [techniqueName] [U("parameters")] ;

			AdditionalTechniqueParameters (pNode, techniqueParameters, out_normals.size () != 0, pMesh->GetDeformerCount(FbxDeformer::eSkin)) ;
			TechniqueParameters (pNode, techniqueParameters, primitive [U("attributes")], localAccessorsAndBufferViews [U("accessors")], false) ;
			ret =WriteTechnique (pNode, nullptr, techniqueParameters) ;
			//MergeJsonObjects (techniques, ret) ;
			techniques [U("techniques")] [techniqueName] =ret ;

			utility::string_t programName =ret [U("program")].as_string () ;
			web::json::value attributes =ret [U("attributes")] ;
			ret =WriteProgram (pNode, nullptr, programName, attributes) ;
			MergeJsonObjects (programs, ret) ;
		}
	} else {
		FbxLayerElementMaterial *pLayerElementMaterial =pLayer->GetMaterials () ;
		int materialCount =pLayerElementMaterial ? pNode->GetMaterialCount () : 0 ;
		if ( materialCount > 1 ) {
			_ASSERTE( materialCount > 1 ) ;
			ucout << U("Warning: (") << utility::conversions::to_string_t (pNode->GetTypeName ())
				<< U(") ") << utility::conversions::to_string_t (pNode->GetName ())
				<< U(" got more than one material. glTF supports one material per primitive (FBX Layer).")
				<< std::endl ;
		}
		// TODO: need to be revisited when glTF will support more than one material per layer/primitive
		materialCount =materialCount == 0 ? 0 : 1 ;
		for ( int i =0 ; i < materialCount ; i++ ) {
			const char* tempNodeName = pNode->GetName();
			web::json::value ret =WriteMaterial (pNode, pNode->GetMaterial (i)) ;
			if ( ret.is_string () ) {
				primitive [U("material")] =ret ;
				continue ;
			}
			primitive [U("material")]=web::json::value::string (GetJsonFirstKey (ret [U("materials")])) ;

			MergeJsonObjects (materials [U("materials")], ret [U("materials")]) ;
			if ( ret.has_field (U("images")) )
				MergeJsonObjects (images [U("images")], ret [U("images")]) ;
			if ( ret.has_field (U("samplers")) )
				MergeJsonObjects (samplers [U("samplers")], ret [U("samplers")]) ;
			if ( ret.has_field (U("textures")) )
				MergeJsonObjects (textures [U("textures")], ret [U("textures")]) ;

			utility::string_t techniqueName =GetJsonFirstKey (ret [U("techniques")]) ;
			web::json::value techniqueParameters =ret [U("techniques")] [techniqueName] [U("parameters")] ;
			AdditionalTechniqueParameters (pNode, techniqueParameters, out_normals.size () != 0, pMesh->GetDeformerCount(FbxDeformer::eSkin)) ;
			TechniqueParameters (pNode, techniqueParameters, primitive [U("attributes")], localAccessorsAndBufferViews [U("accessors")]) ;
			ret =WriteTechnique (pNode, pNode->GetMaterial (i), techniqueParameters) ;
			//MergeJsonObjects (techniques, ret) ;
			techniques [U("techniques")] [techniqueName] =ret ;

			utility::string_t programName =ret [U("program")].as_string () ;
			web::json::value attributes =ret [U("attributes")] ;
			ret =WriteProgram (pNode, pNode->GetMaterial (i), programName, attributes) ;
			MergeJsonObjects (programs, ret) ;
		}
	}
	meshPrimitives [meshPrimitives.size ()] =primitive ;
	meshDef [U("primitives")] =meshPrimitives ;

	web::json::value lib =web::json::value::object ({ { nodeId (pNode, true, true), meshDef } }) ;

	//if ( pMesh->GetShapeCount () )
	//	WriteControllerShape (pMesh) ; // Create a controller
	web::json::value node =WriteNode (pNode) ;

	web::json::value ret =web::json::value::object ({ { U("meshes"), lib }, { U("nodes"), node } }) ;
	MergeJsonObjects (ret, accessorsAndBufferViews) ;
	MergeJsonObjects (ret, images) ;
	MergeJsonObjects (ret, materials) ;
	MergeJsonObjects (ret, techniques) ;
	MergeJsonObjects (ret, programs) ;
	MergeJsonObjects (ret, samplers) ;
	MergeJsonObjects (ret, textures) ;

	return (ret) ;
}

}
