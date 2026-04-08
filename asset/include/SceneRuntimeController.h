#pragma once

#include <memory>
#include <string>
#include <vector>

#include "RenderStateController.h"

class vsgRendererServer;

class SceneRuntimeController
{
public:
    SceneRuntimeController(vsgRendererServer& renderer,
                           std::shared_ptr<RenderStateController> persistence_controller,
                           RenderStateHub& render_state,
                           SceneLinePointStyle& line_point_style,
                           float& base_brightness);

    void initializeForScene(int scene_id);
    void applyLoadedState();

    bool setHdrImageNum(int hdr_num);
    bool setBaseBrightness(float value);
    bool setDepthOcclusionEnabled(bool enabled);
    bool setShadowMode(int mode);
    bool setShadowType(int type);
    bool setExposure(float value);
    bool setSsaoRadius(float value);
    bool setSsaoKernelSize(int value);
    bool setDenoiseSize(int value);
    bool setShadowBias(float value);
    bool setBlockerSampleNum(int value);
    bool setPcfSampleNum(int value);
    bool setPcfSoftness(float value);
    bool setPcssSoftness(float value);
    bool setPcssSoftnessFalloff(float value);
    bool setLineColor(const vsg::vec3& color);
    bool setPointColor(const vsg::vec3& color);
    bool setInstanceTransform(const std::string& instance_name, const vsg::dmat4& transform) const;

    bool saveRenderState(std::string* error_message = nullptr) const;
    bool saveBaseBrightness(std::string* error_message = nullptr) const;
    bool saveSceneTransforms(const std::vector<SceneModelTransformSave>& transforms, std::string* error_message = nullptr) const;

private:
    static bool nearlyEqual(float lhs, float rhs, float epsilon = 1e-6f);
    static bool sameVec3(const vsg::vec3& lhs, const vsg::vec3& rhs);

    vsgRendererServer& renderer_;
    std::shared_ptr<RenderStateController> persistence_controller_;
    RenderStateHub& render_state_;
    SceneLinePointStyle& line_point_style_;
    float& base_brightness_;
    int scene_id_ = -1;
    int applied_hdr_image_num_ = -1;
};
