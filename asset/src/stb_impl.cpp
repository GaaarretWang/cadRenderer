// STB image library implementations
// stb_image: 独立定义（vsgXchange中用STB_IMAGE_STATIC，其符号不导出）
// stb_image_write: vsgXchange中未用STATIC，符号已导出，此处只include头文件即可

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
