
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
#include "../RenderFramework/Shaders/Tonemap.data.fx"
#include "../RenderFramework/GUI/VectorSlider.h"
#include "../RenderFramework/GUI/Button.h"
#include "../RenderFramework/IrradianceRenderer.h"
 
BaseApp *app = new App();

const float ExponentialWarpPower = 1.0f;
const float3 SunDirection = normalize(float3(-0.4f, -1.0f, 0.3f));
const float3 SunIntensity = float3(100.0f, 100.0f, 80.0f);
const float Exposure = 0.05f;

class ProbeButtonListener : public PushButtonListener
{
public:
    ProbeButtonListener() : _pendingPress(true) {}

    void reset()
    {
        _pendingPress = false;
    }

    void onButtonClicked(PushButton *button) override
    {
        _pendingPress = true;
    }

    bool isPressedThisFrame() const { return _pendingPress; }
private:
    bool _pendingPress;
};

struct GUIElements
{
    static const float DialogW;
    static const float DialogH;

    Dialog* paramDialog;
    VectorSlider* sunLightSlider;
    Label* sunLightLabel;
    VectorSlider* sunDirSlider;
    Label* sunDirLabel;
    Slider* exposureSlider;
    Label* exposureLabel;
    Label* cameraPosLabel;
    PushButton* probeButton;
    ProbeButtonListener probeButtonListener;

    GUIElements(int screenW, int screenH) :
        paramDialog(new Dialog(screenW - DialogW - 10, 10, DialogW, DialogH, false, true)),
        sunLightSlider(new VectorSlider(10, 10, 400, 20, 50.0f, 200.0f, SunIntensity)),
        sunLightLabel(new Label(10, 30, 300, 30, "")),
        sunDirSlider(new VectorSlider(10, 70, 400, 20, -1.0f, 1.0f, SunDirection)),
        sunDirLabel(new Label(10, 90, 400, 30, "")),
        exposureSlider(new Slider(10, 140, 250, 20, 0.0f, 1.0f, Exposure)),
        exposureLabel(new Label(265, 135, 150, 30, "")),
        cameraPosLabel(new Label(10, 10, 350, 30, "")),
        probeButton(new PushButton(10, 180, 200, 40, "Recompute probes"))
    {
        int lightingTab = paramDialog->addTab("Lighting");
        int sceneTab = paramDialog->addTab("Scene");
        paramDialog->addWidget(lightingTab, sunLightSlider);
        paramDialog->addWidget(lightingTab, sunLightLabel);
        paramDialog->addWidget(lightingTab, sunDirSlider);
        paramDialog->addWidget(lightingTab, sunDirLabel);
        paramDialog->addWidget(lightingTab, exposureSlider);
        paramDialog->addWidget(lightingTab, exposureLabel);
        paramDialog->addWidget(lightingTab, probeButton);

        paramDialog->addWidget(sceneTab, cameraPosLabel);

        probeButton->setListener(&probeButtonListener);
    }
};

const float GUIElements::DialogW = 450.0f;
const float GUIElements::DialogH = 400.0f;

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

    m_gui = new GUIElements(width, height);
	widgets.addFirst(m_gui->paramDialog);

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

    m_irradianceRenderer.reset(new IrradianceRenderer(gfxDevice, renderStateCache, stateHelper));

	// Shaders
	if ((m_renderVarianceMap = gfxDevice->addShader("../RenderFramework/Shaders/RenderDepthMoments.fx")) == SHADER_NONE) return false;
    if ((m_gausBlurCompute = gfxDevice->addComputeShader("../RenderFramework/Shaders/GaussianBlur.fx")) == SHADER_NONE) return false;
    if ((m_tonemap = gfxDevice->addShader("../RenderFramework/Shaders/BlitTonemap.fx")) == SHADER_NONE) return false;

	if ((m_pointClamp = gfxDevice->addSamplerState(NEAREST, CLAMP, CLAMP, CLAMP)) == SS_NONE) return false;
	if ((m_bilinearSampler = gfxDevice->addSamplerState(BILINEAR, CLAMP, CLAMP, CLAMP)) == SS_NONE) return false;

    SamplerStateID materialSampler = gfxDevice->addSamplerState(TRILINEAR, WRAP, WRAP, WRAP);
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

vec3 probeLocations[] = 
{
    vec3{ -342, 115, 121 }, 
    vec3{ -20, 115, 121 },
    vec3{ 350, 115, 121 },
    vec3{ 428, 212, 425 },
    vec3{ -128, 212, 425 },
    vec3{ -767, 212, 425 }
};

void setupIrradianceData(RenderStateCache* stateCache, TextureID irradianceMap, LightShaderData* lightShaderData)
{
    SamplerStateDesc linearSamplerDesc{ LINEAR, CLAMP, CLAMP, CLAMP };
    SamplerStateID irradianceSampler = stateCache->GetSamplerState(linearSamplerDesc);
    lightShaderData->SetIrradianceSampler(irradianceSampler);
    lightShaderData->SetIrradianceProbes(irradianceMap);

    static const uint ProbeCount = array_size(probeLocations);
    vec4 probeLocations4[ProbeCount];
    for (uint i = 0; i < ProbeCount; ++i)
    {
        probeLocations4[i] = vec4(probeLocations[i], 1.0f);
    }
    lightShaderData->SetProbeCount(ProbeCount);
    lightShaderData->SetProbeLocations(probeLocations4, ProbeCount);
}

void App::drawFrame()
{
    updateGUI();

    m_lightShaderData.SetSunDirection(-m_gui->sunDirSlider->getValue());
    m_lightShaderData.SetSunIntensity(m_gui->sunLightSlider->getValue());

    m_expWarpingData.SetExpPower(ExponentialWarpPower);

    mat4 lightViewProj = renderDepthMapPass();
    m_shadowQueue->SubmitAll(gfxDevice, stateHelper);
    float shadowMapRes = ShadowMapResolution;
    TextureID blurredVariance =
        makeBlurPass(stateHelper, m_gausBlurCompute, m_VarianceMap, m_blurredVarianceTargets, float2(shadowMapRes, shadowMapRes));

    m_shadowShaderData.SetVarianceSampler(m_bilinearSampler);
    m_shadowShaderData.SetShadowMap(blurredVariance);
    m_shadowShaderData.SetShadowMapProjection(lightViewProj);

    if (m_gui->probeButtonListener.isPressedThisFrame())
    {
        Scene scene;
        scene.objects = &m_Scene;
        scene.numObjects = 1;
        scene.lightShaderData = &m_lightShaderData;
        scene.shadowShaderData = &m_shadowShaderData;
        scene.expWarpingData = &m_expWarpingData;

        m_irradianceMapArray = m_irradianceRenderer->BakeProbes(probeLocations, array_size(probeLocations), 256, scene, 3);

        m_gui->probeButtonListener.reset();
    }
    m_irradianceRenderer->DrawDebugSpheres(*m_forwardQueue);

    setupIrradianceData(renderStateCache, m_irradianceMapArray, &m_lightShaderData);

	renderForwardPass(lightViewProj, blurredVariance);

    m_forwardQueue->SubmitAll(gfxDevice, stateHelper);

    TonemapShaderData tonemapData;
    tonemapData.SetExposure(m_gui->exposureSlider->getValue());
    const ShaderData* blitData[] = { &tonemapData };
    RenderQueue::Blit(stateHelper, m_tonemap, blitData, array_size(blitData), m_forwardQueue->GetRenderTarget(0), m_pointClamp, FB_COLOR);
}

mat4 App::renderDepthMapPass()
{
	AABB boundingBox = m_Scene.GetBoundingBox();

	AABB::PointArray bbPoints = boundingBox.GetPoints();

	mat3 lightRotation = lookAtRotation(vec3(0, 0, 0), m_gui->sunDirSlider->getValue(), vec3(0, 1, 0));
	AABB lightBB = RotateAABB(boundingBox, lightRotation);
	const float farBias = 2000.0f; // TODO fix projection computation
	mat4 lightProjection = toD3DProjection(orthoMatrixX(lightBB.GetLeft(), lightBB.GetRight(),
		lightBB.GetTop(), lightBB.GetBottom(),
		lightBB.GetFront(), lightBB.GetBack() + farBias));

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

    m_Scene.Draw(*m_forwardQueue, 0);
}

void App::updateGUI()
{
    char fmtBuffer[256];
    float3 fIntensity = m_gui->sunLightSlider->getValue();
    ::sprintf(fmtBuffer, "Sun intensity: (%.1f %.1f %.1f)", fIntensity.x, fIntensity.y, fIntensity.z);
    m_gui->sunLightLabel->setText(fmtBuffer);

    float3 fDir = m_gui->sunDirSlider->getValue();
    ::sprintf(fmtBuffer, "Sun direction: (%.2f %.2f %.2f", fDir.x, fDir.y, fDir.z);
    m_gui->sunDirLabel->setText(fmtBuffer);

    float fExposure = m_gui->exposureSlider->getValue();
    ::sprintf(fmtBuffer, "Exposure: %.3f", fExposure);
    m_gui->exposureLabel->setText(fmtBuffer);

    ::sprintf(fmtBuffer, "Camera position: %.1f %.1f %1.f", camPos.x, camPos.y, camPos.z);
    m_gui->cameraPosLabel->setText(fmtBuffer);
}

