#include "ImGui.h"
namespace gui {
    vsg::ref_ptr<Params> global_params = Params::create();  // ✅ 在 cpp 中初始化
}
