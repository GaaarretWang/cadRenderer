// STB Image implementation for cadRenderer
// vsgXchange's stbi.cpp uses STB_IMAGE_STATIC for read functions,
// but write functions are still exported. We only need to provide
// non-static read functions here.

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
