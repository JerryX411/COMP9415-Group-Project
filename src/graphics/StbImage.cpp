// Compile stb_image and stb_image_write exactly once for the whole project.
// Other source files should include the headers without these macros.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
