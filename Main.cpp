#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include "shader_s.h"
#include "camera.h"
#include "mesh.h"

#include <iostream>
#include <vector>
#include <cmath>

// Test for GitHub (placeholder line changed)

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// lighting
glm::vec3 lightPos(1.2f, 1.0f, 2.0f);

// Builds a unit sphere (radius 1) centred on the origin.
// sectors = columns (longitude), stacks = rows (latitude).
Mesh createSphereMesh(int sectors, int stacks)
{
    const float PI = 3.14159265f;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // vertices: row/column -> stack/sector angle -> x, y, z
    for (int i = 0; i <= stacks; ++i)
    {
        float stackAngle = PI / 2 - i * (PI / stacks);   // +90 to -90 degrees
        float xz = cosf(stackAngle);
        float y = sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j)
        {
            float sectorAngle = j * (2 * PI / sectors);  // 0 to 360 degrees

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

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
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
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader zprogram
    // ------------------------------------
    Shader lightingShader("2.2.basic_lighting.vs", "2.2.basic_lighting.fs");
    Shader lightCubeShader("2.2.light_cube.vs", "2.2.light_cube.fs");

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    //float vertices[] = {
    //    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    //     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    //     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    //     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    //    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    //    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

    //    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    //     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    //     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    //     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    //    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    //    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

    //    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    //    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    //    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    //    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    //    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    //    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

    //     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
    //     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
    //     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
    //     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
    //     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
    //     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

    //    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
    //     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
    //     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    //     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    //    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    //    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

    //    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
    //     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
    //     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    //     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    //    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    //    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    //};
    //// first, configure the cube's VAO (and VBO)
    //unsigned int VBO, cubeVAO;
    //glGenVertexArrays(1, &cubeVAO);
    //glGenBuffers(1, &VBO);

    //glBindBuffer(GL_ARRAY_BUFFER, VBO);
    //glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    //glBindVertexArray(cubeVAO);

    //// position attribute
    //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    //glEnableVertexAttribArray(0);
    //// normal attribute
    //glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    //glEnableVertexAttribArray(1);


    //// second, configure the light's VAO (VBO stays the same; the vertices are the same for the light object which is also a 3D cube)
    //unsigned int lightCubeVAO;
    //glGenVertexArrays(1, &lightCubeVAO);
    //glBindVertexArray(lightCubeVAO);

    //glBindBuffer(GL_ARRAY_BUFFER, VBO);
    //// note that we update the lamp's position attribute's stride to reflect the updated buffer data
    //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    //glEnableVertexAttribArray(0);

    Mesh sphere = createSphereMesh(36, 18);   // columns, rows

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // change the light's position values over time (can be done anywhere in the render loop actually, but try to do it at least before using the light source positions)
        lightPos.x = 1.0f + sin((glfwGetTime())) * 2.0f;
        lightPos.y = sin((glfwGetTime()) / 2.0f) * 1.0f;

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

        //// world transformation
        //glm::mat4 model = glm::mat4(1.0f);
        //lightingShader.setMat4("model", model);

        //// render the cube
        //glBindVertexArray(cubeVAO);
        //glDrawArrays(GL_TRIANGLES, 0, 36);

        drawSphere(sphere, lightingShader, glm::vec3(0.0f), 0.5f, rgb(0, 119, 190));

        // also draw the lamp object
        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);
        //model = glm::mat4(1.0f);
        //model = glm::translate(model, lightPos);
        //model = glm::scale(model, glm::vec3(0.2f)); // a smaller cube
        //lightCubeShader.setMat4("model", model);

        //glBindVertexArray(lightCubeVAO);
        //glDrawArrays(GL_TRIANGLES, 0, 36);
        drawSphere(sphere, lightCubeShader, lightPos, 0.1f, rgb(255, 255, 255));


        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    // glDeleteVertexArrays(1, &cubeVAO);
    // glDeleteVertexArrays(1, &lightCubeVAO);
    // glDeleteBuffers(1, &VBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
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
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}


// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
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
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}