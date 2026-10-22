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
    float density = 1.41f;   // g/cm^3. Sun 1.41, Earth 5.51, Mars 3.93, Mercury 5.43, Jupiter 1.33
    float mass() const { return density * radius * radius * radius; }
};

void orbitCalc(Body& bodyObject, float startingOffset, float orbitalRadius, float orbitalSpeed) {
    float offsetScaled = startingOffset * 2 * std::numbers::pi;
    bodyObject.position.x = sin((glfwGetTime()) * orbitalSpeed + offsetScaled) * orbitalRadius;
    bodyObject.position.z = cos((glfwGetTime()) * orbitalSpeed + offsetScaled) * orbitalRadius;
}

#endif

