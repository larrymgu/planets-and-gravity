#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include "shader.h"
#include "camera.h"
#include "mesh.h"
#include "body.h"
#include "grid.h"

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

const unsigned int BLOOM_DOWNSCALE = 2;   // 2 = half resolution, 4 = quarter
const unsigned int BLOOM_WIDTH = SCR_WIDTH / BLOOM_DOWNSCALE;
const unsigned int BLOOM_HEIGHT = SCR_HEIGHT / BLOOM_DOWNSCALE;

unsigned int samples = 8;

// camera
Camera camera(glm::vec3(0.0f, -4.0f, 27.5f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// lighting
glm::vec3 lightPos(0.0f, 0.0f, 0.0f);

// For Framebuffer
float rectangleVertices[] =
{
    // Coords    // texCoords
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,

     1.0f,  1.0f,  1.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f
};



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

int main()
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // glfwWindowHint(GLFW_SAMPLES, samples);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);



#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Planets and Gravity", NULL, NULL);
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
    glEnable(GL_MULTISAMPLE);

    Shader gridShader("Shader Files/Grid.vert", "Shader Files/Grid.frag");
    Grid grid = createGrid(50.0f, 300, -12.0f);       
    Body sun = { lightPos, 2.5f, rgb(255, 255, 255) };

    glEnable(GL_BLEND);                                // lets the grid's alpha make it see-through
    glDisablei(GL_BLEND, 1);   // the glow image (colour buffer 1) stores values as written
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // build and compile our shader programs
    Shader lightingShader("Shader Files/ObjectLighting.vert", "Shader Files/ObjectLighting.frag");
    Shader lightSphereShader("Shader Files/LightSphere.vert", "Shader Files/LightSphere.frag");
    Shader framebufferProgram("Shader Files/FrameBuffer.vert", "Shader Files/FrameBuffer.frag");


    Mesh sphere = createSphereMesh(36, 18);   // columns, rows

    std::vector<Body> bodies = {
        { glm::vec3(0.0f, 0.0f, 1.0f), 0.2f, rgb(0, 119, 190), 5.51f },     // Earth-like
        { glm::vec3(0.9f, 0.0f, 0.2f), 0.1f, rgb(140, 55, 62), 3.93f },     // Mars-like
        { glm::vec3(-0.7f, 0.0f, -1.3f), 0.5f, rgb(255, 150, 79), 5.00f },  // Jupiter-like
    };

    // Prepare framebuffer rectangle VBO and VAO
    unsigned int rectVAO, rectVBO;
    glGenVertexArrays(1, &rectVAO);
    glGenBuffers(1, &rectVBO);
    glBindVertexArray(rectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleVertices), &rectangleVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


    // Create Frame Buffer Object
    unsigned int FBO;
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    // Create Framebuffer Texture
    unsigned int framebufferTexture;
    glGenTextures(1, &framebufferTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebufferTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGB, SCR_WIDTH, SCR_HEIGHT, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Prevents edge bleeding
    glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Prevents edge bleeding
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, framebufferTexture, 0);

    // Second colour buffer: only the glowing parts of the scene
    unsigned int brightTextureMS;
    glGenTextures(1, &brightTextureMS);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, brightTextureMS);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE, brightTextureMS, 0);

    // Tell OpenGL the fragment shaders write to both colour buffers
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    // Create Render Buffer Object
    unsigned int RBO;
    glGenRenderbuffers(1, &RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);


    // Error checking framebuffer
    auto fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer error: " << fboStatus << std::endl;

    // Create Frame Buffer Object
    unsigned int postProcessingFBO;
    glGenFramebuffers(1, &postProcessingFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, postProcessingFBO);

    // Create Framebuffer Texture
    unsigned int postProcessingTexture;
    glGenTextures(1, &postProcessingTexture);
    glBindTexture(GL_TEXTURE_2D, postProcessingTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, postProcessingTexture, 0);

    unsigned int brightTexture;
    glGenTextures(1, &brightTexture);
    glBindTexture(GL_TEXTURE_2D, brightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, brightTexture, 0);

    // Error checking framebuffer
    fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Post-Processing Framebuffer error: " << fboStatus << std::endl;

    // Two framebuffers the glow image bounces between while it's blurred
    unsigned int pingpongFBO[2], pingpongTexture[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongTexture);
    for (int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, BLOOM_WIDTH, BLOOM_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongTexture[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // A framebuffer to keep a copy of the glow before it's fully blurred
    unsigned int narrowFBO, narrowTexture;
    glGenFramebuffers(1, &narrowFBO);
    glGenTextures(1, &narrowTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, narrowFBO);
    glBindTexture(GL_TEXTURE_2D, narrowTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, BLOOM_WIDTH, BLOOM_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, narrowTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    Shader blurShader("Shader Files/FrameBuffer.vert", "Shader Files/Blur.frag");

    // Which texture unit each sampler reads from (only needs setting once)
    blurShader.use();
    blurShader.setInt("image", 0);
    framebufferProgram.use();
    framebufferProgram.setInt("screenTexture", 0);
    framebufferProgram.setInt("bloomBlur", 1);
    framebufferProgram.setInt("bloomNarrow", 2);
    framebufferProgram.setInt("planetMask", 3);

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
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glClearBufferfv(GL_COLOR, 1, black);   // colour buffer 1 = the glow image
        glEnable(GL_DEPTH_TEST);

        // change the light's position values over time (can be done anywhere in the render loop actually, but try to do it at least before using the light source positions)
        //lightPos.x = 1.0f + sin((glfwGetTime())) * 2.0f;
        //lightPos.y = sin((glfwGetTime()) / 2.0f) * 1.0f;
        orbitCalc(bodies[0], 0.25, 5 , 0.9);
        orbitCalc(bodies[1], 0.6, 8, 0.45);
        orbitCalc(bodies[2], -0.25, 11, 0.27);

        std::vector<Body> gravitySources = bodies;
        gravitySources.push_back(sun);
        updateGrid(grid, gravitySources, 2.0f);

        // be sure to activate shader when setting uniforms/drawing objects
        lightingShader.use();
        lightingShader.setVec3("objectColor", 1.0f, 0.5f, 0.31f);
        lightingShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
        lightingShader.setVec3("lightPos", lightPos);
        lightingShader.setVec3("viewPos", camera.Position);
        lightingShader.setFloat("sunRadius", 2.5f);   // match the sun's size on line 366

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);

        for (const Body& b : bodies)
            drawSphere(sphere, lightingShader, b.position, b.radius, b.color);

        // also draw the lamp object
        lightSphereShader.use();
        lightSphereShader.setFloat("glowStrength", 1.2f);
        lightSphereShader.setMat4("projection", projection);
        lightSphereShader.setMat4("view", view);

        drawSphere(sphere, lightSphereShader, lightPos, 2.5f, rgb(255, 255, 255));

        gridShader.use();
        gridShader.setMat4("projection", projection);
        gridShader.setMat4("view", view);
        drawGrid(grid, gridShader, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.25f));

        glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postProcessingFBO);
        for (int i = 0; i < 2; i++)
        {
            glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
            glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
            glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        // Screen-rectangle passes: no depth test or blending
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glBindVertexArray(rectVAO);
        glActiveTexture(GL_TEXTURE0);

        // Blur the glow image, alternating horizontal and vertical passes
        glViewport(0, 0, BLOOM_WIDTH, BLOOM_HEIGHT);
        bool horizontal = true;
        int blurPasses = 1000;
        int narrowPasses = 10;   // when to save the tight glow; keep it even and less than blurPasses
        blurShader.use();
        for (int i = 0; i < blurPasses; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setBool("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, i == 0 ? brightTexture : pingpongTexture[!horizontal]);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            horizontal = !horizontal;
            // Partway through, save a copy of the still-tight glow
            if (i == narrowPasses - 1)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, pingpongFBO[!horizontal]);   // the texture just written
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, narrowFBO);
                glBlitFramebuffer(0, 0, BLOOM_WIDTH, BLOOM_HEIGHT, 0, 0, BLOOM_WIDTH, BLOOM_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
        }

        // Draw scene + blurred glow to the window

        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        framebufferProgram.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, postProcessingTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongTexture[!horizontal]);   // the last blurred result
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, narrowTexture);   // the saved early, tight blur
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, brightTexture);   // unblurred glow image, used as the planet mask
        glActiveTexture(GL_TEXTURE0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glEnable(GL_BLEND);   // the grid needs blending again next frame
        glDisablei(GL_BLEND, 1);   // the glow image (colour buffer 1) stores values as written
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
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
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