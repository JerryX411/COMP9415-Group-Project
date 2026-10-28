#include "graphics/Screenshot.h"
#include "graphics/OpenGL.h"
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <stb_image_write.h>
namespace relay {
void saveScreenshot(const std::filesystem::path& path, int width, int height) {
  if(path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
  std::vector<unsigned char> pixels(std::size_t(width)*height*3), flipped(pixels.size());
  glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
  const auto stride=std::size_t(width)*3;
  for(int y=0;y<height;++y) std::copy_n(pixels.data()+std::size_t(height-y-1)*stride,stride,flipped.data()+std::size_t(y)*stride);
  if(!stbi_write_png(path.string().c_str(),width,height,3,flipped.data(),int(stride))) throw std::runtime_error("Cannot write screenshot: "+path.string());
}
}
