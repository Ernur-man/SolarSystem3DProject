#include "Mesh.h"

#include <cmath>
#include <vector>

void Mesh::makeSphere(unsigned rings, unsigned sectors) {
    std::vector<float> vertices;
    std::vector<unsigned> indices;

    for (unsigned ring = 0; ring <= rings; ++ring) {
        const float phi = 3.14159265f * static_cast<float>(ring) / static_cast<float>(rings);
        for (unsigned sector = 0; sector <= sectors; ++sector) {
            const float theta =
                6.2831853f * static_cast<float>(sector) / static_cast<float>(sectors);
            const float x = std::sin(phi) * std::cos(theta);
            const float y = std::cos(phi);
            const float z = std::sin(phi) * std::sin(theta);
            vertices.insert(vertices.end(),
                            {x, y, z, x, y, z,
                             static_cast<float>(sector) / static_cast<float>(sectors),
                             static_cast<float>(ring) / static_cast<float>(rings)});
        }
    }

    for (unsigned ring = 0; ring < rings; ++ring) {
        for (unsigned sector = 0; sector < sectors; ++sector) {
            const unsigned first = ring * (sectors + 1) + sector;
            const unsigned second = first + sectors + 1;
            indices.insert(indices.end(),
                           {first, second, first + 1, first + 1, second, second + 1});
        }
    }

    count = static_cast<GLsizei>(indices.size());
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)), indices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Mesh::draw() const {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
}

void Mesh::makeOrbit(float radius, unsigned segments) {
    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(segments) * 3);
    for (unsigned i = 0; i < segments; ++i) {
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(segments);
        vertices.insert(vertices.end(), {radius * std::cos(angle), 0.0f, radius * std::sin(angle)});
    }
    lineCount = static_cast<GLsizei>(segments);
    glGenVertexArrays(1, &lineVao);
    glGenBuffers(1, &lineVbo);
    glBindVertexArray(lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Mesh::drawOrbit() const {
    glBindVertexArray(lineVao);
    glDrawArrays(GL_LINE_LOOP, 0, lineCount);
}
