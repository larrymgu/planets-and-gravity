#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include "shader.h"
#include "camera.h"
#include "mesh.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <numbers>

// Test for GitHub (placeholder line changed)

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 2.0f, 14.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// lighting
glm::vec3 lightPos(0.0f, 0.0f, 0.0f);

// Builds a unit sphere (radius 1) centred on the origin.
// sectors = columns (longitude), stacks = rows (latitude).
Mesh createSphereMesh(int sectors, int stacks)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // vertices: row/column -> stack/sector angle -> x, y, z
    for (int i = 0; i <= stacks; ++i)
    {
        float stackAngle = std::numbers::pi / 2 - i * (std::numbers::pi / stacks);   // +90 to -90 degrees
        float xz = cosf(stackAngle);
        float y = sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j)
        {
            float sectorAngle = j * (2 * std::numbers::pi / sectors);  // 0 to 360 degrees

            Vertex v{};                                  // zeroes tangent/bone fields
            v.Position = glm::vec3(xz * cosf(sectorAngle), y, -xz * sinf(sectorAngle));
            v.Normal = v.Position;                    // unit sphere: normal == position
            v.TexCoords = glm::vec2((float)j / sectors, (float)i / stacks);
            vertices.push_back(v);
        }
    }

    // indices: two triangles per quad, one at the poles
    for (int i = 0; i < stacks; ++i)
    {
        unsigned int k1 = i * (sectors + 1);   // start of this row
        unsigned int k2 = k1 + sectors + 1;    // start of next row

        for (int j = 0; j < sectors; ++j, ++k1, ++k2)
        {
            if (i != 0)
                indices.insert(indices.end(), { k1, k2, k1 + 1 });
            if (i != stacks - 1)
                indices.insert(indices.end(), { k1 + 1, k2, k2 + 1 });
        }
    }

    return Mesh(vertices, indices, {});        // no textures yet
}

// Converts 0-255 RGB to the 0-1 range OpenGL uses
glm::vec3 rgb(int r, int g, int b)
{
    return glm::vec3(r, g, b) / 255.0f;
}

void drawSphere(Mesh& sphere, Shader& shader, glm::vec3 position, float radius, glm::vec3 color)
{
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::scale(model, glm::vec3(radius));
    shader.setMat4("model", model);
    shader.setVec3("objectColor", color);
    sphere.Draw(shader);
}

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

int main()
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    glEnable(GL_DEPTH_TEST);

    Shader gridShader("Grid.vert", "Grid.frag");
    Grid grid = createGrid(25.0f, 120, -1.0f);        // 20 units wide, 120 squares per side, 1.5 below the orbit plane
    Body sun = { lightPos, 1.0f, rgb(255, 255, 255) };

    glEnable(GL_BLEND);                                // lets the grid's alpha make it see-through
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // build and compile our shader zprogram
    Shader lightingShader("ObjectLighting.vert", "ObjectLighting.frag");
    Shader lightSphereShader("LightSphere.vert", "LightSphere.frag");

    Mesh sphere = createSphereMesh(36, 18);   // columns, rows

    std::vector<Body> bodies = {
        { glm::vec3(0.0f, 0.0f, 1.0f), 0.3f, rgb(0, 119, 190) },
        { glm::vec3(0.9f, 0.0f, 0.2f), 0.2f, rgb(140, 55, 62) },
        { glm::vec3(-0.7f, 0.0f, -1.3f), 0.8f, rgb(255, 150, 79 ) },
    };


    // render loop
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        processInput(window);

        // render
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // change the light's position values over time (can be done anywhere in the render loop actually, but try to do it at least before using the light source positions)
        //lightPos.x = 1.0f + sin((glfwGetTime())) * 2.0f;
        //lightPos.y = sin((glfwGetTime()) / 2.0f) * 1.0f;
        orbitCalc(bodies[0], 0.25, 3, 2);
        orbitCalc(bodies[1], 0.6, 2, 1);
        orbitCalc(bodies[2], -0.25, 5, 3);

        std::vector<Body> gravitySources = bodies;
        gravitySources.push_back(sun);
        updateGrid(grid, gravitySources, 2.5f);

        // be sure to activate shader when setting uniforms/drawing objects
        lightingShader.use();
        lightingShader.setVec3("objectColor", 1.0f, 0.5f, 0.31f);
        lightingShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
        lightingShader.setVec3("lightPos", lightPos);
        lightingShader.setVec3("viewPos", camera.Position);

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);

        for (const Body& b : bodies)
            drawSphere(sphere, lightingShader, b.position, b.radius, b.color);

        // also draw the lamp object
        lightSphereShader.use();
        lightSphereShader.setMat4("projection", projection);
        lightSphereShader.setMat4("view", view);

        drawSphere(sphere, lightSphereShader, lightPos, 1.0f, rgb(255, 255, 255));

        gridShader.use();
        gridShader.setMat4("projection", projection);
        gridShader.setMat4("view", view);
        drawGrid(grid, gridShader, glm::vec4(1.0f, 1.0f, 1.0f, 0.25f));   // white at 25% opacity

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}


// glfw: whenever the mouse moves, this callback is called
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}