
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

#include "../RenderFramework/Direct3D11/D3D11App.h"
#include "../RenderFramework/Util/BSP.h"
#include "../RenderFramework/Util/SceneObject.h"
#include "../RenderFramework/Shaders/Scene.data.fx"
#include "../RenderFramework/Shaders/Shadow.data.fx"

#include <memory>

struct Light
{
	float3 Position;
	float  Radius;
};

class App : public D3D11App
{
public:
	char *getTitle() const { return "VSM"; }

	bool onKey(const uint key, const bool pressed);

	void moveCamera(const float3 &dir);
	void resetCamera();

	void onSize(const int w, const int h);

	bool init();
	void exit();

	bool initAPI();
	void exitAPI();

	bool load();
	void unload();

	void drawFrame();

protected:
	mat4 renderDepthMapPass();
	void renderForwardPass(const mat4& lightViewProjection, TextureID varianceMap);

	mat4 m_projectionMatrix;
	TextureID m_BaseRT, m_NormalRT, m_DepthRT, m_VarianceMap, m_VarianceMapDepthRT;
	ShaderID m_renderVarianceMap, m_forwardGeometry, m_deferredLighting;
    ShaderID m_gausBlurCompute;
    TextureID m_blurredVarianceTargets[2];
	SamplerStateID m_pointClamp;
    SamplerStateID m_bilinearSampler;

	SceneShaderData m_sceneShaderData;
    ShadowShaderData m_shadowShaderData;

	SceneObject m_Scene;
	Model m_Sphere;

	Dialog* m_paramDialog;

    std::unique_ptr<RenderQueue> m_forwardQueue;
};
