#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace {
std::string readFile(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Shader file missing: " << path << '\n';
        return {};
    }

    std::stringstream contents;
    contents << file.rdbuf();
    return contents.str();
}
} // namespace

bool Shader::load(const std::string &vertexPath, const std::string &fragmentPath) {
    const std::string vertexSource = readFile(vertexPath);
    const std::string fragmentSource = readFile(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty()) {
        return false;
    }

    const auto compile = [](GLenum type, const std::string &source, const std::string &file) {
        const GLuint shader = glCreateShader(type);
        const char *sourcePointer = source.c_str();
        glShaderSource(shader, 1, &sourcePointer, nullptr);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint logLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            std::string log(static_cast<size_t>(logLength), '\0');
            glGetShaderInfoLog(shader, logLength, nullptr, log.data());
            std::cerr << "Shader compile error [" << file << "]:\n" << log << '\n';
            glDeleteShader(shader);
            return GLuint(0);
        }
        return shader;
    };

    const GLuint vertex = compile(GL_VERTEX_SHADER, vertexSource, vertexPath);
    const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
    if (!vertex || !fragment) {
        return false;
    }

    id = glCreateProgram();
    glAttachShader(id, vertex);
    glAttachShader(id, fragment);
    glLinkProgram(id);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLength = 0;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<size_t>(logLength), '\0');
        glGetProgramInfoLog(id, logLength, nullptr, log.data());
        std::cerr << "Shader link error [" << vertexPath << ", " << fragmentPath << "]:\n"
                  << log << '\n';
        glDeleteProgram(id);
        id = 0;
        return false;
    }
    return true;
}

void Shader::set(const char *name, const glm::mat4 &value) const {
    glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, &value[0][0]);
}

void Shader::set(const char *name, const glm::vec3 &value) const {
    glUniform3fv(glGetUniformLocation(id, name), 1, &value[0]);
}

void Shader::set(const char *name, float value) const {
    glUniform1f(glGetUniformLocation(id, name), value);
}

void Shader::set(const char *name, int value) const {
    glUniform1i(glGetUniformLocation(id, name), value);
}
