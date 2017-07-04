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
#include "glslShader.h"

namespace _IOglTF_NS_ {

bool gltfWriter::WriteShaders () {
	//web::json::value buffer =web::json::value::object () ;
	//FbxString filename =FbxPathUtils::GetFileName (utility::conversions::to_utf8string (_fileName).c_str (), false) ;
	//buffer [U("name")] =web::json::value::string (utility::conversions::to_string_t (filename.Buffer ())) ;

	//buffer [U("uri")] =web::json::value::string (utility::conversions::to_string_t ((filename + ".bin").Buffer ())) ;
	//// The Buffer file should be fully completed by now.
	//if ( GetIOSettings ()->GetBoolProp (IOSN_FBX_GLTF_EMBEDMEDIA, false) ) {
	//	// data:[<mime type>][;charset=<charset>][;base64],<encoded data>
	//	buffer [U("uri")] =web::json::value::string (IOglTF::dataURI (_bin)) ;
	//}

	//if ( _writeDefaults )
	//	buffer [U("type")] =web::json::value::string (U("arraybuffer")) ; ; // default is arraybuffer
	//buffer [U("byteLength")] =web::json::value::number ((int)_bin.tellg ()) ;

	//_json [U("buffers")] [utility::conversions::to_string_t (filename.Buffer ())] =buffer ;

	//web::json::value techs =_json [U("techniques")] ;
	//for ( auto iter =techs.as_object ().begin () ; iter != techs.as_object ().end () ; iter++ ) {
	//	glslTech test (iter->second) ;

	//	ucout << test.vertexShader ().source ()
	//		<< std::endl ;
	//}


	web::json::value materials =_json [U("materials")] ;
	for ( auto iter =materials.as_object ().begin () ; iter != materials.as_object ().end () ; iter++ ) {
		utility::string_t name =iter->first ;
		utility::string_t techniqueName =iter->second [U("technique")].as_string () ;
		glslTech tech (
			_json [U("techniques")] [techniqueName],
			iter->second [U("values")],
			_json
		) ;

		utility::string_t programName =_json [U("techniques")] [techniqueName] [U("program")].as_string () ;
		utility::string_t vsName =_json [U("programs")] [programName] [U("vertexShader")].as_string () ;
		utility::string_t fsName =_json [U("programs")] [programName] [U("fragmentShader")].as_string () ;
		
		FbxString gltfFilename (utility::conversions::to_utf8string (_fileName).c_str ()) ;
		
		if ( GetIOSettings ()->GetBoolProp (IOSN_FBX_GLTF_EMBEDMEDIA, false) ) {
			// data:[<mime type>][;charset=<charset>][;base64],<encoded data>
			_json [U("shaders")] [vsName] [U("uri")] =web::json::value::string (IOglTF::dataURI (tech.vertexShader ().source (),0)) ;
			_json [U("shaders")] [fsName] [U("uri")] =web::json::value::string (IOglTF::dataURI (tech.fragmentShader ().source (), 0)) ;
		} else {
			utility::string_t vsFilename =_json [U("shaders")] [vsName] [U("uri")].as_string () ;
			{
				FbxString shaderFilename (utility::conversions::to_utf8string (vsFilename).c_str ()) ;
#if defined(_WIN32) || defined(_WIN64)
				shaderFilename =FbxPathUtils::GetFolderName (gltfFilename) + "\\" + shaderFilename ;
#else
				shaderFilename =FbxPathUtils::GetFolderName (gltfFilename) + "/" + shaderFilename ;
#endif
				std::wfstream shaderFile (shaderFilename, std::ios::out | std::ofstream::binary) ;
				//_bin.seekg (0, std::ios_base::beg) ;
#if defined(_WIN32) || defined(_WIN64)
				shaderFile.write (tech.vertexShader ().source ().c_str (), tech.vertexShader ().source ().length ()) ;
#else
				shaderFile << tech.vertexShader ().source ().c_str();

#endif
				shaderFile.close () ;
			}
			utility::string_t fsFileame =_json [U("shaders")] [fsName] [U("uri")].as_string () ;
			{
				FbxString shaderFilename (utility::conversions::to_utf8string (fsFileame).c_str ()) ;
#if defined(_WIN32) || defined(_WIN64)
				shaderFilename =FbxPathUtils::GetFolderName (gltfFilename) + "\\" + shaderFilename ;
#else
				shaderFilename =FbxPathUtils::GetFolderName (gltfFilename) + "/" + shaderFilename ;
#endif
				std::wfstream shaderFile (shaderFilename, std::ios::out | std::ofstream::binary) ;
				//_bin.seekg (0, std::ios_base::beg) ;
#if defined(_WIN32) || defined(_WIN64)
				shaderFile.write (tech.fragmentShader ().source ().c_str (), tech.fragmentShader ().source ().length ()) ;
#else
				shaderFile << tech.fragmentShader ().source ().c_str ();
#endif
				shaderFile.close () ;
			}
		}
	}
	return (true) ;
}

}
