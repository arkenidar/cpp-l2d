// Single translation unit that expands the stb header-only implementations.
// Included once in the l2d static library; other files include the headers
// in "header-only" mode (just the declarations).
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"