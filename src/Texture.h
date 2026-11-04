#pragma once

#include <glad/glad.h>
#include <string>

class Texture {
public:
    ~Texture();
    bool load(const std::string& path);
    bool isLoaded() const { return textureId != 0; }
    void bind() const;
    void release();

private:
    GLuint textureId = 0;
};
