#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_METALLROUGHNESS_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_WORKFLOW_SPECGLOSS, SHADOWMAP_DEBUG)

#define MATERIAL_DESCRIPTOR_SET 2

layout(
    input_attachment_index = 1,  // 强制要求：子通道输入附件列表中的索引
    set = MATERIAL_DESCRIPTOR_SET,  // 和 CPU 侧一致（比如 2）
    binding = 0  // 和 CPU 侧一致（比如 0）
) uniform subpassInputMS colorInputAttachment;  // color attachment 1（法线）
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2DMS samplerSSAO;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec4 LINEARtoSRGB(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return vec4(linOut, srgbIn.w);
}

void main()
{
    vec4 colorAttachment = subpassLoad(colorInputAttachment, gl_SampleID);
    vec3 colorData = colorAttachment.xyz;
    vec2 uv = inUV * 0.5 + 0.5;

    ivec2 textureSize = textureSize(samplerSSAO);
    vec2 texelSize = 1.0 / vec2(textureSize);
    float result = 0.0;
    for (int x = -2; x <= 2; ++x) 
    {
        for (int y = -2; y <= 2; ++y) 
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texelFetch(samplerSSAO, ivec2((uv + offset)*textureSize), 0).r;
        }
    }
    if(texelFetch(samplerSSAO, ivec2(uv*textureSize), 0).w > 0.5){
        outColor = vec4(colorData, 1);
    }else{
        float exposure = 8.0f;
        vec3 color = Uncharted2Tonemap(colorData * result / 25.0 * result / 25.0 * exposure);
        // vec3 color = Uncharted2Tonemap(vec3(result / 25.0 * result / 25.0 * exposure));
        color = color * (vec3(1.0f) / Uncharted2Tonemap(vec3(11.2f)));
        outColor = LINEARtoSRGB(vec4(color, 1));
    }

    return;
}
