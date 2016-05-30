#pragma once

#include <string>
#include <array>
#include <exception>
#include "../Framework3/Math/Vector.h"
#include "../Framework3/Renderer.h"

typedef std::string NameType;

namespace ParamTags
{
    enum Tags
    {
        None = 0,
        SamplerState,
        Texture,
        Count
    };
}

class ShaderParamBase
{
public:
	virtual void Apply(Renderer& renderer) const = 0;
};

template <typename T, ParamTags::Tags tag>
class ShaderParam : public ShaderParamBase
{
public:
	typedef T ValueType;

    ShaderParam() : _name(), _value() {}
	ShaderParam(const std::string& name) : _name(name) {}
	ShaderParam(const std::string& name, const T& initVal) : _name(name), _value(initVal) {}

	const NameType& Name() const { return _name; }
	const ValueType& Value() const { return _value; }

	void SetValue(const T& value) { _value = value; }
	virtual void Apply(Renderer& renderer) const override;

    bool IsValid() const { return !_name.empty(); }
private:
	std::string _name;
	T _value;
};

typedef ShaderParam<float2, ParamTags::None> Float2Param;
typedef ShaderParam<float3, ParamTags::None> Float3Param;
typedef ShaderParam<float4, ParamTags::None> Float4Param;
typedef ShaderParam<mat4, ParamTags::None>	Mat4Param;
typedef ShaderParam<SamplerStateID, ParamTags::SamplerState> SamplerStateParam;
typedef ShaderParam<TextureID, ParamTags::Texture> TextureParam;

template <>
void Float2Param::Apply(Renderer& renderer) const
{
    renderer.setShaderConstant2f(_name.c_str(), _value);
}

template <>
void Float3Param::Apply(Renderer& renderer) const
{
    renderer.setShaderConstant3f(_name.c_str(), _value);
}

template <>
void Float4Param::Apply(Renderer& renderer) const
{
    renderer.setShaderConstant4f(_name.c_str(), _value);
}

template <>
void Mat4Param::Apply(Renderer& renderer) const
{
    renderer.setShaderConstant4x4f(_name.c_str(), _value);
}

template <>
void SamplerStateParam::Apply(Renderer& renderer) const
{
    renderer.setSamplerState(_name.c_str(), _value);
}

template <>
void TextureParam::Apply(Renderer& renderer) const
{
    renderer.setTexture(_name.c_str(), _value);
}

class FrameAllocator
{
	template <typename T> T* Allocate();
	void FreeAll();
};

class ShaderRequirements
{

};

class ShaderParamMap
{
public:
	void AddParam(const ShaderParamBase* shaderParam);
	void Validate(const ShaderRequirements& requirements);
private:

};