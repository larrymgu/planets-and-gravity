#ifndef BODY_H
#define BODY_H

#include <glad/glad.h>   
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cmath>
#include <numbers>

struct Body {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    float mass() const { return std::pow(radius, 3); }
};

void orbitCalc(Body& bodyObject, float startingOffset, float orbitalRadius, float orbitalSpeed) {
    float offsetScaled = startingOffset * 2 * std::numbers::pi;
    bodyObject.position.x = sin((glfwGetTime()) * orbitalSpeed + offsetScaled) * orbitalRadius;
    bodyObject.position.z = cos((glfwGetTime()) * orbitalSpeed + offsetScaled) * orbitalRadius;
}

#endif

