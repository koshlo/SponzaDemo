
/* * * * * * * * * * * * * Author's note * * * * * * * * * * * *\
*   _       _   _       _   _       _   _       _     _ _ _ _   *
*  |_|     |_| |_|     |_| |_|_   _|_| |_|     |_|  _|_|_|_|_|  *
*  |_|_ _ _|_| |_|     |_| |_|_|_|_|_| |_|     |_| |_|_ _ _     *
*  |_|_|_|_|_| |_|     |_| |_| |_| |_| |_|     |_|   |_|_|_|_   *
*  |_|     |_| |_|_ _ _|_| |_|     |_| |_|_ _ _|_|  _ _ _ _|_|  *
*  |_|     |_|   |_|_|_|   |_|     |_|   |_|_|_|   |_|_|_|_|    *
*                                                               *
*                     http://www.humus.name                     *
*                                                                *
* This file is a part of the work done by Humus. You are free to   *
* use the code in any way you like, modified, unmodified or copied   *
* into your own work. However, I expect you to respect these points:  *
*  - If you use this file and its contents unmodified, or use a major *
*    part of this file, please credit the author and leave this note. *
*  - For use in anything commercial, please request my approval.     *
*  - Share your work and ideas too as much as you can.             *
*                                                                *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "App.h"
#include <array>

#include "../RenderFramework/Util/ResourceLoader.h"
#include "../RenderFramework/RenderQueue.h"
#include "../RenderFramework/StateHelper.h"
#include "../RenderFramework/Util/SceneObject.h"
#include "../RenderFramework/Util/Helpers.h"
#include "../RenderFramework/Shaders/GaussianBlur.data.fx"

BaseApp *app = new App();

bool App::onKey(const uint key, const bool pressed)
{
	if (D3D11App::onKey(key, pressed))
		return true;

	if (pressed)
	{
		if (key >= KEY_F5 && key <= KEY_F8)
		{
			return true;
		}
	}

	return false;
}

void App::moveCamera(const float3 &dir)
{
	float3 newPos = camPos + dir * (speed * frameTime);

	camPos = newPos;
}

void App::resetCamera()
{
	camPos = vec3(5, 4, -200);
	wx = 0.44f;
	wy = 0.96f;
}

void App::onSize(const int w, const int h)
{
	D3D11App::onSize(w, h);
	
	const float near_plane = 0.01f;
	const float far_plane = 5000.0f;

	m_projectionMatrix = toD3DProjection(perspectiveMatrixY(1.2f, w, h, far_plane, near_plane));
	if (gfxDevice)
	{
		// Make sure render targets are the size of the window
		//gfxDevice->resizeRenderTarget(m_BaseRT,      w, h, 1, 1, 1);
		//gfxDevice->resizeRenderTarget(m_NormalRT,    w, h, 1, 1, 1);
		gfxDevice->resizeRenderTarget(m_VarianceMap,     w, h, 1, 1, 1);
	}
}

bool App::init()
{
	// No framework created depth buffer
	depthBits = 0;
	speed = 100;

	float dialogW = 400, dialogH = 200;
	m_paramDialog = new Dialog(width-dialogW, 0, dialogW, dialogH, false, false);
	int tab = m_paramDialog->addTab("Test");
	m_paramDialog->addWidget(tab, new Slider(10, 0, 100, 20, 0, 1, 0));
	widgets.addFirst(m_paramDialog);

	return true;
}

void App::exit()
{
}

bool App::initAPI()
{
	return D3D11App::initAPI(D3D11, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D32_FLOAT, 1, NO_SETTING_CHANGE | SAMPLE_BACKBUFFER);
}

void App::exitAPI()
{
	D3D11App::exitAPI();
}

const int ShadowMapResolution = 1024;

bool App::load()
{
	const char* modelName = "sponza.obj";
	String modelPath;
	modelPath.sprintf("%s%s", RenderResourceLoader::DataPath(), modelName);
	RenderResourceLoader loader(*gfxDevice);
	if (!m_Scene.Load(modelPath.dataPtr(), loader, *shaderCache, *renderStateCache)) return false;

	// Shaders
	if ((m_renderVarianceMap = gfxDevice->addShader("../RenderFramework/Shaders/RenderDepthMoments.fx")) == SHADER_NONE) return false;
    if ((m_gausBlurCompute = gfxDevice->addComputeShader("../RenderFramework/Shaders/GaussianBlur.fx")) == SHADER_NONE) return false;
    if ((m_tonemap = gfxDevice->addShader("../RenderFramework/Shaders/BlitTonemap.fx")) == SHADER_NONE) return false;

	if ((m_pointClamp = gfxDevice->addSamplerState(NEAREST, CLAMP, CLAMP, CLAMP)) == SS_NONE) return false;
	if ((m_bilinearSampler = gfxDevice->addSamplerState(BILINEAR, CLAMP, CLAMP, CLAMP)) == SS_NONE) return false;

    SamplerStateID materialSampler = gfxDevice->addSamplerState(BILINEAR, WRAP, WRAP, WRAP);
    ASSERT(materialSampler != -1);
    m_lightShaderData.SetMaterialSampler(materialSampler);

    // Main render targets
	FORMAT shadowMapFormat = FORMAT_RG32F;
    if ((m_VarianceMap = gfxDevice->addRenderTarget (ShadowMapResolution, ShadowMapResolution, shadowMapFormat, SS_NONE, 0)) == TEXTURE_NONE) return false;
    if ((m_blurredVarianceTargets[0] = gfxDevice->addRenderTarget(ShadowMapResolution, ShadowMapResolution, shadowMapFormat, SS_NONE, ADD_UAV)) == TEXTURE_NONE) return false;
	if ((m_blurredVarianceTargets[1] = gfxDevice->addRenderTarget(ShadowMapResolution, ShadowMapResolution, shadowMapFormat, SS_NONE, ADD_UAV)) == TEXTURE_NONE) return false;

	if ((m_DepthRT = gfxDevice->addRenderDepth(width, height, 1, FORMAT_D16, 1, SS_NONE, 0)) == TEXTURE_NONE) return false;
    if ((m_VarianceMapDepthRT = gfxDevice->addRenderDepth(ShadowMapResolution, ShadowMapResolution, 1, FORMAT_D16, 1)) == TEXTURE_NONE) return false;

    TextureID hdrColor = gfxDevice->addRenderTarget(width, height, FORMAT_RGBA16F);

    const TextureID forwardRTs[] = { hdrColor };
    m_forwardQueue.reset(new RenderQueue(gfxDevice, forwardRTs, array_size(forwardRTs), FB_DEPTH));
    m_forwardQueue->AddShaderData(&m_viewShaderData);
    m_forwardQueue->AddShaderData(&m_lightShaderData);
    m_forwardQueue->AddShaderData(&m_shadowShaderData);
    m_forwardQueue->AddShaderData(&m_expWarpingData);
    m_forwardQueue->SetClear(true, true, float4(0.2f, 0.2f, 1.0f, 0), 0.0f);

    const TextureID shadowRTs[] = { m_VarianceMap };
    m_shadowQueue.reset(new RenderQueue(gfxDevice, shadowRTs, array_size(shadowRTs), m_VarianceMapDepthRT));
    m_shadowQueue->AddShaderData(&m_viewShaderData);
    m_shadowQueue->AddShaderData(&m_expWarpingData);
    m_shadowQueue->SetClear(true, true, float4(0, 0, 0, 0), 0.0f);

    RenderStateDesc shadowDesc{ m_renderVarianceMap, true, true, GEQUAL, CULL_BACK };
    m_shadowState = renderStateCache->GetRenderState(shadowDesc);

	return true;
}

void App::unload()
{
}

const float ExponentialWarpPower = 5.0f;

TextureID makeBlurPass(StateHelper* stateHelper, ShaderID shader, TextureID srcTexture, TextureID* destTextures, float2 texSize)
{
	const float sigma = 3.0f;
	const float sigmaSqr = sigma * sigma;
	const float gaussMultiplier = 1.0f / (sqrtf(2 * PI * sigmaSqr));
	const uint kernelHalfSize = static_cast<uint>(sigma * 3.0f);

	std::vector<float> weights(kernelHalfSize);
	for (uint i = 0; i < kernelHalfSize; ++i)
	{
		float x = static_cast<float>(i);
		weights[i] = gaussMultiplier * expf( -(x * x) / (2 * sigmaSqr) );
	}

	static const float2 HorizontalStep(1, 0);
	static const float2 VerticalStep(0, 1);

	BlurShaderData blurShaderParams;
	blurShaderParams.SetSourceTexture(srcTexture);
	blurShaderParams.SetDestTexture(destTextures[0]);
	blurShaderParams.SetSampleParams(float4(texSize, HorizontalStep));
	blurShaderParams.SetBlurHalfSize(weights.size() - 1);
	blurShaderParams.SetWeightsRaw(weights.data(), weights.size() * sizeof(float));

	ExpWarpingShaderData warpShaderParams;
    warpShaderParams.SetExpPower(float4(ExponentialWarpPower));

	const uint nThreadsX = 16, nThreadsY = 16;
	DispatchGroup dispatchGroup;
	dispatchGroup.x = ShadowMapResolution / nThreadsX;
	dispatchGroup.y = ShadowMapResolution / nThreadsY;
	dispatchGroup.z = 1;

	const ShaderData* shaderData[] = { &blurShaderParams, &warpShaderParams };

	RenderQueue::DispatchCompute(stateHelper, dispatchGroup, shader, shaderData, array_size(shaderData));
	
	blurShaderParams.SetSampleParams(float4(texSize, VerticalStep));
	blurShaderParams.SetSourceTexture(destTextures[0]);
	blurShaderParams.SetDestTexture(destTextures[1]);
	
	RenderQueue::DispatchCompute(stateHelper, dispatchGroup, shader, shaderData, array_size(shaderData));

	return destTextures[1];
}

void App::drawFrame()
{
	mat4 lightViewProj = renderDepthMapPass();
    m_shadowQueue->SubmitAll(gfxDevice, stateHelper);
	float shadowMapRes = ShadowMapResolution;
	TextureID blurredVariance =
		makeBlurPass(stateHelper, m_gausBlurCompute, m_VarianceMap, m_blurredVarianceTargets, float2(shadowMapRes, shadowMapRes));
	renderForwardPass(lightViewProj, blurredVariance);
    m_forwardQueue->SubmitAll(gfxDevice, stateHelper);
    RenderQueue::Blit(stateHelper, renderStateCache, m_tonemap, m_forwardQueue->GetRenderTarget(0), FB_COLOR);
}

const float3 SunDirection = normalize(float3(-0.4f, -1.0f, 0.3f));
const float3 SunIntensity = float3(20.0f, 20.0f, 18.0f);
const float3 Ambient = float3(0.2f, 0.2f, 0.3f);

mat4 App::renderDepthMapPass()
{
	AABB boundingBox = m_Scene.GetBoundingBox();

	AABB::PointArray bbPoints = boundingBox.GetPoints();

	mat3 lightRotation = lookAtRotation(vec3(0, 0, 0), SunDirection, vec3(0, 1, 0));
	AABB lightBB = RotateAABB(boundingBox, lightRotation);
	const float nearFarBias = 2.5f;
	mat4 lightProjection = toD3DProjection(orthoMatrixX(lightBB.GetLeft(), lightBB.GetRight(),
		lightBB.GetTop(), lightBB.GetBottom(),
		lightBB.GetFront() - nearFarBias, lightBB.GetBack() + nearFarBias));

	mat4 lightView(lightRotation, vec4(0, 0, 0, 1));
	mat4 lightViewProj = lightProjection * lightView;

    m_viewShaderData.SetViewProjection(lightViewProj);
    m_expWarpingData.SetExpPower(ExponentialWarpPower);

    m_Scene.Draw(*m_shadowQueue, &m_shadowState, 0);

	return lightViewProj;
}

void App::renderForwardPass(const mat4& lightViewProj, TextureID varianceMap)
{
	float4x4 view = rotateXY(-wx, -wy);
	view.translate(-camPos);
	float4x4 viewProj = m_projectionMatrix * view;

	m_viewShaderData.SetViewProjection(viewProj);
	m_viewShaderData.SetViewport(vec2(static_cast<float>(width), static_cast<float>(height)));
    m_viewShaderData.SetEyePos(vec4(camPos, 1.0f));
	m_lightShaderData.SetSunDirection(-SunDirection);
	m_lightShaderData.SetSunIntensity(SunIntensity);
	m_lightShaderData.SetAmbient(Ambient);
	
	m_shadowShaderData.SetVarianceSampler(m_bilinearSampler);
	m_shadowShaderData.SetShadowMap(varianceMap);
	m_shadowShaderData.SetShadowMapProjection(lightViewProj);

    m_expWarpingData.SetExpPower(ExponentialWarpPower);

    m_Scene.Draw(*m_forwardQueue, 0);
}

