///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ============
// manage the preparing and rendering of 3D scenes - textures, materials, lighting
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

#include "..\\..\\Utilities\\camera.h"

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager *pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			stbi_image_free(image);
			glBindTexture(GL_TEXTURE_2D, 0);
			return false;
		}

		// generate the texture mipmaps for mapping textures to lower resolutions
		glGenerateMipmap(GL_TEXTURE_2D);

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// delete the OpenGL texture
		glDeleteTextures(1, &m_textureIDs[i].ID);
		m_textureIDs[i].ID = 0;
		m_textureIDs[i].tag.clear();
	}
	m_loadedTextures = 0;
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
			material.ambientColor = m_objectMaterials[index].ambientColor;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationZ * rotationY * rotationX * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/


/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene 
 *  rendering
 ***********************************************************/
void SceneManager::LoadSceneTextures() {
	bool bReturn = false;

	bReturn = CreateGLTexture(

		"textures/floor.jpg", "floor");

	bReturn = CreateGLTexture(

		"textures/base.jpg", "base");

	bReturn = CreateGLTexture(

		"textures/top.jpg", "top");

	bReturn = CreateGLTexture(
		"textures/wall.jpg", "wall");
	
	bReturn = CreateGLTexture(
		"textures/blue.jpg", "blue");

	bReturn = CreateGLTexture(
		"textures/green.jpg", "green");

	bReturn = CreateGLTexture(
		"textures/red.png", "red");

	bReturn = CreateGLTexture(
		"textures/orange.jpg", "orange");

	bReturn = CreateGLTexture(
		"textures/yellow.jpg", "yellow");

	bReturn = CreateGLTexture(
		"textures/block2.png", "block2");

	bReturn = CreateGLTexture(
		"textures/block.jpg", "block");

	bReturn = CreateGLTexture(
		"textures/torus.jpg", "torus");

	bReturn = CreateGLTexture(
		"textures/cylinder.jpg", "cylinder");

	BindGLTextures();
}
void SceneManager::PrepareScene()
{
	// Load textures so they are available when rendering
	LoadSceneTextures();

	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene
	DefineObjectMaterials();
	SetupSceneLights();
	

	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadConeMesh();
	m_basicMeshes->LoadTorusMesh();
	m_basicMeshes->LoadTaperedCylinderMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadCylinderMesh();

	
}

void SceneManager::DefineObjectMaterials() {
	OBJECT_MATERIAL goldMaterial;

	goldMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.1f);
	goldMaterial.ambientStrength = 0.4f;

	goldMaterial.diffuseColor = glm::vec3(0.3f, 0.3f, 0.2f);
	goldMaterial.specularColor = glm::vec3(0.6f, 0.5f, 0.4f);
	goldMaterial.shininess = 22.0;
	goldMaterial.tag = "gold";

	m_objectMaterials.push_back(goldMaterial);



	OBJECT_MATERIAL cementMaterial;

	cementMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.2f);
	cementMaterial.ambientStrength = 0.2f;
	cementMaterial.diffuseColor = glm::vec3(0.5f, 0.5f, 0.5f);
	cementMaterial.specularColor = glm::vec3(0.4f, 0.4f, 0.4f);
	cementMaterial.shininess = 0.5;
	cementMaterial.tag = "cement";



	m_objectMaterials.push_back(cementMaterial);



	OBJECT_MATERIAL woodMaterial;

	woodMaterial.ambientColor = glm::vec3(0.4f, 0.3f, 0.1f);
	woodMaterial.ambientStrength = 0.2f;
	woodMaterial.diffuseColor = glm::vec3(0.3f, 0.2f, 0.1f);
	woodMaterial.specularColor = glm::vec3(0.1f, 0.1f, 0.1f);
	woodMaterial.shininess = 0.3;

	woodMaterial.tag = "wood";



	m_objectMaterials.push_back(woodMaterial);



	OBJECT_MATERIAL leMaterial;

	leMaterial.ambientColor = glm::vec3(0.2f, 0.3f, 0.4f);

	leMaterial.ambientStrength = 0.3f;

	leMaterial.diffuseColor = glm::vec3(0.3f, 0.2f, 0.1f);

	leMaterial.specularColor = glm::vec3(0.4f, 0.5f, 0.6f);

	leMaterial.shininess = 25.0;

	leMaterial.tag = "le";



	m_objectMaterials.push_back(leMaterial);



	OBJECT_MATERIAL glassMaterial;

	glassMaterial.ambientColor = glm::vec3(0.4f, 0.4f, 0.4f);
	glassMaterial.ambientStrength = 0.3f;
	glassMaterial.diffuseColor = glm::vec3(0.3f, 0.3f, 0.3f);
	glassMaterial.specularColor = glm::vec3(0.6f, 0.6f, 0.6f);
	glassMaterial.shininess = 85.0;

	glassMaterial.tag = "glass";



	m_objectMaterials.push_back(glassMaterial);



	OBJECT_MATERIAL clayMaterial;

	clayMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.3f);
	clayMaterial.ambientStrength = 0.3f;
	clayMaterial.diffuseColor = glm::vec3(0.4f, 0.4f, 0.5f);
	clayMaterial.specularColor = glm::vec3(0.2f, 0.2f, 0.4f);

	clayMaterial.shininess = 0.5;

	clayMaterial.tag = "clay";



	m_objectMaterials.push_back(clayMaterial);


}
void SceneManager::SetupSceneLights() {
	// ensure shader program is active so uniform calls take effect
	if (m_pShaderManager)
		m_pShaderManager->use();

	// Primary overhead light (key) stronger diffuse and slightly higher ambient
	m_pShaderManager->setVec3Value("lightSources[0].position", 10.0f, 10.0f, 10.0f);
	m_pShaderManager->setVec3Value("lightSources[0].ambientColor", 0.03f, 0.03f, 0.03f);   // increase ambient slightly
	m_pShaderManager->setVec3Value("lightSources[0].diffuseColor", 0.8f, 0.8f, 0.78f);    // stronger key diffuse
	m_pShaderManager->setVec3Value("lightSources[0].specularColor", 1.0f, 1.0f, 1.0f);    // keep bright highlights
	m_pShaderManager->setFloatValue("lightSources[0].focalStrength", 24.0f);              // tighten falloff a bit
	m_pShaderManager->setFloatValue("lightSources[0].specularIntensity", 0.18f);         // stronger specular contribution

	// Left-upper soft yellow ambient wash (background ambient)  warmer fill, small diffuse
	m_pShaderManager->setVec3Value("lightSources[1].position", -15.0f, 15.0f, -5.0f);
	m_pShaderManager->setVec3Value("lightSources[1].ambientColor", 0.18f, 0.15f, 0.04f); // warmer and slightly stronger ambient
	m_pShaderManager->setVec3Value("lightSources[1].diffuseColor", 0.05f, 0.045f, 0.02f); // small diffuse to hint direction
	m_pShaderManager->setVec3Value("lightSources[1].specularColor", 0.0f, 0.0f, 0.0f);
	m_pShaderManager->setFloatValue("lightSources[1].focalStrength", 64.0f);
	m_pShaderManager->setFloatValue("lightSources[1].specularIntensity", 0.0f);

	// Fill light 2 (soft frontal fill)  moderate to lift midtones
	m_pShaderManager->setVec3Value("lightSources[2].position", 1.0f, 10.0f, 5.0f);
	m_pShaderManager->setVec3Value("lightSources[2].ambientColor", 0.02f, 0.02f, 0.02f);
	m_pShaderManager->setVec3Value("lightSources[2].diffuseColor", 0.5f, 0.5f, 0.5f);   // stronger fill
	m_pShaderManager->setVec3Value("lightSources[2].specularColor", 1.0f, 1.0f, 1.0f);
	m_pShaderManager->setFloatValue("lightSources[2].focalStrength", 64.0f);
	m_pShaderManager->setFloatValue("lightSources[2].specularIntensity", 0.08f);

	// Backlight / rim light (behind & above) ? stronger rim to enhance silhouettes and depth
	m_pShaderManager->setVec3Value("lightSources[3].position", 0.0f, 8.0f, 15.0f);
	m_pShaderManager->setVec3Value("lightSources[3].ambientColor", 0.03f, 0.03f, 0.035f); // small ambient
	m_pShaderManager->setVec3Value("lightSources[3].diffuseColor", 0.5f, 0.5f, 0.55f);    // noticeable rim diffuse
	m_pShaderManager->setVec3Value("lightSources[3].specularColor", 0.9f, 0.9f, 1.0f);    // strong rim specular
	m_pShaderManager->setFloatValue("lightSources[3].focalStrength", 12.0f);              // tighter rim
	m_pShaderManager->setFloatValue("lightSources[3].specularIntensity", 0.35f);         // stronger highlight for edges
}







/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by 
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{
	// ensure shader program is active so uniform calls take effect
	if (m_pShaderManager)
		m_pShaderManager->use();
	// ensure shader program is active so uniform calls take effect
	if (m_pShaderManager)
		m_pShaderManager->use();

	// ensure shader program is active so uniform calls take effect
	if (m_pShaderManager)
		m_pShaderManager->use();

	// ensure shader program is active so uniform calls take effect
	if (m_pShaderManager)
		m_pShaderManager->use();

	// ensure shader program is active so uniform calls take effect
	if (m_pShaderManager)
		m_pShaderManager->use();

	// ensure shader program is active so uniform calls take effect
	if (m_pShaderManager)
		m_pShaderManager->use();

	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/*** Set needed transformations before drawing the basic mesh.  ***/
	/*** This same ordering of code should be used for transforming ***/
	/*** and drawing all the basic 3D shapes.						***/
	/******************************************************************/
	/****************************************************************/
	//Toddler Cup
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(2.0f, 5.0f, 1.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 180.0f;
	YrotationDegrees = 180.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-5.0f, 6.0f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);
	
	//SetShaderColor(0.23, 0.63, 0.54, 1);
	SetShaderTexture("base");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("clay");
	// draw the mesh with transformation values
	m_basicMeshes->DrawTaperedCylinderMesh();

	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(2.0f, 1.05f, 1.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(-5.0f, 6.2f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.26, 0.78, 0.54, 1);
	SetShaderTexture("top");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("clay");
	// draw the mesh with transformation values
	m_basicMeshes->DrawTorusMesh();

	//******************************************************//
	//3D plane Milestone 3//
	// set the XYZ scale for the mesh
		// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(20.0f, 2.0f, 10.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.781, 0.6518, 0.1658, 1);
	SetShaderTexture("floor");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("wood");
	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
//-----------------------------------------------------------------------//
	// set the XYZ scale for the mesh
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(20.0f, 200.0f, 10.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 10.0f, -10.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,			
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.781, 0.6518, 0.1658, 1);
	SetShaderTexture("wall");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("clay");
	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
	//-----------------------------------------------------------------------//
	// set the XYZ scale for the mesh
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.0f, 7.0f, 1.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(2.02f, 0.0f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.781, 0.6518, 0.1658, 1);
	SetShaderTexture("cylinder");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("clay");
	// draw the mesh with transformation values
	m_basicMeshes->DrawTaperedCylinderMesh();
	//-----------------------------------------------------------------------//
	// set the XYZ scale for the mesh
	// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(6.0f, 2.0f, 5.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(2.0f, 0.0f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.0, 0.0, 0.0, 1);
	SetShaderTexture("wall");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.8f, 3.0f, 2.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(2.0f, 1.3f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.0, 0.1, 0.0, 1);
	SetShaderTexture("blue");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawTorusMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.6f, 3.0f, 2.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(2.0f, 2.0f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.0, 0.2, 0.0, 1);
	SetShaderTexture("green");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawTorusMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.4f, 3.0f, 2.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(2.0f, 2.7f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.1, 0.2, 0.0, 1);
	SetShaderTexture("yellow");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawTorusMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.2f, 3.0f, 2.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(2.0f, 3.4f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.7, 0.2, 0.0, 1);
	SetShaderTexture("orange");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawTorusMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(1.0f, 3.0f, 2.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(2.0f, 4.2f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.7, 0.8, 0.0, 1);
	SetShaderTexture("red");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawTorusMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.8f, 3.0f, 2.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(2.0f, 5.0f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.65, 0.2, 0.0, 1);
	SetShaderTexture("torus");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawTorusMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.53f, 1.0f, 1.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(2.01f, 6.4f, 3.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.7, 0.2, 0.0, 1);
	SetShaderTexture("cylinder");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("clay");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(2.0f, 2.0f, 2.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(8.5f, 1.0f, 4.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.7, 0.2, 0.0, 1);
	SetShaderTexture("block");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.8f, 0.5f, 0.5f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 90.0f;
	YrotationDegrees = 90.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(9.3f, 1.0f, 3.9f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(1.0, 0.2, 0.0, 1);
	SetShaderTexture("block");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(2.0f, 2.0f, 2.0f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 1.0f, 8.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(0.7, 0.2, 0.0, 1);
	SetShaderTexture("red");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawBoxMesh();
	//-----------------------------------------------------------------------//
// set the XYZ scale for the mesh
// set the XYZ scale for the mesh
	scaleXYZ = glm::vec3(0.8f, 0.5f, 0.5f);

	// set the XYZ rotation for the mesh
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh
	positionXYZ = glm::vec3(0.0f, 1.9f, 8.0f);

	// set the transformations into memory to be used on the drawn meshes
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	//SetShaderColor(1.0, 0.2, 0.0, 1);
	SetShaderTexture("red");
	SetTextureUVScale(1.0, 1.0);
	SetShaderMaterial("glass");
	// draw the mesh with transformation values
	m_basicMeshes->DrawCylinderMesh();
}
