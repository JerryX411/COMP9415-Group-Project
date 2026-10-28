#include "graphics/Shader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <glm/gtc/type_ptr.hpp>

#include "graphics/OpenGL.h"

namespace craft::graphics {

namespace {

std::string readTextFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Failed to open shader: " + path.string());
  }

  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

GLuint compileShader(const std::filesystem::path& path, GLenum type) {
  const std::string source = readTextFile(path);
  const char* sourceData = source.c_str();

  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &sourceData, nullptr);
  glCompileShader(shader);

  // Compilation errors are reported with the driver's GLSL error log.
  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_TRUE) {
    return shader;
  }

  GLint logLength = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
  std::string log(static_cast<std::size_t>(logLength > 0 ? logLength : 1),
                  '\0');
  glGetShaderInfoLog(shader, logLength, nullptr, log.data());
  glDeleteShader(shader);

  throw std::runtime_error("Shader compilation failed (" + path.string() +
                           "):\n" + log);
}

}  // namespace

Shader::Shader(const std::filesystem::path& vertexPath,
               const std::filesystem::path& fragmentPath) {
  GLuint vertexShader = 0;
  GLuint fragmentShader = 0;

  try {
    vertexShader = compileShader(vertexPath, GL_VERTEX_SHADER);
    fragmentShader = compileShader(fragmentPath, GL_FRAGMENT_SHADER);

    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader);
    glAttachShader(program_, fragmentShader);
    glLinkProgram(program_);

    // Linking checks that shader stages agree on their inputs and outputs.
    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
      GLint logLength = 0;
      glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &logLength);
      std::string log(static_cast<std::size_t>(logLength > 0 ? logLength : 1),
                      '\0');
      glGetProgramInfoLog(program_, logLength, nullptr, log.data());
      throw std::runtime_error("Shader program link failed:\n" + log);
    }
  } catch (...) {
    // Clean up partly created GPU objects before preserving the error.
    if (program_ != 0) {
      glDeleteProgram(program_);
      program_ = 0;
    }
    if (vertexShader != 0) {
      glDeleteShader(vertexShader);
    }
    if (fragmentShader != 0) {
      glDeleteShader(fragmentShader);
    }
    throw;
  }

  // A linked program keeps the compiled code, so the shader objects can go.
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
}

Shader::~Shader() {
  if (program_ != 0) {
    glDeleteProgram(program_);
  }
}

void Shader::use() const {
  glUseProgram(program_);
}

void Shader::setMat4(std::string_view name, const glm::mat4& value) const {
  // GLM stores matrices in the column-major order expected by OpenGL.
  glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec2(std::string_view name, const glm::vec2& value) const {
  glUniform2fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec3(std::string_view name, const glm::vec3& value) const {
  glUniform3fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec4(std::string_view name, const glm::vec4& value) const {
  glUniform4fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setFloat(std::string_view name, float value) const {
  glUniform1f(uniformLocation(name), value);
}

void Shader::setInt(std::string_view name, int value) const {
  glUniform1i(uniformLocation(name), value);
}

int Shader::uniformLocation(std::string_view name) const {
  // OpenGL expects a null-terminated C string, while string_view may not have
  // one.
  const std::string nullTerminatedName(name);
  const GLint location =
      glGetUniformLocation(program_, nullTerminatedName.c_str());
  if (location < 0) {
    throw std::runtime_error("Shader uniform not found: " + nullTerminatedName);
  }
  return location;
}

}  // namespace craft::graphics
