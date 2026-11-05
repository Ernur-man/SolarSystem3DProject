#pragma once
#include <glad/glad.h>
class Mesh {
public:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei count = 0;
    GLuint lineVao = 0;
    GLuint lineVbo = 0;
    GLsizei lineCount = 0;

    void makeSphere(unsigned rings = 24, unsigned sectors = 32);
    void draw() const;
    void makeRing(float innerRadius, float outerRadius, unsigned segments = 192);
    void makeOrbit(float radius, unsigned segments = 128);
    void drawOrbit() const;
};
