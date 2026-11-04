#pragma once
#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    GLuint id = 0;

    bool load(const std::string &vertexPath, const std::string &fragmentPath);
    void use() const {
        glUseProgram(id);
    }
    void set(const char *name, const glm::mat4 &value) const;
    void set(const char *name, const glm::vec3 &value) const;
    void set(const char *name, float value) const;
    void set(const char *name, int value) const;
};
