#pragma once

/************************************************************************************
* Mirror.cpp
*
* Sample app showing basic usage of the avatar SDK
************************************************************************************/

#include <OVR_Avatar.h>

#include <GL/glew.h>

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <malloc.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <map>
#include <chrono>

#include "Shader.h"

/************************************************************************************
* GL helpers
************************************************************************************/

enum {
	VERTEX,
	FRAGMENT,
	SHADER_COUNT
};


/************************************************************************************
* Math helpers and type conversions
************************************************************************************/

glm::vec3 _glmFromOvrVector(const ovrVector3f& ovrVector)
{
	return glm::vec3(ovrVector.x, ovrVector.y, ovrVector.z);
}

glm::quat _glmFromOvrQuat(const ovrQuatf& ovrQuat)
{
	return glm::quat(ovrQuat.w, ovrQuat.x, ovrQuat.y, ovrQuat.z);
}

void _glmFromOvrAvatarTransform(const ovrAvatarTransform& transform, glm::mat4* target) {
	glm::vec3 position(transform.position.x, transform.position.y, transform.position.z);
	glm::quat orientation(transform.orientation.w, transform.orientation.x, transform.orientation.y, transform.orientation.z);
	glm::vec3 scale(transform.scale.x, transform.scale.y, transform.scale.z);
	*target = glm::translate(position) * glm::mat4_cast(orientation) * glm::scale(scale);
}

void _ovrAvatarTransformFromGlm(const glm::vec3& position, const glm::quat& orientation, const glm::vec3& scale, ovrAvatarTransform* target) {
	target->position.x = position.x;
	target->position.y = position.y;
	target->position.z = position.z;
	target->orientation.x = orientation.x;
	target->orientation.y = orientation.y;
	target->orientation.z = orientation.z;
	target->orientation.w = orientation.w;
	target->scale.x = scale.x;
	target->scale.y = scale.y;
	target->scale.z = scale.z;
}

void _ovrAvatarTransformFromGlm(const glm::mat4& matrix, ovrAvatarTransform* target) {
	glm::vec3 scale;
	glm::quat orientation;
	glm::vec3 translation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(matrix, scale, orientation, translation, skew, perspective);
	_ovrAvatarTransformFromGlm(translation, orientation, scale, target);
}

void _ovrAvatarHandInputStateFromOvr(const ovrAvatarTransform& transform, const ovrInputState& inputState, ovrHandType hand, ovrAvatarHandInputState* state)
{
	state->transform = transform;
	state->buttonMask = 0;
	state->touchMask = 0;
	state->joystickX = inputState.Thumbstick[hand].x;
	state->joystickY = inputState.Thumbstick[hand].y;
	state->indexTrigger = inputState.IndexTrigger[hand];
	state->handTrigger = inputState.HandTrigger[hand];
	state->isActive = false;
	if (hand == ovrHand_Left)
	{
		if (inputState.Buttons & ovrButton_X) state->buttonMask |= ovrAvatarButton_One;
		if (inputState.Buttons & ovrButton_Y) state->buttonMask |= ovrAvatarButton_Two;
		if (inputState.Buttons & ovrButton_Enter) state->buttonMask |= ovrAvatarButton_Three;
		if (inputState.Buttons & ovrButton_LThumb) state->buttonMask |= ovrAvatarButton_Joystick;
		if (inputState.Touches & ovrTouch_X) state->touchMask |= ovrAvatarTouch_One;
		if (inputState.Touches & ovrTouch_Y) state->touchMask |= ovrAvatarTouch_Two;
		if (inputState.Touches & ovrTouch_LThumb) state->touchMask |= ovrAvatarTouch_Joystick;
		if (inputState.Touches & ovrTouch_LThumbRest) state->touchMask |= ovrAvatarTouch_ThumbRest;
		if (inputState.Touches & ovrTouch_LIndexTrigger) state->touchMask |= ovrAvatarTouch_Index;
		if (inputState.Touches & ovrTouch_LIndexPointing) state->touchMask |= ovrAvatarTouch_Pointing;
		if (inputState.Touches & ovrTouch_LThumbUp) state->touchMask |= ovrAvatarTouch_ThumbUp;
		state->isActive = (inputState.ControllerType & ovrControllerType_LTouch) != 0;
	}
	else if (hand == ovrHand_Right)
	{
		if (inputState.Buttons & ovrButton_A) state->buttonMask |= ovrAvatarButton_One;
		if (inputState.Buttons & ovrButton_B) state->buttonMask |= ovrAvatarButton_Two;
		if (inputState.Buttons & ovrButton_Home) state->buttonMask |= ovrAvatarButton_Three;
		if (inputState.Buttons & ovrButton_RThumb) state->buttonMask |= ovrAvatarButton_Joystick;
		if (inputState.Touches & ovrTouch_A) state->touchMask |= ovrAvatarTouch_One;
		if (inputState.Touches & ovrTouch_B) state->touchMask |= ovrAvatarTouch_Two;
		if (inputState.Touches & ovrTouch_RThumb) state->touchMask |= ovrAvatarTouch_Joystick;
		if (inputState.Touches & ovrTouch_RThumbRest) state->touchMask |= ovrAvatarTouch_ThumbRest;
		if (inputState.Touches & ovrTouch_RIndexTrigger) state->touchMask |= ovrAvatarTouch_Index;
		if (inputState.Touches & ovrTouch_RIndexPointing) state->touchMask |= ovrAvatarTouch_Pointing;
		if (inputState.Touches & ovrTouch_RThumbUp) state->touchMask |= ovrAvatarTouch_ThumbUp;
		state->isActive = (inputState.ControllerType & ovrControllerType_RTouch) != 0;
	}
}

void _computeWorldPose(const ovrAvatarSkinnedMeshPose& localPose, glm::mat4* worldPose)
{
	for (uint32_t i = 0; i < localPose.jointCount; ++i)
	{
		glm::mat4 local;
		_glmFromOvrAvatarTransform(localPose.jointTransform[i], &local);

		int parentIndex = localPose.jointParents[i];
		if (parentIndex < 0)
		{
			worldPose[i] = local;
		}
		else
		{
			worldPose[i] = worldPose[parentIndex] * local;
		}
	}
}

/************************************************************************************
* Wrappers for GL representations of avatar assets
************************************************************************************/

struct MeshData {
	GLuint vertexArray;
	GLuint vertexBuffer;
	GLuint elementBuffer;
	GLuint elementCount;
	glm::mat4 bindPose[OVR_AVATAR_MAXIMUM_JOINT_COUNT];
	glm::mat4 inverseBindPose[OVR_AVATAR_MAXIMUM_JOINT_COUNT];
};

struct TextureData {
	GLuint textureID;
};


class AvatarHandler {
	/************************************************************************************
	* Static state
	************************************************************************************/
	GLuint _skinnedMeshProgram;
	GLuint _combinedMeshProgram;
	GLuint _skinnedMeshPBSProgram;
	GLuint _debugLineProgram;
	GLuint _debugVertexArray;
	GLuint _debugVertexBuffer;
	ovrAvatar* _avatar;
	bool _combineMeshes = true;
	ovrAvatarAssetID _avatarCombinedMeshAlpha = 0;
	ovrAvatarVector4f _avatarCombinedMeshAlphaOffset;
	size_t _loadingAssets;
	bool _waitingOnCombinedMesh = false;
	bool controllersVisible = true;

	float _elapsedSeconds;
	std::map<ovrAvatarAssetID, void*> _assetMap;
	ovrSession _session;

private:
	MeshData* _loadCombinedMesh(const ovrAvatarMeshAssetDataV2* data)
	{
		_waitingOnCombinedMesh = false;

		MeshData* mesh = new MeshData();

		// Create the vertex array and buffer
		glGenVertexArrays(1, &mesh->vertexArray);
		glGenBuffers(1, &mesh->vertexBuffer);
		glGenBuffers(1, &mesh->elementBuffer);

		// Bind the vertex buffer and assign the vertex data	
		glBindVertexArray(mesh->vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, data->vertexCount * sizeof(ovrAvatarMeshVertexV2), data->vertexBuffer, GL_STATIC_DRAW);

		// Bind the index buffer and assign the index data
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->elementBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data->indexCount * sizeof(GLushort), data->indexBuffer, GL_STATIC_DRAW);
		mesh->elementCount = data->indexCount;

		// Fill in the array attributes
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertexV2), &((ovrAvatarMeshVertexV2*)0)->x);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertexV2), &((ovrAvatarMeshVertexV2*)0)->nx);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertexV2), &((ovrAvatarMeshVertexV2*)0)->tx);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertexV2), &((ovrAvatarMeshVertexV2*)0)->u);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(4, 4, GL_BYTE, GL_FALSE, sizeof(ovrAvatarMeshVertexV2), &((ovrAvatarMeshVertexV2*)0)->blendIndices);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertexV2), &((ovrAvatarMeshVertexV2*)0)->blendWeights);
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertexV2), &((ovrAvatarMeshVertexV2*)0)->r);
		glEnableVertexAttribArray(6);

		// Clean up
		glBindVertexArray(0);

		// Translate the bind pose
		_computeWorldPose(data->skinnedBindPose, mesh->bindPose);
		for (uint32_t i = 0; i < data->skinnedBindPose.jointCount; ++i)
		{
			mesh->inverseBindPose[i] = glm::inverse(mesh->bindPose[i]);
		}
		return mesh;
	}

	MeshData* _loadMesh(const ovrAvatarMeshAssetData* data)
	{
		MeshData* mesh = new MeshData();

		// Create the vertex array and buffer
		glGenVertexArrays(1, &mesh->vertexArray);
		glGenBuffers(1, &mesh->vertexBuffer);
		glGenBuffers(1, &mesh->elementBuffer);

		// Bind the vertex buffer and assign the vertex data	
		glBindVertexArray(mesh->vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, data->vertexCount * sizeof(ovrAvatarMeshVertex), data->vertexBuffer, GL_STATIC_DRAW);

		// Bind the index buffer and assign the index data
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->elementBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data->indexCount * sizeof(GLushort), data->indexBuffer, GL_STATIC_DRAW);
		mesh->elementCount = data->indexCount;

		// Fill in the array attributes
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->x);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->nx);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->tx);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->u);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(4, 4, GL_BYTE, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->blendIndices);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->blendWeights);
		glEnableVertexAttribArray(5);

		// Clean up
		glBindVertexArray(0);

		// Translate the bind pose
		_computeWorldPose(data->skinnedBindPose, mesh->bindPose);
		for (uint32_t i = 0; i < data->skinnedBindPose.jointCount; ++i)
		{
			mesh->inverseBindPose[i] = glm::inverse(mesh->bindPose[i]);
		}
		return mesh;
	}

	TextureData* _loadTexture(const ovrAvatarTextureAssetData* data)
	{
		// Create a texture
		TextureData* texture = new TextureData();
		glGenTextures(1, &texture->textureID);
		glBindTexture(GL_TEXTURE_2D, texture->textureID);

		// Load the image data
		switch (data->format)
		{
			// Handle uncompressed image data
		case ovrAvatarTextureFormat_RGB24:
			for (uint32_t level = 0, offset = 0, width = data->sizeX, height = data->sizeY; level < data->mipCount; ++level)
			{
				glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data->textureData + offset);
				offset += width * height * 3;
				width /= 2;
				height /= 2;
			}
			break;

			// Handle compressed image data
		case ovrAvatarTextureFormat_DXT1:
		case ovrAvatarTextureFormat_DXT5:
		{
			GLenum glFormat;
			int blockSize;
			if (data->format == ovrAvatarTextureFormat_DXT1)
			{
				blockSize = 8;
				glFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			}
			else
			{
				blockSize = 16;
				glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			}

			for (uint32_t level = 0, offset = 0, width = data->sizeX, height = data->sizeY; level < data->mipCount; ++level)
			{
				GLsizei levelSize = (width < 4 || height < 4) ? blockSize : blockSize * (width / 4) * (height / 4);
				glCompressedTexImage2D(GL_TEXTURE_2D, level, glFormat, width, height, 0, levelSize, data->textureData + offset);
				offset += levelSize;
				width /= 2;
				height /= 2;
			}
			break;
		}

		// Handle ASTC data
		case ovrAvatarTextureFormat_ASTC_RGB_6x6_MIPMAPS:
		{
			const unsigned char * level = (const unsigned char*)data->textureData;

			unsigned int w = data->sizeX;
			unsigned int h = data->sizeY;
			for (unsigned int i = 0; i < data->mipCount; i++)
			{
				int32_t blocksWide = (w + 5) / 6;
				int32_t blocksHigh = (h + 5) / 6;
				int32_t mipSize = 16 * blocksWide * blocksHigh;

				glCompressedTexImage2D(GL_TEXTURE_2D, i, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, w, h, 0, mipSize, level);

				level += mipSize;

				w >>= 1;
				h >>= 1;
				if (w < 1) { w = 1; }
				if (h < 1) { h = 1; }
			}
			break;
		}

		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		return texture;
	}


	/************************************************************************************
	* Rendering functions
	************************************************************************************/

	void _setTextureSampler(GLuint program, int textureUnit, const char uniformName[], ovrAvatarAssetID assetID)
	{
		GLuint textureID = 0;
		if (assetID)
		{
			void* data = _assetMap[assetID];
			TextureData* textureData = (TextureData*)data;
			textureID = textureData->textureID;
		}
		glActiveTexture(GL_TEXTURE0 + textureUnit);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(glGetUniformLocation(program, uniformName), textureUnit);
	}

	void _setTextureSamplers(GLuint program, const char uniformName[], size_t count, const int textureUnits[], const ovrAvatarAssetID assetIDs[])
	{
		for (int i = 0; i < count; ++i)
		{
			ovrAvatarAssetID assetID = assetIDs[i];

			GLuint textureID = 0;
			if (assetID)
			{
				void* data = _assetMap[assetID];
				if (data)
				{
					TextureData* textureData = (TextureData*)data;
					textureID = textureData->textureID;
				}
			}
			glActiveTexture(GL_TEXTURE0 + textureUnits[i]);
			glBindTexture(GL_TEXTURE_2D, textureID);
		}
		GLint uniformLocation = glGetUniformLocation(program, uniformName);
		glUniform1iv(uniformLocation, (GLsizei)count, textureUnits);
	}

	void _setMeshState(
		GLuint program,
		const ovrAvatarTransform& localTransform,
		const MeshData* data,
		const ovrAvatarSkinnedMeshPose& skinnedPose,
		const glm::mat4& world,
		const glm::mat4& view,
		const glm::mat4 proj,
		const glm::vec3& viewPos
	) {
		// Compute the final world and viewProjection matrices for this part
		glm::mat4 local;
		_glmFromOvrAvatarTransform(localTransform, &local);
		glm::mat4 worldMat = world * local;
		glm::mat4 viewProjMat = proj * view;

		// Compute the skinned pose
		glm::mat4* skinnedPoses = (glm::mat4*)alloca(sizeof(glm::mat4) * skinnedPose.jointCount);
		_computeWorldPose(skinnedPose, skinnedPoses);
		for (uint32_t i = 0; i < skinnedPose.jointCount; ++i)
		{
			skinnedPoses[i] = skinnedPoses[i] * data->inverseBindPose[i];
		}

		// Pass the world view position to the shader for view-dependent rendering
		glUniform3fv(glGetUniformLocation(program, "viewPos"), 1, glm::value_ptr(viewPos));

		// Assign the vertex uniforms
		glUniformMatrix4fv(glGetUniformLocation(program, "world"), 1, 0, glm::value_ptr(worldMat));
		glUniformMatrix4fv(glGetUniformLocation(program, "viewProj"), 1, 0, glm::value_ptr(viewProjMat));
		glUniformMatrix4fv(glGetUniformLocation(program, "meshPose"), (GLsizei)skinnedPose.jointCount, 0, glm::value_ptr(*skinnedPoses));
	}

	void _setMaterialState(GLuint program, const ovrAvatarMaterialState* state, glm::mat4* projectorInv)
	{
		// Assign the fragment uniforms
		glUniform1i(glGetUniformLocation(program, "useAlpha"), state->alphaMaskTextureID != 0);
		glUniform1i(glGetUniformLocation(program, "useNormalMap"), state->normalMapTextureID != 0);
		glUniform1i(glGetUniformLocation(program, "useRoughnessMap"), state->roughnessMapTextureID != 0);

		glUniform1f(glGetUniformLocation(program, "elapsedSeconds"), _elapsedSeconds);

		if (projectorInv)
		{
			glUniform1i(glGetUniformLocation(program, "useProjector"), 1);
			glUniformMatrix4fv(glGetUniformLocation(program, "projectorInv"), 1, 0, glm::value_ptr(*projectorInv));
		}
		else
		{
			glUniform1i(glGetUniformLocation(program, "useProjector"), 0);
		}

		int textureSlot = 1;
		glUniform4fv(glGetUniformLocation(program, "baseColor"), 1, &state->baseColor.x);
		glUniform1i(glGetUniformLocation(program, "baseMaskType"), state->baseMaskType);
		glUniform4fv(glGetUniformLocation(program, "baseMaskParameters"), 1, &state->baseMaskParameters.x);
		glUniform4fv(glGetUniformLocation(program, "baseMaskAxis"), 1, &state->baseMaskAxis.x);
		_setTextureSampler(program, textureSlot++, "alphaMask", state->alphaMaskTextureID);
		glUniform4fv(glGetUniformLocation(program, "alphaMaskScaleOffset"), 1, &state->alphaMaskScaleOffset.x);
		_setTextureSampler(program, textureSlot++, "clothingAlpha", _avatarCombinedMeshAlpha);
		glUniform4fv(glGetUniformLocation(program, "clothingAlphaScaleOffset"), 1, &_avatarCombinedMeshAlphaOffset.x);
		_setTextureSampler(program, textureSlot++, "normalMap", state->normalMapTextureID);
		glUniform4fv(glGetUniformLocation(program, "normalMapScaleOffset"), 1, &state->normalMapScaleOffset.x);
		_setTextureSampler(program, textureSlot++, "parallaxMap", state->parallaxMapTextureID);
		glUniform4fv(glGetUniformLocation(program, "parallaxMapScaleOffset"), 1, &state->parallaxMapScaleOffset.x);
		_setTextureSampler(program, textureSlot++, "roughnessMap", state->roughnessMapTextureID);
		glUniform4fv(glGetUniformLocation(program, "roughnessMapScaleOffset"), 1, &state->roughnessMapScaleOffset.x);

		struct LayerUniforms {
			int layerSamplerModes[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			int layerBlendModes[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			int layerMaskTypes[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			ovrAvatarVector4f layerColors[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			int layerSurfaces[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			ovrAvatarAssetID layerSurfaceIDs[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			ovrAvatarVector4f layerSurfaceScaleOffsets[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			ovrAvatarVector4f layerSampleParameters[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			ovrAvatarVector4f layerMaskParameters[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			ovrAvatarVector4f layerMaskAxes[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
		} layerUniforms;
		memset(&layerUniforms, 0, sizeof(layerUniforms));
		for (uint32_t i = 0; i < state->layerCount; ++i)
		{
			const ovrAvatarMaterialLayerState& layerState = state->layers[i];
			layerUniforms.layerSamplerModes[i] = layerState.sampleMode;
			layerUniforms.layerBlendModes[i] = layerState.blendMode;
			layerUniforms.layerMaskTypes[i] = layerState.maskType;
			layerUniforms.layerColors[i] = layerState.layerColor;
			layerUniforms.layerSurfaces[i] = textureSlot++;
			layerUniforms.layerSurfaceIDs[i] = layerState.sampleTexture;
			layerUniforms.layerSurfaceScaleOffsets[i] = layerState.sampleScaleOffset;
			layerUniforms.layerSampleParameters[i] = layerState.sampleParameters;
			layerUniforms.layerMaskParameters[i] = layerState.maskParameters;
			layerUniforms.layerMaskAxes[i] = layerState.maskAxis;
		}

		glUniform1i(glGetUniformLocation(program, "layerCount"), state->layerCount);
		glUniform1iv(glGetUniformLocation(program, "layerSamplerModes"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, layerUniforms.layerSamplerModes);
		glUniform1iv(glGetUniformLocation(program, "layerBlendModes"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, layerUniforms.layerBlendModes);
		glUniform1iv(glGetUniformLocation(program, "layerMaskTypes"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, layerUniforms.layerMaskTypes);
		glUniform4fv(glGetUniformLocation(program, "layerColors"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerColors);
		_setTextureSamplers(program, "layerSurfaces", OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, layerUniforms.layerSurfaces, layerUniforms.layerSurfaceIDs);
		glUniform4fv(glGetUniformLocation(program, "layerSurfaceScaleOffsets"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerSurfaceScaleOffsets);
		glUniform4fv(glGetUniformLocation(program, "layerSampleParameters"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerSampleParameters);
		glUniform4fv(glGetUniformLocation(program, "layerMaskParameters"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerMaskParameters);
		glUniform4fv(glGetUniformLocation(program, "layerMaskAxes"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerMaskAxes);

	}

	void _setPBSState(GLuint program, const ovrAvatarAssetID albedoTextureID, const ovrAvatarAssetID surfaceTextureID)
	{
		int textureSlot = 0;
		_setTextureSampler(program, textureSlot++, "albedo", albedoTextureID);
		_setTextureSampler(program, textureSlot++, "surface", surfaceTextureID);
	}

	void _renderDebugLine(const glm::mat4& worldViewProj, const glm::vec3& a, const glm::vec3& b, const glm::vec4& aColor, const glm::vec4& bColor)
	{
		glUseProgram(_debugLineProgram);
		glUniformMatrix4fv(glGetUniformLocation(_debugLineProgram, "worldViewProj"), 1, 0, glm::value_ptr(worldViewProj));

		struct {
			glm::vec3 p;
			glm::vec4 c;
		} vertices[2] = {
			{ a, aColor },
			{ b, bColor },
		};

		glBindVertexArray(_debugVertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, _debugVertexArray);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		// Fill in the array attributes
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertices[0]), 0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(vertices[0]), (void*)sizeof(glm::vec3));
		glEnableVertexAttribArray(1);

		glDrawArrays(GL_LINE_STRIP, 0, 2);
	}

	void _renderPose(const glm::mat4& worldViewProj, const ovrAvatarSkinnedMeshPose& pose)
	{
		glm::mat4* skinnedPoses = (glm::mat4*)alloca(sizeof(glm::mat4) * pose.jointCount);
		_computeWorldPose(pose, skinnedPoses);
		for (uint32_t i = 1; i < pose.jointCount; ++i)
		{
			int parent = pose.jointParents[i];
			_renderDebugLine(worldViewProj, glm::vec3(skinnedPoses[parent][3]), glm::vec3(skinnedPoses[i][3]), glm::vec4(1, 1, 1, 1), glm::vec4(1, 0, 0, 1));
		}
	}

	void _renderSkinnedMeshPart(GLuint shader, const ovrAvatarRenderPart_SkinnedMeshRender* mesh, uint32_t visibilityMask, const glm::mat4& world, const glm::mat4& view, const glm::mat4 proj, const glm::vec3& viewPos, bool renderJoints)
	{
		// If this part isn't visible from the viewpoint we're rendering from, do nothing
		if ((mesh->visibilityMask & visibilityMask) == 0)
		{
			return;
		}

		// Get the GL mesh data for this mesh's asset
		MeshData* data = (MeshData*)_assetMap[mesh->meshAssetID];

		glUseProgram(shader);

		// Apply the vertex state
		_setMeshState(shader, mesh->localTransform, data, mesh->skinnedPose, world, view, proj, viewPos);

		// Apply the material state
		_setMaterialState(shader, &mesh->materialState, nullptr);

		// Draw the mesh
		glBindVertexArray(data->vertexArray);
		//glDepthFunc(GL_LESS);

		// Write to depth first for self-occlusion
		//if (mesh->visibilityMask & ovrAvatarVisibilityFlag_SelfOccluding)
		//{
		//	glDepthMask(GL_TRUE);
		//	glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		//	glDrawElements(GL_TRIANGLES, (GLsizei)data->elementCount, GL_UNSIGNED_SHORT, 0);
		//	glDepthFunc(GL_EQUAL);
		//}

		// Render to color buffer
		//glDepthMask(GL_FALSE);
		glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDrawElements(GL_TRIANGLES, (GLsizei)data->elementCount, GL_UNSIGNED_SHORT, 0);
		glBindVertexArray(0);

		if (renderJoints)
		{
			glm::mat4 local;
			_glmFromOvrAvatarTransform(mesh->localTransform, &local);
			glDepthFunc(GL_ALWAYS);
			_renderPose(proj * view * world * local, mesh->skinnedPose);
		}
	}

	void _renderSkinnedMeshPartPBS(const ovrAvatarRenderPart_SkinnedMeshRenderPBS* mesh, uint32_t visibilityMask, const glm::mat4& world, const glm::mat4& view, const glm::mat4 proj, const glm::vec3& viewPos, bool renderJoints)
	{
		// If this part isn't visible from the viewpoint we're rendering from, do nothing
		if ((mesh->visibilityMask & visibilityMask) == 0)
		{
			return;
		}

		// Get the GL mesh data for this mesh's asset
		MeshData* data = (MeshData*)_assetMap[mesh->meshAssetID];

		glUseProgram(_skinnedMeshPBSProgram);

		// Apply the vertex state
		_setMeshState(_skinnedMeshPBSProgram, mesh->localTransform, data, mesh->skinnedPose, world, view, proj, viewPos);

		// Apply the material state
		_setPBSState(_skinnedMeshPBSProgram, mesh->albedoTextureAssetID, mesh->surfaceTextureAssetID);

		// Draw the mesh
		glBindVertexArray(data->vertexArray);
		//glDepthFunc(GL_LESS);

		// Write to depth first for self-occlusion
		//if (mesh->visibilityMask & ovrAvatarVisibilityFlag_SelfOccluding)
		//{
		//	glDepthMask(GL_TRUE);
		//	glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		//	glDrawElements(GL_TRIANGLES, (GLsizei)data->elementCount, GL_UNSIGNED_SHORT, 0);
		//	glDepthFunc(GL_EQUAL);
		//}
		//glDepthMask(GL_FALSE);

		// Draw the mesh
		glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDrawElements(GL_TRIANGLES, (GLsizei)data->elementCount, GL_UNSIGNED_SHORT, 0);
		glBindVertexArray(0);

		if (renderJoints)
		{
			glm::mat4 local;
			_glmFromOvrAvatarTransform(mesh->localTransform, &local);
			glDepthFunc(GL_ALWAYS);
			_renderPose(proj * view * world * local, mesh->skinnedPose);
		}
	}

	void _renderProjector(const ovrAvatarRenderPart_ProjectorRender* projector, ovrAvatar* avatar, uint32_t visibilityMask, const glm::mat4& world, const glm::mat4& view, const glm::mat4 proj, const glm::vec3& viewPos)
	{

		// Compute the mesh transform
		const ovrAvatarComponent* component = ovrAvatarComponent_Get(avatar, projector->componentIndex);
		const ovrAvatarRenderPart* renderPart = component->renderParts[projector->renderPartIndex];
		const ovrAvatarRenderPart_SkinnedMeshRender* mesh = ovrAvatarRenderPart_GetSkinnedMeshRender(renderPart);

		// If this part isn't visible from the viewpoint we're rendering from, do nothing
		if ((mesh->visibilityMask & visibilityMask) == 0)
		{
			return;
		}

		// Compute the projection matrix
		glm::mat4 projection;
		_glmFromOvrAvatarTransform(projector->localTransform, &projection);
		glm::mat4 worldProjection = world * projection;
		glm::mat4 projectionInv = glm::inverse(worldProjection);

		// Compute the mesh transform
		glm::mat4 meshWorld;
		_glmFromOvrAvatarTransform(component->transform, &meshWorld);

		// Get the GL mesh data for this mesh's asset
		MeshData* data = (MeshData*)_assetMap[mesh->meshAssetID];

		glUseProgram(_skinnedMeshProgram);

		// Apply the vertex state
		_setMeshState(_skinnedMeshProgram, mesh->localTransform, data, mesh->skinnedPose, meshWorld, view, proj, viewPos);

		// Apply the material state
		_setMaterialState(_skinnedMeshProgram, &projector->materialState, &projectionInv);

		// Draw the mesh
		glBindVertexArray(data->vertexArray);
		//glDepthMask(GL_FALSE);
		//glDepthFunc(GL_EQUAL);
		glDrawElements(GL_TRIANGLES, (GLsizei)data->elementCount, GL_UNSIGNED_SHORT, 0);
		glBindVertexArray(0);
	}

	void _renderAvatar(ovrAvatar* avatar, uint32_t visibilityMask, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos, bool renderJoints)
	{
		// Traverse over all components on the avatar
		uint32_t componentCount = ovrAvatarComponent_Count(avatar);

		const ovrAvatarComponent* bodyComponent = nullptr;
		if (const ovrAvatarBodyComponent* body = ovrAvatarPose_GetBodyComponent(avatar))
		{
			bodyComponent = body->renderComponent;
		}

		for (uint32_t i = 0; i < componentCount; ++i)
		{
			const ovrAvatarComponent* component = ovrAvatarComponent_Get(avatar, i);
			//std::cerr << compon	ent->name << std::endl;

			if (std::strcmp(component->name, "base") == 0) {
				continue;
			}

			const bool useCombinedMeshProgram = _combineMeshes && bodyComponent == component;

			// Compute the transform for this component
			glm::mat4 world;
			_glmFromOvrAvatarTransform(component->transform, &world);

			// Render each render part attached to the component
			for (uint32_t j = 0; j < component->renderPartCount; ++j)
			{
				const ovrAvatarRenderPart* renderPart = component->renderParts[j];
				ovrAvatarRenderPartType type = ovrAvatarRenderPart_GetType(renderPart);
				switch (type)
				{
				case ovrAvatarRenderPartType_SkinnedMeshRender:
					_renderSkinnedMeshPart(useCombinedMeshProgram ? _combinedMeshProgram : _skinnedMeshProgram, ovrAvatarRenderPart_GetSkinnedMeshRender(renderPart), visibilityMask, world, view, proj, viewPos, renderJoints);
					break;
				case ovrAvatarRenderPartType_SkinnedMeshRenderPBS:
					_renderSkinnedMeshPartPBS(ovrAvatarRenderPart_GetSkinnedMeshRenderPBS(renderPart), visibilityMask, world, view, proj, viewPos, renderJoints);
					break;
				case ovrAvatarRenderPartType_ProjectorRender:
					_renderProjector(ovrAvatarRenderPart_GetProjectorRender(renderPart), avatar, visibilityMask, world, view, proj, viewPos);
					break;
				}
			}
		}
	}

	void _updateAvatar(
		ovrAvatar* avatar,
		float deltaSeconds,
		const ovrAvatarTransform& hmd,
		const ovrAvatarHandInputState& left,
		const ovrAvatarHandInputState& right,
		ovrAvatarPacket* packet,
		float* packetPlaybackTime
	) {
		if (packet)
		{
			float packetDuration = ovrAvatarPacket_GetDurationSeconds(packet);
			*packetPlaybackTime += deltaSeconds;
			if (*packetPlaybackTime > packetDuration)
			{
				ovrAvatarPose_Finalize(avatar, 0.0f);
				*packetPlaybackTime = 0;
			}
			ovrAvatar_UpdatePoseFromPacket(avatar, packet, *packetPlaybackTime);
		}
		else
		{
			// Update the avatar pose from the inputs
			ovrAvatarPose_UpdateBody(avatar, hmd);
			ovrAvatarPose_UpdateHands(avatar, left, right);
		}
		ovrAvatarPose_Finalize(avatar, deltaSeconds);
	}


	/************************************************************************************
	* Avatar message handlers
	************************************************************************************/

	void _handleAvatarSpecification(const ovrAvatarMessage_AvatarSpecification* message)
	{
		// Create the avatar instance
		_avatar = ovrAvatar_Create(message->avatarSpec, ovrAvatarCapability_All);

		// Trigger load operations for all of the assets referenced by the avatar
		uint32_t refCount = ovrAvatar_GetReferencedAssetCount(_avatar);
		for (uint32_t i = 0; i < refCount; ++i)
		{
			ovrAvatarAssetID id = ovrAvatar_GetReferencedAsset(_avatar, i);
			ovrAvatarAsset_BeginLoading(id);
			++_loadingAssets;
		}
		std::cout << "Loading " << _loadingAssets << " assets...\r\n" << std::endl;
	}

	void _handleAssetLoaded(const ovrAvatarMessage_AssetLoaded* message)
	{
		// Determine the type of the asset that got loaded
		ovrAvatarAssetType assetType = ovrAvatarAsset_GetType(message->asset);
		void* data = nullptr;

		// Call the appropriate loader function
		switch (assetType)
		{
		case ovrAvatarAssetType_Mesh:
			data = _loadMesh(ovrAvatarAsset_GetMeshData(message->asset));
			break;
		case ovrAvatarAssetType_Texture:
			data = _loadTexture(ovrAvatarAsset_GetTextureData(message->asset));
			break;
		case ovrAvatarAssetType_CombinedMesh:
			data = _loadCombinedMesh(ovrAvatarAsset_GetCombinedMeshData(message->asset));
			break;
		default:
			break;
		}

		// Store the data that we loaded for the asset in the asset map
		_assetMap[message->assetID] = data;

		if (assetType == ovrAvatarAssetType_CombinedMesh)
		{
			uint32_t idCount = 0;
			ovrAvatarAsset_GetCombinedMeshIDs(message->asset, &idCount);
			_loadingAssets -= idCount;
			ovrAvatar_GetCombinedMeshAlphaData(_avatar, &_avatarCombinedMeshAlpha, &_avatarCombinedMeshAlphaOffset);
		}
		else
		{
			--_loadingAssets;
		}

		std::cout << "Loading " << _loadingAssets << " assets...\r\n" << std::endl;
	}



	void initAvatar(ovrSession s)
	{
		_session = s;

		// Compile the reference shaders
		_skinnedMeshProgram = Shader("AvatarVertexShader.glsl", "AvatarFragmentShader.glsl").ID;
		_skinnedMeshPBSProgram = Shader("AvatarVertexShader.glsl", "AvatarFragmentShaderPBS.glsl").ID;
		_combinedMeshProgram = Shader("AvatarVertexShader.glsl", "AvatarFragmentShader_CombinedMesh.glsl").ID;

		// Start retrieving the avatar specification
		std::cout << "Requesting avatar specification..." << std::endl;
		uint64_t userID = 129572074806497;

		_waitingOnCombinedMesh = _combineMeshes;
		auto requestSpec = ovrAvatarSpecificationRequest_Create(userID);
		ovrAvatarSpecificationRequest_SetCombineMeshes(requestSpec, _combineMeshes);
		ovrAvatar_RequestAvatarSpecificationFromSpecRequest(requestSpec);
		ovrAvatarSpecificationRequest_Destroy(requestSpec);
	}

public:

	AvatarHandler(ovrSession s) {
		initAvatar(s);
	}
	~AvatarHandler() {
		avatar_shutdown();
	}

	void updateAvatar(const glm::mat4 & proj, const glm::mat4 & view, const glm::vec3 eyePos) {
		// Run the main loop
		bool customBasePosition = false;
		bool renderJoints = false;
		bool freezePose = false;
		int capabilities = ovrAvatarCapability_All;
		bool running = true;
		long long frameIndex = 0;
		ovrAvatarPacket* playbackPacket = nullptr;
		float playbackTime = 0;
		std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
		uint64_t testUserID = 0;

		// Compute how much time has elapsed since the last frame
		std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
		std::chrono::duration<float> deltaTime = currentTime - lastTime;
		float deltaSeconds = deltaTime.count();
		lastTime = currentTime;
		_elapsedSeconds += deltaSeconds;


		// Pump avatar messages
		while (ovrAvatarMessage* message = ovrAvatarMessage_Pop())
		{
			switch (ovrAvatarMessage_GetType(message))
			{
			case ovrAvatarMessageType_AvatarSpecification:
				_handleAvatarSpecification(ovrAvatarMessage_GetAvatarSpecification(message));
				break;
			case ovrAvatarMessageType_AssetLoaded:
				_handleAssetLoaded(ovrAvatarMessage_GetAssetLoaded(message));
				break;
			}
			ovrAvatarMessage_Free(message);
		}

		// If the avatar is initialized, update it
		if (_avatar)
		{
			ovrAvatar_SetLeftControllerVisibility(_avatar, false);
			ovrAvatar_SetRightControllerVisibility(_avatar, false);

			// Convert the OVR inputs into Avatar SDK inputs
			ovrInputState touchState;
			ovr_GetInputState(_session, ovrControllerType_Active, &touchState);
			ovrTrackingState trackingState = ovr_GetTrackingState(_session, 0.0, false);

			glm::vec3 hmdP = _glmFromOvrVector(trackingState.HeadPose.ThePose.Position);
			glm::quat hmdQ = _glmFromOvrQuat(trackingState.HeadPose.ThePose.Orientation);
			glm::vec3 leftP = _glmFromOvrVector(trackingState.HandPoses[ovrHand_Left].ThePose.Position);
			glm::quat leftQ = _glmFromOvrQuat(trackingState.HandPoses[ovrHand_Left].ThePose.Orientation);
			glm::vec3 rightP = _glmFromOvrVector(trackingState.HandPoses[ovrHand_Right].ThePose.Position);
			glm::quat rightQ = _glmFromOvrQuat(trackingState.HandPoses[ovrHand_Right].ThePose.Orientation);

			ovrAvatarTransform hmd;
			_ovrAvatarTransformFromGlm(hmdP, hmdQ, glm::vec3(1.0f), &hmd);

			ovrAvatarTransform left;
			_ovrAvatarTransformFromGlm(leftP, leftQ, glm::vec3(1.0f), &left);

			ovrAvatarTransform right;
			_ovrAvatarTransformFromGlm(rightP, rightQ, glm::vec3(1.0f), &right);

			ovrAvatarHandInputState inputStateLeft;
			_ovrAvatarHandInputStateFromOvr(left, touchState, ovrHand_Left, &inputStateLeft);

			ovrAvatarHandInputState inputStateRight;
			_ovrAvatarHandInputStateFromOvr(right, touchState, ovrHand_Right, &inputStateRight);

			_updateAvatar(_avatar, deltaSeconds, hmd, inputStateLeft, inputStateRight, playbackPacket, &playbackTime);

			// Render the avatar
			_renderAvatar(_avatar, ovrAvatarVisibilityFlag_FirstPerson, view, proj, eyePos, renderJoints);
		}

	}

	void avatar_shutdown() {
		std::cout << "Shutting down..." << std::endl;;
		if (_avatar)
		{
			ovrAvatar_Destroy(_avatar);
		}
		ovrAvatar_Shutdown();
	}

};

