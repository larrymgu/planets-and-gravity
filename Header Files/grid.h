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
    unsigned int triangleIndexCount = 0;
    unsigned int lineIndexCount = 0;
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

    // triangles first: two per square, same pattern as the sphere
    for (int row = 0; row < divisions; ++row)
        for (int col = 0; col < divisions; ++col)
        {
            unsigned int k1 = row * perRow + col;   // this corner
            unsigned int k2 = k1 + perRow;          // the corner below it on the next row
            indices.insert(indices.end(), { k1, k2, k1 + 1,   k1 + 1, k2, k2 + 1 });
        }
    grid.triangleIndexCount = indices.size();

    // then the lines, exactly as before
    for (int row = 0; row < perRow; ++row)
        for (int col = 0; col < perRow; ++col)
        {
            unsigned int i = row * perRow + col;
            if (col < divisions) indices.insert(indices.end(), { i, i + 1 });
            if (row < divisions) indices.insert(indices.end(), { i, i + perRow });
        }
    grid.lineIndexCount = indices.size() - grid.triangleIndexCount;

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
            float distance = sqrtf(dx * dx + dz * dz + b.radius * b.radius * 4);   // softened
            depth += strength * b.mass() / distance;
        }
        p.y = grid.baseHeight + depth;
    }

    // Overwrite the existing buffer instead of reallocating it
    glBindBuffer(GL_ARRAY_BUFFER, grid.VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, grid.points.size() * sizeof(glm::vec3), grid.points.data());
}

void drawGrid(const Grid& grid, Shader& shader, glm::vec4 surfaceColor, glm::vec4 lineColor)
{
    shader.setMat4("model", glm::mat4(1.0f));
    glBindVertexArray(grid.VAO);

    // 1. solid surface, pushed slightly away from the camera so the lines on it still pass the depth test
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    shader.setVec4("gridColor", surfaceColor);
    glDrawElements(GL_TRIANGLES, grid.triangleIndexCount, GL_UNSIGNED_INT, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // 2. lines on top; their indices start right after the triangles' in the same buffer
    shader.setVec4("gridColor", lineColor);
    glDrawElements(GL_LINES, grid.lineIndexCount, GL_UNSIGNED_INT,
        (void*)(grid.triangleIndexCount * sizeof(unsigned int)));

    glBindVertexArray(0);
}

#endif
