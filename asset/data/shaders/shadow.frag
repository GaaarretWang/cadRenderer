#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma import_defines (VSG_DIFFUSE_MAP, VSG_GREYSCALE_DIFFUSE_MAP, VSG_EMISSIVE_MAP, VSG_LIGHTMAP_MAP, VSG_NORMAL_MAP, VSG_METALLROUGHNESS_MAP, VSG_SPECULAR_MAP, VSG_TWO_SIDED_LIGHTING, VSG_WORKFLOW_SPECGLOSS, SHADOWMAP_DEBUG)

#define VIEW_DESCRIPTOR_SET 1
#define MATERIAL_DESCRIPTOR_SET 2

const float PI = 3.14159265359;
const float RECIPROCAL_PI = 0.31830988618;
const float RECIPROCAL_PI2 = 0.15915494;
const float EPSILON = 1e-6;
const float c_MinRoughness = 0.04;

#ifdef VSG_DIFFUSE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 0) uniform sampler2D diffuseMap;
#endif

#ifdef VSG_METALLROUGHNESS_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 1) uniform sampler2D mrMap;
#endif

#ifdef VSG_NORMAL_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 2) uniform sampler2D normalMap;
#endif

#ifdef VSG_LIGHTMAP_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 3) uniform sampler2D aoMap;
#endif

#ifdef VSG_EMISSIVE_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 4) uniform sampler2D emissiveMap;
#endif

#ifdef VSG_SPECULAR_MAP
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 5) uniform sampler2D specularMap;
#endif

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 7) uniform sampler2D cameraImage;
layout(set = MATERIAL_DESCRIPTOR_SET, binding = 8) uniform sampler2D depthImage;

#define GLOBAL_BUFFER_SET MATERIAL_DESCRIPTOR_SET
#define GLOBAL_BUFFER_BINDING 12
#pragma include "global_buffer.glsl"

layout(set = MATERIAL_DESCRIPTOR_SET, binding = 13) uniform sampler2D shadowInputAttachment;

// ViewDependentState
layout(set = VIEW_DESCRIPTOR_SET, binding = 0) uniform LightData
{
    vec4 values[2048];
} lightData;

layout(set = VIEW_DESCRIPTOR_SET, binding = 2) uniform sampler2DArrayShadow shadowMaps;
layout(set = VIEW_DESCRIPTOR_SET, binding = 3) uniform sampler2DArray shadowMapsSampler;

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 normalDir;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec2 texCoord0;
layout(location = 4) in vec3 worldPos;
layout(location = 5) in vec3 viewDir;
layout(location = 6) in float InstanceID;
layout(location = 7) in vec3 worldNormal;
layout(location = 8) in vec3 lastWorldPos;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPos;
layout(location = 3) out vec4 outShadow;
layout(location = 4) out vec4 outMaterial;
layout(location = 5) out float outMask;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
} pc;

#pragma include "sample_data.glsl"

#define PCSS_FRAME_NUM globalBuffer.frame_num
#define PCSS_SOFTNESS globalBuffer.softness
#define PCSS_PCF_SAMPLE_NUM globalBuffer.pcf_sample_num
#define PCSS_MAX_DISTANCE_SCALE 65.0
#define PCSS_PENUMBRA_CLAMP 1.8
#define PCSS_MIN_FILTER_SCALE 10.0
#pragma include "pcss.glsl"

void main()
{
    vec2 screen_uv = vec2(gl_FragCoord.x / globalBuffer.width, gl_FragCoord.y / globalBuffer.height);
    if(globalBuffer.enable_real_depth_occlusion != 0){
        float cadDepth = -eyePos.z / globalBuffer.z_far;
        float cameraDepth = texture(depthImage, screen_uv).r;
        if(cadDepth > cameraDepth){
            outColor = texture(cameraImage, screen_uv);
            outNormal = vec4(0.0, 0.0, 0.0, 1.0);
            outWorldPos = vec4(0.0, 0.0, 0.0, 1.0);
            outShadow = vec4(1.0, -10000000.0, gl_FragCoord.z, 1.0);
            outMaterial = vec4(0.0);
            outMask = 0.0;
            return;
        }
    }

    float brightnessCutoff = 0.001;

    vec4 lightNums = lightData.values[0];
    int numAmbientLights = int(lightNums[0]);
    int numDirectionalLights = int(lightNums[1]);
    int numPointLights = int(lightNums[2]);
    int numSpotLights = int(lightNums[3]);
    int index = 1;

    float scene_brightness = 1.0f;
    int shadowMapIndex = 0;
    if (globalBuffer.shadow_mode != 0)
    {
        scene_brightness = 1.0f;
    }
    else if (numDirectionalLights>0)
    {
        float totalBrigtness = globalBuffer.baseBrightness;
        float totalRealBrightness = globalBuffer.baseBrightness;
        // directional lights
        for(int i = 0; i<numDirectionalLights; ++i)
        {
            vec4 lightColor = lightData.values[index++];
            float area = lightData.values[index].w;
            vec3 direction = -lightData.values[index++].xyz;
            vec4 shadowMapSettings = lightData.values[index++];

            float brightness = lightColor.a;
            totalBrigtness += brightness;
            float visibility = 0.0f;

            // check shadow maps if required
            bool matched = false;
            while ((shadowMapSettings.r > 0.0 && brightness > brightnessCutoff) && !matched)
            {
                mat4 sm_matrix = mat4(lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++],
                                      lightData.values[index++]);

                vec4 sm_tc = (sm_matrix) * vec4(worldPos, 1.0);

                if (sm_tc.x >= 0.0 && sm_tc.x <= 1.0 && sm_tc.y >= 0.0 && sm_tc.y <= 1.0 && sm_tc.z >= 0.0 /* && sm_tc.z <= 1.0*/)
                {
                    //visibility = 1 - texture(shadowMaps, vec4(sm_tc.st, shadowMapIndex, sm_tc.z)).r; //锟斤拷锟斤拷前片锟轿碉拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷影锟斤拷图锟叫碉拷锟斤拷锟街碉拷锟斤拷斜冉锟?锟斤拷锟斤拷影0 锟斤拷锟斤拷锟斤拷影1

                    matched = true;
                    // poissonDiskSamples(sm_tc.xy); 
                    float random = ValueNoise(sm_tc.xyz);
                    if(globalBuffer.shadow_type == 0){
                        visibility = PCF(sm_tc,shadowMapIndex,area, random);
                    }else if(globalBuffer.shadow_type == 1){
                        // visibility = 1 - PCSS(sm_tc,shadowMapIndex, area, random);
                        visibility = SampleShadow_PCSS_Area(sm_tc.xyz, vec2(gl_FragCoord.xy), globalBuffer.softness, globalBuffer.softness_falloff, globalBuffer.blocker_sample_num, globalBuffer.pcf_sample_num, globalBuffer.shadow_bias, shadowMapIndex);
                    }
                }else{
                    visibility = 1.0;
                }

                ++shadowMapIndex;
                shadowMapSettings.r -= 1.0;
            }

            if (shadowMapSettings.r > 0.0)
            {
                // skip lightData and shadowMap entries for shadow maps that we haven't visited for this light
                // so subsequent light pointions are correct.
                index += 4 * int(shadowMapSettings.r);
                shadowMapIndex += int(shadowMapSettings.r);
            }

            totalRealBrightness += brightness * visibility;
        }
        scene_brightness = totalRealBrightness / totalBrigtness;
    }
    vec4 last_ndc = pc.projection * globalBuffer.last_view * vec4(lastWorldPos, 1);
    ivec2 last_coord = ivec2(((last_ndc.x / last_ndc.w) / 2 + 0.5) * globalBuffer.width, ((last_ndc.y / last_ndc.w) / 2 + 0.5) * globalBuffer.height);
    float old_shadow = 1;
    float oldInstanceID = -1;
    if(last_coord.x >= 0 && last_coord.y >= 0 && last_coord.x < globalBuffer.width && last_coord.y < globalBuffer.height){
        vec2 shadowdataold_shadow = texelFetch(shadowInputAttachment, last_coord, 0).rg;
        oldInstanceID = shadowdataold_shadow.y;
        old_shadow = shadowdataold_shadow.x;
    }

    float current_shadow_value = scene_brightness; // 鏆傛椂淇濆瓨褰撳墠甯х殑闃村奖鍊?
    if (abs(oldInstanceID - InstanceID) < 0.1 && abs(old_shadow - scene_brightness) < 0.1)
    {
        float historyLuma = old_shadow;
        float currentLuma = current_shadow_value;

        float diff = abs(currentLuma - historyLuma) / max(max(currentLuma, historyLuma), 0.2); // 璁＄畻鐩稿宸紓

        float weight_sq = (1.0 - diff);
        weight_sq = weight_sq * weight_sq;
        
        const float feedbackMin = 0.96; // 鏈€灏忓弽棣?(褰撳墠甯у樊寮傚ぇ鏃?
        const float feedbackMax = 0.91; // 鏈€澶у弽棣?(褰撳墠甯у樊寮傚皬鏃?

        float feedback = (1.0 - weight_sq) * feedbackMin + weight_sq * feedbackMax;

        scene_brightness = mix(current_shadow_value, old_shadow, feedback);

        // 閽冲埗鏈€缁堢粨鏋?
        scene_brightness = clamp(scene_brightness, 0.0, 1.0);
    }

    outColor = vec4(texture(cameraImage, screen_uv).rgb * scene_brightness, 1.0);
    outNormal = vec4(0.0, 0.0, 0.0, 1.0);
    outWorldPos = vec4(0.0, 0.0, 0.0, 1.0);
    outShadow = vec4(scene_brightness, InstanceID, gl_FragCoord.z, 1);
    outMaterial = vec4(0.0);
    outMask = 2.0;
}
