
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
#include "../Framework3/Util/ResourceLoader.h"
#include <array>

BaseApp *app = new App();

class AABB
{
public:
	AABB(const float* minCoord, const float* maxCoord) :
		_left(minCoord[0]),
		_bottom(minCoord[1]),
		_front(minCoord[2])
	{
		SetRight(maxCoord[0]);
		SetTop(maxCoord[1]);
		SetBack(maxCoord[2]);
	}

	float GetLeft() const { return _left; }
	float GetBottom() const { return _bottom; }
	float GetFront() const { return _front; }
	float GetRight() const { return _left + _width; }
	float GetTop() const { return _bottom + _height; }
	float GetBack() const { return _front + _depth; }

	void SetRight(float right) { _width = right - _left; }
	void SetTop(float top) { _height = top - _bottom; }
	void SetBack(float back) { _depth = back - _front; }

	typedef std::array<vec3, 8> PointArray;
	PointArray GetPoints() const
	{
		PointArray points = {
			vec3(_left, _bottom, _front),
			vec3(_left, _bottom, GetBack()),
			vec3(_left, GetTop(), _front),
			vec3(_left, GetTop(), GetBack()),
			vec3(GetRight(), _bottom, _front),
			vec3(GetRight(), _bottom, GetBack()),
			vec3(GetRight(), GetTop(), _front),
			vec3(GetRight(), GetTop(), GetBack())
		};
		return points;
	}
private:
	float _left, _bottom, _front;
	float _width, _height, _depth;
};

AABB RotateAABB(const AABB& aabb, mat3 rotation)
{
	AABB::PointArray points = aabb.GetPoints();
	for (AABB::PointArray::size_type i = 0; i < points.size(); ++i)
	{
		points[i] = rotation * points[i];
	}
	vec3 nearPoint = points[0];
	vec3 farPoint = points[0];
	for (AABB::PointArray::size_type i = 0; i < points.size(); ++i)
	{
		vec3 curPoint = points[i];
		for (unsigned int j = 0; j < 3; ++j)
		{
			nearPoint[j] = min(curPoint[j], nearPoint[j]);
			farPoint[j] = max(curPoint[j], farPoint[j]);
		}
	}
	return AABB(nearPoint, farPoint);
}
 

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
	
	const float near_plane = 0.1f;
	const float far_plane = 1000.0f;

	m_projectionMatrix = toD3DProjection(perspectiveMatrixY(1.2f, w, h, far_plane, near_plane));
	if (renderer)
	{
		// Make sure render targets are the size of the window
		//renderer->resizeRenderTarget(m_BaseRT,      w, h, 1, 1, 1);
		//renderer->resizeRenderTarget(m_NormalRT,    w, h, 1, 1, 1);
		renderer->resizeRenderTarget(m_VarianceMap,     w, h, 1, 1, 1);
	}
}

bool App::init()
{
	const char* modelName = "banner.obj";
	String modelPath;
	modelPath.sprintf("%s%s", RenderResourceLoader::DataPath(), modelName);
	if (!m_Map.loadObj(modelPath.dataPtr())) return false;

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
	// Override the user's MSAA settings
	return D3D11App::initAPI(D3D11, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_UNKNOWN, 1, NO_SETTING_CHANGE | SAMPLE_BACKBUFFER);
}

void App::exitAPI()
{
	D3D11App::exitAPI();
}

const int ShadowMapResolution = 512;

bool App::load()
{
	const MaterialLibResId& matName = m_Map.getMaterialLib();
	bool isPbr = false;
	RenderResourceLoader loader(*renderer);
	if (matName.isValid())
	{
		loader.SyncLoad(matName, isPbr, &m_mapMaterials);
	}
	if (!m_mapMaterials)
	{
		return false;
	}

	// Shaders
	if ((m_renderVarianceMap = renderer->addShader("RenderDepthMoments.shd")) == SHADER_NONE) return false;
	if ((m_forwardGeometry = renderer->addShader("ForwardGeometry.shd")) == SHADER_NONE) return false;
	if ((m_deferredLighting	= renderer->addShader("DeferredLighting.shd")) == SHADER_NONE) return false;
    if ((m_gausBlurCompute = renderer->addComputeShader("GaussianBlur.csd")) == SHADER_NONE) return false;

	if ((m_pointClamp = renderer->addSamplerState(NEAREST, CLAMP, CLAMP, CLAMP)) == SS_NONE) return false;
	if ((m_bilinearSampler = renderer->addSamplerState(BILINEAR, CLAMP, CLAMP, CLAMP)) == SS_NONE) return false;

    // Main render targets
	FORMAT shadowMapFormat = FORMAT_RG32F;
    if ((m_VarianceMap = renderer->addRenderTarget (ShadowMapResolution, ShadowMapResolution, shadowMapFormat, SS_NONE, 0)) == TEXTURE_NONE) return false;
    if ((m_blurredVarianceTargets[0] = renderer->addRenderTarget(ShadowMapResolution, ShadowMapResolution, shadowMapFormat, SS_NONE, ADD_UAV)) == TEXTURE_NONE) return false;
	if ((m_blurredVarianceTargets[1] = renderer->addRenderTarget(ShadowMapResolution, ShadowMapResolution, shadowMapFormat, SS_NONE, ADD_UAV)) == TEXTURE_NONE) return false;

	if ((m_DepthRT = renderer->addRenderDepth(width, height, 1, FORMAT_D16, 1, SS_NONE, 0)) == TEXTURE_NONE) return false;
    if ((m_VarianceMapDepthRT = renderer->addRenderDepth(ShadowMapResolution, ShadowMapResolution, 1, FORMAT_D16, 1)) == TEXTURE_NONE) return false;

	if ((m_DepthTest = renderer->addDepthState(true, true, GEQUAL)) == DS_NONE) return false;

	
    if (!m_Map.makeDrawable(renderer, true, m_forwardGeometry)) return false;

	return true;
}

void App::unload()
{
}

const float ExponentialWarpPower = 5.0f;

TextureID makeBlurPass(Renderer* renderer, ShaderID shader, TextureID srcTexture, TextureID* destTextures, float2 texSize)
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

    renderer->setShader(shader);
    renderer->setTexture("SourceTexture", srcTexture);
    renderer->setUnorderedAccessTexture("DestTexture", destTextures[0]);
	renderer->setShaderConstantArray1f("Weights", weights.data(), weights.size());
	renderer->setShaderConstant2f("TextureSize", texSize);
	renderer->setShaderConstant1i("BlurHalfSize", weights.size() - 1);
	renderer->setShaderConstant2f("SampleStep", float2(1, 0));
	renderer->setShaderConstant1f("ExponentialWarpPower", ExponentialWarpPower);
	renderer->apply();
	
	const uint nThreadsX = 16, nThreadsY = 16;
	const uint dispThreadsX = ShadowMapResolution / nThreadsX;
	const uint dispThreadsY = ShadowMapResolution / nThreadsY;
	renderer->dispatchCompute(dispThreadsX, dispThreadsY, 1);

	renderer->reset(RESET_UAV);
	renderer->applyTextures();

	renderer->setShaderConstant2f("SampleStep", float2(0, 1));
	renderer->setTexture("SourceTexture", destTextures[0]);
	renderer->setUnorderedAccessTexture("DestTexture", destTextures[1]);
	renderer->applyTextures();
	renderer->applyConstants();
	renderer->dispatchCompute(dispThreadsX, dispThreadsY, 1);

	renderer->reset(RESET_UAV);
	renderer->applyTextures();

	return destTextures[1];
}

void App::drawFrame()
{
	mat4 lightViewProj = renderDepthMapPass();
	float shadowMapRes = ShadowMapResolution;
	TextureID blurredVariance =
		makeBlurPass(renderer, m_gausBlurCompute, m_VarianceMap, m_blurredVarianceTargets, float2(shadowMapRes, shadowMapRes));
	renderForwardPass(lightViewProj, blurredVariance);
}

const float3 SunDirection = normalize(float3(-0.4f, -1.0f, 0.3f));
const float3 SunIntensity = float3(0.8f, 0.7f, 0.6f);
const float3 Ambient = float3(0.2f, 0.2f, 0.4f);

mat4 App::renderDepthMapPass()
{
	float aabbCoordsMin[3];
	float aabbCoordsMax[3];
	m_Map.getBoundingBox(m_Map.findStream(TYPE_VERTEX), aabbCoordsMin, aabbCoordsMax);
	AABB boundingBox(aabbCoordsMin, aabbCoordsMax);

	AABB::PointArray bbPoints = boundingBox.GetPoints();

	mat3 lightRotation = lookAtRotation(vec3(0, 0, 0), SunDirection, vec3(0, 1, 0));
	AABB lightBB = RotateAABB(boundingBox, lightRotation);
	const float nearFarBias = 2.5f;
	mat4 lightProjection = toD3DProjection(orthoMatrixX(lightBB.GetLeft(), lightBB.GetRight(),
		lightBB.GetTop(), lightBB.GetBottom(),
		lightBB.GetFront() - nearFarBias, lightBB.GetBack() + nearFarBias));

	mat4 lightView(lightRotation, vec4(0, 0, 0, 1));
	mat4 lightViewProj = lightProjection * lightView;

	renderer->changeRenderTarget(m_VarianceMap, m_VarianceMapDepthRT);
	renderer->clear(true, true, float4(0, 0, 0, 0), 0.0f);

	renderer->reset();
	renderer->setRasterizerState(cullNone);
	renderer->setDepthState(m_DepthTest);
	renderer->setShader(m_renderVarianceMap);
	renderer->setShaderConstant4x4f("ViewProj", lightViewProj);
	renderer->setShaderConstant1f("ExponentialWarpPower", ExponentialWarpPower);
	renderer->apply();

	for (uint i = 0; i < m_Map.getBatchCount(); ++i)
	{
		m_Map.drawBatch(renderer, i);
	}

    renderer->changeRenderTargets(NULL, 0, TEXTURE_NONE);

	return lightViewProj;
}

void App::renderForwardPass(const mat4& lightViewProj, TextureID varianceMap)
{
	float4x4 view = rotateXY(-wx, -wy);
	view.translate(-camPos);
	float4x4 view_proj = m_projectionMatrix * view;

	renderer->changeRenderTarget(FB_COLOR, m_DepthRT);
	renderer->clear(true, true, float4(0, 0, 0, 0), 0.0f);

	renderer->reset();
	renderer->setRasterizerState(cullBack);
	renderer->setDepthState(m_DepthTest);
	renderer->setShader(m_forwardGeometry);
	renderer->setShaderConstant1f("ExponentialWarpPower", ExponentialWarpPower);
	renderer->setShaderConstant4x4f("ViewProj", view_proj);
	renderer->setShaderConstant4x4f("ShadowMapProjection", lightViewProj);
	renderer->setShaderConstant4x4f("InvViewProjection", !view_proj);
	renderer->setShaderConstant2f("Viewport", vec2(static_cast<float>(width), static_cast<float>(height)));
	renderer->setShaderConstant3f("SunDirection", -SunDirection);
	renderer->setShaderConstant3f("SunIntensity", SunIntensity);
	renderer->setShaderConstant3f("Ambient", Ambient);
	renderer->setSamplerState("VarianceSampler", m_bilinearSampler);
	renderer->setTexture("ShadowMap", varianceMap);

	renderer->apply();

	for (uint i = 0; i < m_Map.getBatchCount(); ++i)
	{
		m_mapMaterials.getVal().applyMaterial(m_Map.getBatchMaterialName(i), renderer);
		m_Map.drawBatch(renderer, i);
	}
}

