#ifndef GRID_H
#define GRID_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <cmath>
#include <vector>

#include "shader.h"
#include "body.h"


struct Grid {
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    float baseHeight = 0.0f;          // y of the sheet before any dips
    std::vector<glm::vec3> points;
    unsigned int indexCount = 0;
};

// Builds a flat size x size sheet of lines in the XZ plane, centred on the origin
Grid createGrid(float size, int divisions, float baseHeight)
{
    Grid grid;
    grid.baseHeight = baseHeight;

    int perRow = divisions + 1;
    float step = size / divisions;
    float half = size / 2.0f;

    // Vertices: one point at every grid intersection
    for (int row = 0; row < perRow; ++row)
        for (int col = 0; col < perRow; ++col)
            grid.points.push_back(glm::vec3(-half + col * step, baseHeight, -half + row * step));

    // Indices: join each point to its right neighbour and the point below it
    std::vector<unsigned int> indices;
    for (int row = 0; row < perRow; ++row)
        for (int col = 0; col < perRow; ++col)
        {
            unsigned int i = row * perRow + col;
            if (col < divisions) indices.insert(indices.end(), { i, i + 1 });        // line along x
            if (row < divisions) indices.insert(indices.end(), { i, i + perRow });   // line along z
        }
    grid.indexCount = indices.size();

    // Upload to the GPU (same pattern as Mesh::setupMesh)
    glGenVertexArrays(1, &grid.VAO);
    glGenBuffers(1, &grid.VBO);
    glGenBuffers(1, &grid.EBO);
    glBindVertexArray(grid.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, grid.VBO);
    glBufferData(GL_ARRAY_BUFFER, grid.points.size() * sizeof(glm::vec3), grid.points.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grid.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return grid;
}

// Lowers every grid point by the combined gravity well of all the bodies.
void updateGrid(Grid& grid, const std::vector<Body>& bodies, float strength)
{
    for (glm::vec3& p : grid.points)
    {
        float depth = 0.0f;
        for (const Body& b : bodies)
        {
            float dx = p.x - b.position.x;
            float dz = p.z - b.position.z;
            float distance = sqrtf(dx * dx + dz * dz + b.radius * b.radius);   // softened
            depth += strength * b.mass() / distance;
        }
        p.y = grid.baseHeight - depth;
    }

    // Overwrite the existing buffer instead of reallocating it
    glBindBuffer(GL_ARRAY_BUFFER, grid.VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, grid.points.size() * sizeof(glm::vec3), grid.points.data());
}

void drawGrid(const Grid& grid, Shader& shader, glm::vec4 color)
{
    shader.setMat4("model", glm::mat4(1.0f));   // Points are already in world space
    shader.setVec4("gridColor", color);
    glBindVertexArray(grid.VAO);
    glDrawElements(GL_LINES, grid.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

#endif
