#pragma once

#include <algorithm>
#include <vector>
#include <array>
#include "Core.h"
#include "GlfwApp.h"
#include "AvatarHandler.h"


class RiftManagerApp {
protected:
	ovrSession _session;
	ovrHmdDesc _hmdDesc;
	ovrGraphicsLuid _luid;

public:
	RiftManagerApp() {
		if (!OVR_SUCCESS(ovr_Create(&_session, &_luid))) {
			FAIL("Unable to create HMD session");
		}

		_hmdDesc = ovr_GetHmdDesc(_session);
	}

	~RiftManagerApp() {
		ovr_Destroy(_session);
		_session = nullptr;
	}
};

class RiftApp : public GlfwApp, public RiftManagerApp {
public:

private:
	GLuint _fbo{ 0 };
	GLuint _depthBuffer{ 0 };
	ovrTextureSwapChain _eyeTexture;

	GLuint _mirrorFbo{ 0 };
	ovrMirrorTexture _mirrorTexture;

	ovrEyeRenderDesc _eyeRenderDescs[2];

	mat4 _eyeProjections[2];

	ovrLayerEyeFov _sceneLayer;
	ovrViewScaleDesc _viewScaleDesc;

	// TODO: delete
	ovrViewScaleDesc _viewScaleDescBase;
	const float minIPD = -.1f; // based on right eye
	const float maxIPD = .3f; // based on right eye
	//std::vector<ovrPosef[2]> headBuffer;
	std::array<std::array<ovrPosef,2>, 30> headBuffer;
	int headPointer = 0;
	int lag = 0;
	int delay = 0;
	int currentDelay = 0;

	template <typename T, size_t SIZE>
	static int incRingHead(std::array<T, SIZE> & v, int head) {
		if (head < 0)
			head = head + (int) v.size();
		return (head + 1) % v.size();
	}

	template <typename T, size_t SIZE>
	static T& ringIdx(std::array<T, SIZE> & v, int idx) {
		if (idx < 0)
			idx = idx + (int) v.size();
		return  v.at(idx % v.size());
	}


	uvec2 _renderTargetSize;
	uvec2 _mirrorSize;

	std::unique_ptr<AvatarHandler> av;

public:

	RiftApp() {
		using namespace ovr;
		_viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;

		memset(&_sceneLayer, 0, sizeof(ovrLayerEyeFov));
		_sceneLayer.Header.Type = ovrLayerType_EyeFov;
		_sceneLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

		ovr::for_each_eye([&](ovrEyeType eye) {
			ovrEyeRenderDesc& erd = _eyeRenderDescs[eye] = ovr_GetRenderDesc(_session, eye, _hmdDesc.DefaultEyeFov[eye]);
			ovrMatrix4f ovrPerspectiveProjection =
				ovrMatrix4f_Projection(erd.Fov, 0.01f, 1000.0f, ovrProjection_ClipRangeOpenGL);
			_eyeProjections[eye] = ovr::toGlm(ovrPerspectiveProjection);
			_viewScaleDesc.HmdToEyePose[eye] = erd.HmdToEyePose;

			ovrFovPort & fov = _sceneLayer.Fov[eye] = _eyeRenderDescs[eye].Fov;
			auto eyeSize = ovr_GetFovTextureSize(_session, eye, fov, 1.0f);
			_sceneLayer.Viewport[eye].Size = eyeSize;
			_sceneLayer.Viewport[eye].Pos = { (int)_renderTargetSize.x, 0 };

			_renderTargetSize.y = std::max(_renderTargetSize.y, (uint32_t)eyeSize.h);
			_renderTargetSize.x += eyeSize.w;
		});
		// Make the on screen window 1/4 the resolution of the render target
		_mirrorSize = _renderTargetSize;
		_mirrorSize /= 4;

		_viewScaleDescBase = _viewScaleDesc;
		//_viewScaleDesc.HmdToEyePose[ovrEye_Left].Position.x = -.08f;
		//_viewScaleDesc.HmdToEyePose[ovrEye_Right].Position.x = .08f;
	}

protected:
	enum EYE_RENDER_STATE {
		BOTH, MONO, RIGHT, LEFT, SWITCHED
	};
	const std::map<EYE_RENDER_STATE, EYE_RENDER_STATE> eyeRenderMap {
		{BOTH, MONO},
		{MONO, RIGHT},
		{RIGHT, LEFT},
		{LEFT, SWITCHED},
		{SWITCHED, BOTH}
	};
	EYE_RENDER_STATE curEyeRenderState = BOTH;

	GLFWwindow * createRenderingTarget(uvec2 & outSize, ivec2 & outPosition) override {
		return glfw::createWindow(_mirrorSize);
	}

	void initGl() override {
		GlfwApp::initGl();

		// Disable the v-sync for buffer swap
		glfwSwapInterval(0);

		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = _renderTargetSize.x;
		desc.Height = _renderTargetSize.y;
		desc.MipLevels = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleCount = 1;
		desc.StaticImage = ovrFalse;
		ovrResult result = ovr_CreateTextureSwapChainGL(_session, &desc, &_eyeTexture);
		_sceneLayer.ColorTexture[0] = _eyeTexture;
		if (!OVR_SUCCESS(result)) {
			FAIL("Failed to create swap textures");
		}

		int length = 0;
		result = ovr_GetTextureSwapChainLength(_session, _eyeTexture, &length);
		if (!OVR_SUCCESS(result) || !length) {
			FAIL("Unable to count swap chain textures");
		}
		for (int i = 0; i < length; ++i) {
			GLuint chainTexId;
			ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, i, &chainTexId);
			glBindTexture(GL_TEXTURE_2D, chainTexId);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		// Set up the framebuffer object
		glGenFramebuffers(1, &_fbo);
		glGenRenderbuffers(1, &_depthBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
		glBindRenderbuffer(GL_RENDERBUFFER, _depthBuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, _renderTargetSize.x, _renderTargetSize.y);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		ovrMirrorTextureDesc mirrorDesc;
		memset(&mirrorDesc, 0, sizeof(mirrorDesc));
		mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		mirrorDesc.Width = _mirrorSize.x;
		mirrorDesc.Height = _mirrorSize.y;
		if (!OVR_SUCCESS(ovr_CreateMirrorTextureGL(_session, &mirrorDesc, &_mirrorTexture))) {
			FAIL("Could not create mirror texture");
		}
		glGenFramebuffers(1, &_mirrorFbo);

		// FIXME: find a better place for this
		av = std::make_unique<AvatarHandler>(_session);
	}

	void onKey(int key, int scancode, int action, int mods) override {
		if (GLFW_PRESS == action) switch (key) {
		case GLFW_KEY_R:
			ovr_RecenterTrackingOrigin(_session);
			return;
		}

		GlfwApp::onKey(key, scancode, action, mods);
	}

	int curIndex;
	GLuint curTexId;
	GLuint mirrorTextureId;
	void draw() final override {
		ovrPosef eyePoses[2], beyePoses[2];
		ovr_GetEyePoses(_session, frame, true, _viewScaleDesc.HmdToEyePose, eyePoses, &_sceneLayer.SensorSampleTime);
		beyePoses[0] = eyePoses[0];
		beyePoses[1] = eyePoses[1];

		handleInput();

		// copy into buffer
		// TODO: determine if copy wants the address for the 3rd argument
		// TODO: do we need to also delay the controllers?
		std::copy(eyePoses, eyePoses + 2, ringIdx(headBuffer, headPointer).begin());
		std::cerr << "Tracking lag: " << lag << " frames" << std::endl;
		std::cerr << "Rendering delay: " << delay << " frames" << std::endl;
		std::copy(ringIdx(headBuffer, headPointer - lag).begin(), ringIdx(headBuffer, headPointer - lag).end(), eyePoses);
		headPointer = incRingHead(headBuffer, headPointer);

		bool render = true;
		if (delay) {
			if (currentDelay < delay) {
				render = false;
				currentDelay++;
			}
			else {
				currentDelay = 0;
			}
		}
		else if (currentDelay != 0) {
			currentDelay = 0;
		}

		//int curIndex;
		ovr_GetTextureSwapChainCurrentIndex(_session, _eyeTexture, &curIndex);
		//GLuint curTexId;
		ovr_GetTextureSwapChainBufferGL(_session, _eyeTexture, curIndex, &curTexId);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (render) {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			ovr::for_each_eye([&](ovrEyeType eye) {
				if ((curEyeRenderState == RIGHT && eye == ovrEye_Left) ||
					(curEyeRenderState == LEFT && eye == ovrEye_Right)) {
					return;
				}

				const auto& vp = _sceneLayer.Viewport[eye];
				glViewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);

				if (curEyeRenderState == SWITCHED) {
					if (eye == ovrEye_Left)
						eye = ovrEye_Right;
					else
						eye = ovrEye_Left;
				}
				else if (curEyeRenderState == MONO && eye == ovrEye_Right) {
					eye = ovrEye_Left;
				}
				//_sceneLayer.RenderPose[eye] = eyePoses[eye];
				_sceneLayer.RenderPose[eye] = beyePoses[eye];

				// avatar stuff, don't really want to touch, put in own space to avoid potential conflicts
				// render hands first?
				{
					ovrVector3f eyePosition = eyePoses[eye].Position;
					ovrQuatf eyeOrientation = eyePoses[eye].Orientation;
					glm::quat glmOrientation = ovr::toGlm(eyeOrientation);
					glm::vec3 eyeWorld = ovr::toGlm(eyePosition);
					glm::vec3 eyeForward = glmOrientation * glm::vec3(0, 0, -1);
					glm::vec3 eyeUp = glmOrientation * glm::vec3(0, 1, 0);
					glm::mat4 view = glm::lookAt(eyeWorld, eyeWorld + eyeForward, eyeUp);
					av->updateAvatar(_eyeProjections[eye], view, eyeWorld);
				}

				//renderScene(_eyeProjections[eye], ovr::toGlm(eyePoses[eye])); // score on hand
				renderScene(_eyeProjections[eye], ovr::toGlm(eyePoses[eye]), eye); // score on hand
				//renderScene(_eyeProjections[eye], ovr::toGlm(eyePoses[eye]), eyePoses[eye]); // score in hud
			});
		}
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		ovr_CommitTextureSwapChain(_session, _eyeTexture);
		ovrLayerHeader* headerList = &_sceneLayer.Header;
		ovr_SubmitFrame(_session, frame, &_viewScaleDesc, &headerList, 1);

		//GLuint mirrorTextureId;
		ovr_GetMirrorTextureBufferGL(_session, _mirrorTexture, &mirrorTextureId);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _mirrorFbo);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTextureId, 0);
		glBlitFramebuffer(0, 0, _mirrorSize.x, _mirrorSize.y, 0, _mirrorSize.y, _mirrorSize.x, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	void incIPD() {
		_viewScaleDesc.HmdToEyePose[ovrEye_Left].Position.x -= .005f;
		_viewScaleDesc.HmdToEyePose[ovrEye_Right].Position.x += .005f;
		if (_viewScaleDesc.HmdToEyePose[ovrEye_Right].Position.x > maxIPD) {
			_viewScaleDesc.HmdToEyePose[ovrEye_Left].Position.x = -maxIPD;
			_viewScaleDesc.HmdToEyePose[ovrEye_Right].Position.x = maxIPD;
		}
	}

	void decIPD() {
		_viewScaleDesc.HmdToEyePose[ovrEye_Left].Position.x += .005f;
		_viewScaleDesc.HmdToEyePose[ovrEye_Right].Position.x -= .005f;
		if (_viewScaleDesc.HmdToEyePose[ovrEye_Right].Position.x < minIPD) {
			_viewScaleDesc.HmdToEyePose[ovrEye_Left].Position.x = -minIPD;
			_viewScaleDesc.HmdToEyePose[ovrEye_Right].Position.x = minIPD;
		}
	}

	void resetIPD() {
		_viewScaleDesc = _viewScaleDescBase;
	}

	void incLag() {
		lag++;
	}

	void decLag() {
		lag--;
		if (lag < 0)
			lag = 0;
	}

	void resetLag() {
		lag = 0;
	}

	int getlag() {
		return lag;
	}

	void incDelay() {
		delay++;
		if (delay > 10)
			delay = 10;
	}

	void decDelay() {
		delay--;
		if (delay < 0)
			delay = 0;
	}

	//void update() {}
	virtual void handleInput() = 0;
	virtual void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose) = 0;
	virtual void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose, ovrEyeType eye) = 0;
	//virtual void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose, const ovrPosef camera) = 0;
	//virtual void renderScene(const glm::mat4 & projection, const glm::mat4 & headPose, const glm::vec3 & headPos) = 0;
};