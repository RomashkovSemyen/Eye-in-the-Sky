/*
 * Earth Satellite Viewer
 * 
 * Управление:
 *   - ЛКМ + мышь: вращение камеры
 *   - WASD: вращение камеры (без зажатия ЛКМ)
 *   - Колесо мыши: приближение/отдаление
 *   - Стрелки вверх/вниз: чувствительность мыши
 *   - Стрелки влево/вправо: изменение масштаба времени
 *   - Shift + стрелки влево/вправо: быстрое изменение масштаба времени
 *   - X, Y, Z: вид по осям
 *   - 1-5: показать/скрыть орбиту соответствующего спутника
 * 
 * Формат elems.txt (пример):
 * a = 7000
 * e = 0.01
 * i = 51.6
 * W = 0.0
 * w = 0.0
 * nu = 45.0
 * Комментарий о статусе
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstdlib>

// ----------------------- Настройки окна -----------------------
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// ----------------------- Камера -----------------------
bool firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
float yaw = -90.0f;
float pitch = 0.0f;
float radius = 5.0f;
float camX, camY, camZ;
float mouseSensitivity = 0.01f;
bool mousePressed = false;

// ----------------------- Физические константы -----------------------
const double MU_KM3 = 398600.4418;
const double R_EARTH_KM = 6400.0;
const double SCALE = 1.0 / R_EARTH_KM;
const double SAT_RADIUS_KM = 500.0;
const double SAT_RADIUS_VIS = SAT_RADIUS_KM * SCALE;
const double EARTH_ANGULAR_SPEED = 2.0 * M_PI / 86400.0;

float timeScale = 1.0f;

// ----------------------- Шейдеры -----------------------
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D texture1;
void main()
{
    FragColor = texture(texture1, TexCoord);
}
)";

const char* colorVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* colorFragmentShader = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main()
{
    FragColor = vec4(color, 1.0);
}
)";

const char* lineVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* lineFragmentShader = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main()
{
    FragColor = vec4(color, 1.0);
}
)";

const char* textVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 projection;
uniform mat4 model;
void main()
{
    gl_Position = projection * model * vec4(aPos, 1.0);
}
)";

const char* textFragmentShader = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 textColor;
void main()
{
    FragColor = vec4(textColor, 1.0);
}
)";

// ----------------------- Загрузка текстуры -----------------------
unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

// ----------------------- Генерация сферы -----------------------
void generateSphere(std::vector<float>& vertices, std::vector<unsigned int>& indices,
                    float radius, int sectors, int stacks) {
    float x, y, z, s, t;
    float sectorStep = 2 * M_PI / sectors;
    float stackStep = M_PI / stacks;
    
    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = M_PI / 2 - i * stackStep;
        z = -radius * sinf(stackAngle);
        float xy = radius * cosf(stackAngle);
        t = 1.0f - (float)i / stacks;
        
        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * sectorStep;
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);
            s = (float)j / sectors + 0.5f;
            
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(s);
            vertices.push_back(t);
        }
    }
    
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = (i + 1) * (sectors + 1);
        for (int j = 0; j < sectors; ++j) {
            if (i != 0) {
                indices.push_back(k1 + j);
                indices.push_back(k2 + j);
                indices.push_back(k1 + j + 1);
            }
            if (i != (stacks - 1)) {
                indices.push_back(k1 + j + 1);
                indices.push_back(k2 + j);
                indices.push_back(k2 + j + 1);
            }
        }
    }
}

void generateSimpleSphere(std::vector<float>& vertices, std::vector<unsigned int>& indices,
                          float radius, int sectors, int stacks) {
    float x, y, z;
    float sectorStep = 2 * M_PI / sectors;
    float stackStep = M_PI / stacks;
    
    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = M_PI / 2 - i * stackStep;
        z = -radius * sinf(stackAngle);
        float xy = radius * cosf(stackAngle);
        
        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * sectorStep;
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = (i + 1) * (sectors + 1);
        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}

// ----------------------- Генерация орбиты -----------------------
void generateOrbit(std::vector<float>& orbitVertices, double a_km, double e, int segments) {
    if (e < 0.0) e = 0.0;
    if (e >= 1.0) e = 0.99;
    double a_vis = a_km * SCALE;
    double b = a_vis * sqrt(1.0 - e*e);
    double c = a_vis * e;
    for (int i = 0; i <= segments; ++i) {
        double t = 2.0 * M_PI * i / segments;
        double x = a_vis * cos(t) - c;
        double y = b * sin(t);
        orbitVertices.push_back((float)x);
        orbitVertices.push_back((float)y);
        orbitVertices.push_back(0.0f);
    }
}

// ----------------------- Оси -----------------------
void generateAxesLines(std::vector<float>& vertices) {
    vertices.insert(vertices.end(), {0,0,0, 2.2f,0,0});
    vertices.insert(vertices.end(), {0,0,0, 0,2.2f,0});
    vertices.insert(vertices.end(), {0,0,0, 0,0,2.2f});
}

void generateArrows(std::vector<float>& vertices) {
    auto addArrow = [&](float x, float y, float z, float dx, float dy, float dz) {
        vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
        vertices.push_back(x+dx); vertices.push_back(y+dy); vertices.push_back(z+dz);
        float wing = 0.12f;
        if (dx != 0) {
            vertices.insert(vertices.end(), {x+dx,y,z, x+dx*0.7f, y+wing, z+wing});
            vertices.insert(vertices.end(), {x+dx,y,z, x+dx*0.7f, y-wing, z-wing});
        } else if (dy != 0) {
            vertices.insert(vertices.end(), {x,y+dy,z, x+wing, y+dy*0.7f, z+wing});
            vertices.insert(vertices.end(), {x,y+dy,z, x-wing, y+dy*0.7f, z-wing});
        } else if (dz != 0) {
            vertices.insert(vertices.end(), {x,y,z+dz, x+wing, y+wing, z+dz*0.7f});
            vertices.insert(vertices.end(), {x,y,z+dz, x-wing, y-wing, z+dz*0.7f});
        }
    };
    addArrow(2.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f);
    addArrow(0.0f, 2.0f, 0.0f, 0.0f, 0.4f, 0.0f);
    addArrow(0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.4f);
}

// ----------------------- Структура спутника -----------------------
struct Satellite {
    int id;
    double a_km = 0.0;
    double e = 0.0;
    double i_rad = 0.0;
    double W_rad = 0.0;
    double w_rad = 0.0;
    double nu0_rad = 0.0;   // истинная аномалия на момент t=0
    double meanMotion = 0.0;
    double T0 = 0.0;
    
    std::string statusMsg;
    
    std::vector<float> orbitVertices;
    unsigned int orbitVAO = 0, orbitVBO = 0;
    int orbitVertexCount = 0;
    
    unsigned int satVAO = 0, satVBO = 0, satEBO = 0;
    int satIndexCount = 0;
    
    bool orbitVisible = true;
    bool destroyed = false;
    glm::vec3 color;
    
    glm::dvec3 position_km;
    glm::dvec3 velocity_km;
    
    double orbitPeriod = 0.0;

    // Инициализация без OpenGL (для консольного режима)
    void initOrbitParams() {
        if (a_km <= 0.0 || e >= 1.0) {
            destroyed = true;
            return;
        }
        meanMotion = sqrt(MU_KM3 / fabs(a_km*a_km*a_km));
        orbitPeriod = 2.0 * M_PI / meanMotion;
        
        // Перевод истинной аномалии nu0 в среднюю аномалию M0
        double tan_nu2 = tan(nu0_rad / 2.0);
        double tan_E2 = sqrt((1.0 - e) / (1.0 + e)) * tan_nu2;
        double E0 = 2.0 * atan(tan_E2);
        double M0 = E0 - e * sin(E0);
        
        // Вычисляем T0 так, чтобы при t = 0 средняя аномалия равнялась M0
        T0 = -M0 / meanMotion;
    }

    // Полная инициализация с OpenGL (для графического режима)
    void initGL() {
        if (destroyed) return;
        generateOrbit(orbitVertices, a_km, e, 200);
        orbitVertexCount = orbitVertices.size() / 3;
        
        glGenVertexArrays(1, &orbitVAO);
        glGenBuffers(1, &orbitVBO);
        glBindVertexArray(orbitVAO);
        glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
        glBufferData(GL_ARRAY_BUFFER, orbitVertices.size() * sizeof(float), orbitVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        std::vector<float> satVerts;
        std::vector<unsigned int> satInds;
        generateSimpleSphere(satVerts, satInds, SAT_RADIUS_VIS, 20, 20);
        satIndexCount = satInds.size();
        glGenVertexArrays(1, &satVAO);
        glGenBuffers(1, &satVBO);
        glGenBuffers(1, &satEBO);
        glBindVertexArray(satVAO);
        glBindBuffer(GL_ARRAY_BUFFER, satVBO);
        glBufferData(GL_ARRAY_BUFFER, satVerts.size() * sizeof(float), satVerts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, satEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, satInds.size() * sizeof(unsigned int), satInds.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }
    
    void update(double t) {
        if (destroyed) return;
        double M = meanMotion * (t - T0);
        M = fmod(M, 2.0 * M_PI);
        double E = M;
        for (int i = 0; i < 10; ++i) {
            E = E - (E - e * sin(E) - M) / (1.0 - e * cos(E));
        }
        double cosE = cos(E);
        double sinE = sin(E);
        double a = a_km;
        double b = a * sqrt(1.0 - e*e);
        
        double x_orb = a * (cosE - e);
        double y_orb = b * sinE;
        
        double dEdt = meanMotion / (1.0 - e * cosE);
        double vx_orb = -a * sinE * dEdt;
        double vy_orb =  b * cosE * dEdt;
        
        double cosw = cos(w_rad), sinw = sin(w_rad);
        double x_rot =  x_orb * cosw + y_orb * sinw;
        double y_rot = -x_orb * sinw + y_orb * cosw;
        double vx_rot =  vx_orb * cosw + vy_orb * sinw;
        double vy_rot = -vx_orb * sinw + vy_orb * cosw;
        
        double cosW = cos(W_rad), sinW = sin(W_rad);
        double cosi = cos(i_rad), sini = sin(i_rad);
        
        double x_eq = x_rot * cosW - y_rot * cosi * sinW;
        double y_eq = x_rot * sinW + y_rot * cosi * cosW;
        double z_eq = y_rot * sini;
        
        position_km = glm::dvec3(x_eq, y_eq, z_eq);
        
        double vx_eq = vx_rot * cosW - vy_rot * cosi * sinW;
        double vy_eq = vx_rot * sinW + vy_rot * cosi * cosW;
        double vz_eq = vy_rot * sini;
        
        velocity_km = glm::dvec3(vx_eq, vy_eq, vz_eq);
    }
    
    void cleanup() {
        if (orbitVAO) glDeleteVertexArrays(1, &orbitVAO);
        if (orbitVBO) glDeleteBuffers(1, &orbitVBO);
        if (satVAO) glDeleteVertexArrays(1, &satVAO);
        if (satVBO) glDeleteBuffers(1, &satVBO);
        if (satEBO) glDeleteBuffers(1, &satEBO);
    }
};

// ----------------------- Чтение elems.txt -----------------------
std::vector<Satellite> loadSatellitesFromFile(const std::string& filename, bool withOpenGL) {
    std::vector<Satellite> satellites;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть " << filename << std::endl;
        return satellites;
    }
    
    std::string line;
    int id = 1;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        Satellite sat;
        sat.id = id++;
        
        if (line.find("a = ") != 0) {
            std::cerr << "Ошибка формата: ожидалось 'a = ...'" << std::endl;
            break;
        }
        sat.a_km = std::strtod(line.substr(4).c_str(), nullptr);
        
        if (!std::getline(file, line) || line.find("e = ") != 0) break;
        sat.e = std::strtod(line.substr(4).c_str(), nullptr);
        if (sat.e >= 1.0) sat.e = 0.99;
        
        if (!std::getline(file, line) || line.find("i = ") != 0) break;
        double i_deg = std::strtod(line.substr(4).c_str(), nullptr);
        sat.i_rad = i_deg * M_PI / 180.0;
        
        if (!std::getline(file, line) || line.find("W = ") != 0) break;
        double W_deg = std::strtod(line.substr(4).c_str(), nullptr);
        sat.W_rad = W_deg * M_PI / 180.0;
        
        if (!std::getline(file, line) || line.find("w = ") != 0) break;
        double w_deg = std::strtod(line.substr(4).c_str(), nullptr);
        sat.w_rad = w_deg * M_PI / 180.0;
        
        // Новая строка: истинная аномалия nu
        if (!std::getline(file, line) || line.find("nu = ") != 0) break;
        double nu_deg = std::strtod(line.substr(5).c_str(), nullptr);
        sat.nu0_rad = nu_deg * M_PI / 180.0;
        
        // Строка статуса
        if (!std::getline(file, line)) break;
        sat.statusMsg = line;
        
        std::string lowerMsg = line;
        std::transform(lowerMsg.begin(), lowerMsg.end(), lowerMsg.begin(), ::tolower);
        if (lowerMsg.find("разобьётся") != std::string::npos ||
            lowerMsg.find("аннигилировался") != std::string::npos ||
            lowerMsg.find("под поверхностью") != std::string::npos) {
            sat.destroyed = true;
            sat.orbitVisible = false;
        }
        
        static const glm::vec3 colors[] = {
            {1.0f, 0.5f, 0.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f},
            {1.0f, 0.0f, 1.0f}, {0.5f, 0.5f, 1.0f}
        };
        sat.color = colors[(sat.id - 1) % 5];
        
        sat.initOrbitParams();
        if (withOpenGL) {
            sat.initGL();
        }
        satellites.push_back(sat);
    }
    return satellites;
}

// ----------------------- Рендеринг текста -----------------------
void renderText(unsigned int shaderProgram, const std::string& text, float x, float y, 
                glm::vec3 color, float scale = 1.0f) {
    glm::mat4 projection = glm::ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT);
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), color.r, color.g, color.b);
    
    char buffer[60000];
    int num_quads = stb_easy_font_print(0, 0, (char*)text.c_str(), nullptr, buffer, sizeof(buffer));
    
    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, num_quads * 16 * sizeof(float), buffer, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(scale, scale, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    
    glDrawArrays(GL_QUADS, 0, num_quads * 4);
    
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

// ----------------------- Callbacks -----------------------
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mousePressed = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        } else {
            mousePressed = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!mousePressed) return;
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = (xpos - lastX) * mouseSensitivity;
    float yoffset = (lastY - ypos) * mouseSensitivity;
    lastX = xpos;
    lastY = ypos;
    yaw += xoffset;
    pitch += yoffset;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    camX = radius * cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    camY = radius * sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camZ = radius * sin(glm::radians(pitch));
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    radius -= (float)yoffset * 0.5f;
    if (radius < 2.0f) radius = 2.0f;
    if (radius > 15.0f) radius = 15.0f;
    camX = radius * cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    camY = radius * sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camZ = radius * sin(glm::radians(pitch));
}

// ----------------------- Обработка клавиш -----------------------
void processInput(GLFWwindow* window, std::vector<Satellite>& satellites) {
    float rotateSpeed = 1.0f;
    bool rotated = false;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { yaw -= rotateSpeed; rotated = true; }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { yaw += rotateSpeed; rotated = true; }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { pitch += rotateSpeed; if (pitch > 89.0f) pitch = 89.0f; rotated = true; }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { pitch -= rotateSpeed; if (pitch < -89.0f) pitch = -89.0f; rotated = true; }

    if (rotated) {
        camX = radius * cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        camY = radius * sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        camZ = radius * sin(glm::radians(pitch));
    }

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        mouseSensitivity += 0.005f;
        if (mouseSensitivity > 1.0f) mouseSensitivity = 1.0f;
        glfwWaitEventsTimeout(0.1);
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        mouseSensitivity -= 0.005f;
        if (mouseSensitivity < 0.01f) mouseSensitivity = 0.01f;
        glfwWaitEventsTimeout(0.1);
    }

    for (size_t i = 0; i < satellites.size() && i < 5; ++i) {
        int key = GLFW_KEY_1 + (int)i;
        if (glfwGetKey(window, key) == GLFW_PRESS) {
            if (!satellites[i].destroyed)
                satellites[i].orbitVisible = !satellites[i].orbitVisible;
            glfwWaitEventsTimeout(0.15);
        }
    }
    
    bool shiftPressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
                        (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
    float step = shiftPressed ? 10.0f : 1.0f;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        timeScale = std::max(1.0f, timeScale - step);
        std::cout << "Time scale: " << timeScale << "x" << std::endl;
        char title[100];
        snprintf(title, sizeof(title), "Earth Viewer - Time scale: %.0fx", timeScale);
        glfwSetWindowTitle(window, title);
        glfwWaitEventsTimeout(0.15);
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        timeScale = std::min(10000.0f, timeScale + step);
        std::cout << "Time scale: " << timeScale << "x" << std::endl;
        char title[100];
        snprintf(title, sizeof(title), "Earth Viewer - Time scale: %.0fx", timeScale);
        glfwSetWindowTitle(window, title);
        glfwWaitEventsTimeout(0.15);
    }

    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        yaw = 0.0f; pitch = 0.0f;
        radius = 6.0f;
        glfwSetWindowTitle(window, "Earth Viewer - View: +X");
        glfwWaitEventsTimeout(0.2);
    }
    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) {
        yaw = 90.0f; pitch = 0.0f;
        radius = 6.0f;
        glfwSetWindowTitle(window, "Earth Viewer - View: +Y");
        glfwWaitEventsTimeout(0.2);
    }
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        yaw = 0.0f; pitch = 90.0f;
        radius = 6.0f;
        glfwSetWindowTitle(window, "Earth Viewer - View: +Z");
        glfwWaitEventsTimeout(0.2);
    }
    camX = radius * cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    camY = radius * sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camZ = radius * sin(glm::radians(pitch));
}

// ----------------------- Проверка столкновений -----------------------
void checkCollisions(std::vector<Satellite>& satellites, std::string& destructionMessage, float& messageTimer) {
    const double COLLISION_DIST_KM = R_EARTH_KM + SAT_RADIUS_KM;
    for (auto& sat : satellites) {
        if (sat.destroyed) continue;
        double dist = glm::length(sat.position_km);
        if (dist < COLLISION_DIST_KM) {
            sat.destroyed = true;
            sat.orbitVisible = false;
            std::ostringstream ss;
            ss << "Спутник №" << sat.id << " аннигилировался о поверхность Земли";
            destructionMessage = ss.str();
            messageTimer = 3.0f;
        }
    }
}

// ----------------------- Автоматическая запись траекторий -----------------------
void recordTrajectories(const std::vector<Satellite>& satellites, double totalTime, double stepSec) {
    for (const auto& sat : satellites) {
        if (sat.destroyed) continue;
        std::string filename = "satellite_" + std::to_string(sat.id) + "_orbit.csv";
        std::ofstream file(filename);
        file << "time,x_km,y_km,z_km\n";
        
        Satellite simSat = sat;
        for (double t = 0.0; t <= totalTime; t += stepSec) {
            simSat.update(t);
            file << std::fixed << std::setprecision(3)
                 << t << ","
                 << simSat.position_km.x << ","
                 << simSat.position_km.y << ","
                 << simSat.position_km.z << "\n";
        }
        file.close();
        std::cout << "Saved trajectory to " << filename << std::endl;
    }
}

// ----------------------- Main -----------------------
int main(int argc, char* argv[]) {
    // Проверяем аргументы командной строки
    if (argc >= 2) {
        // Режим записи CSV (без OpenGL)
        std::vector<Satellite> satellites = loadSatellitesFromFile("elems.txt", false);
        if (satellites.empty()) {
            std::cerr << "Не найдено ни одного спутника в elems.txt." << std::endl;
            return -1;
        }

        double periods = std::atof(argv[1]);
        double stepSec = 60.0;
        if (argc >= 3) {
            stepSec = std::atof(argv[2]);
        }
        if (periods <= 0.0) {
            std::cerr << "Количество периодов должно быть положительным." << std::endl;
            return -1;
        }
        
        double maxPeriod = 0.0;
        for (const auto& sat : satellites) {
            if (!sat.destroyed && sat.orbitPeriod > maxPeriod)
                maxPeriod = sat.orbitPeriod;
        }
        if (maxPeriod <= 0.0) {
            std::cerr << "Нет активных спутников с корректной орбитой." << std::endl;
            return -1;
        }
        
        double totalTime = periods * maxPeriod;
        std::cout << "Запись траекторий на " << periods << " периодов ("
                  << totalTime << " секунд) с шагом " << stepSec << " с." << std::endl;
        
        recordTrajectories(satellites, totalTime, stepSec);
        std::cout << "Запись завершена." << std::endl;
        return 0;
    }

    // --- Графический режим (без аргументов) ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Earth Viewer - Time scale: 1x", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0f);

    auto compileProgram = [](const char* vs, const char* fs) {
        unsigned int v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
        unsigned int f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
        unsigned int p = glCreateProgram();
        glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
        glDeleteShader(v); glDeleteShader(f);
        return p;
    };
    unsigned int earthProgram = compileProgram(vertexShaderSource, fragmentShaderSource);
    unsigned int satProgram   = compileProgram(colorVertexShader, colorFragmentShader);
    unsigned int lineProgram  = compileProgram(lineVertexShader, lineFragmentShader);
    unsigned int textProgram  = compileProgram(textVertexShader, textFragmentShader);

    // Земля
    std::vector<float> sphereVerts;
    std::vector<unsigned int> sphereInds;
    generateSphere(sphereVerts, sphereInds, 1.0f, 180, 90);
    unsigned int earthVAO, earthVBO, earthEBO;
    glGenVertexArrays(1, &earthVAO);
    glGenBuffers(1, &earthVBO);
    glGenBuffers(1, &earthEBO);
    glBindVertexArray(earthVAO);
    glBindBuffer(GL_ARRAY_BUFFER, earthVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVerts.size() * sizeof(float), sphereVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, earthEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereInds.size() * sizeof(unsigned int), sphereInds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    unsigned int earthTex = loadTexture("earth_map.jpg");

    // Оси
    std::vector<float> axesVerts; generateAxesLines(axesVerts);
    unsigned int axesVAO, axesVBO;
    glGenVertexArrays(1, &axesVAO); glGenBuffers(1, &axesVBO);
    glBindVertexArray(axesVAO); glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
    glBufferData(GL_ARRAY_BUFFER, axesVerts.size() * sizeof(float), axesVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Стрелки осей
    std::vector<float> arrowVerts; generateArrows(arrowVerts);
    unsigned int arrowVAO, arrowVBO;
    glGenVertexArrays(1, &arrowVAO); glGenBuffers(1, &arrowVBO);
    glBindVertexArray(arrowVAO); glBindBuffer(GL_ARRAY_BUFFER, arrowVBO);
    glBufferData(GL_ARRAY_BUFFER, arrowVerts.size() * sizeof(float), arrowVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Загрузка спутников с OpenGL
    std::vector<Satellite> satellites = loadSatellitesFromFile("elems.txt", true);
    if (satellites.empty()) {
        std::cerr << "Не найдено ни одного спутника в elems.txt." << std::endl;
    }

    std::string destructionMessage;
    float messageTimer = 0.0f;

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH/SCR_HEIGHT, 0.1f, 100.0f);
    yaw = -90.0f; pitch = 0.0f; radius = 6.0f;
    camX = radius * cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    camY = radius * sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camZ = radius * sin(glm::radians(pitch));

    float lastTime = glfwGetTime();
    double simTime = 0.0;

    while (!glfwWindowShouldClose(window)) {
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        simTime += deltaTime * timeScale;
        
        processInput(window, satellites);
        
        for (auto& sat : satellites) {
            sat.update(simTime);
        }
        
        checkCollisions(satellites, destructionMessage, messageTimer);
        if (messageTimer > 0.0f) {
            messageTimer -= deltaTime;
            if (messageTimer <= 0.0f) destructionMessage.clear();
        }
        
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 camPos(camX, camY, camZ);
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        // Земля
        glUseProgram(earthProgram);
        glUniformMatrix4fv(glGetUniformLocation(earthProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(earthProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glm::mat4 earthModel = glm::rotate(glm::mat4(1.0f), (float)(simTime * EARTH_ANGULAR_SPEED), glm::vec3(0.0f, 0.0f, 1.0f));
        glUniformMatrix4fv(glGetUniformLocation(earthProgram, "model"), 1, GL_FALSE, glm::value_ptr(earthModel));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, earthTex);
        glUniform1i(glGetUniformLocation(earthProgram, "texture1"), 0);
        glBindVertexArray(earthVAO);
        glDrawElements(GL_TRIANGLES, sphereInds.size(), GL_UNSIGNED_INT, 0);

        // Орбиты
        glUseProgram(lineProgram);
        glUniformMatrix4fv(glGetUniformLocation(lineProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(lineProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        for (const auto& sat : satellites) {
            if (sat.destroyed) continue;
            if (sat.orbitVisible) {
                glUniform3f(glGetUniformLocation(lineProgram, "color"), sat.color.r, sat.color.g, sat.color.b);
                glm::mat4 orbitRot = glm::rotate(glm::mat4(1.0f), (float)sat.W_rad, glm::vec3(0,0,1));
                orbitRot = glm::rotate(orbitRot, (float)sat.i_rad, glm::vec3(1,0,0));
                orbitRot = glm::rotate(orbitRot, (float)(-sat.w_rad), glm::vec3(0,0,1)); // минус w!
                glUniformMatrix4fv(glGetUniformLocation(lineProgram, "model"), 1, GL_FALSE, glm::value_ptr(orbitRot));
                glBindVertexArray(sat.orbitVAO);
                glDrawArrays(GL_LINE_LOOP, 0, sat.orbitVertexCount);
            }
        }

        // Спутники
        glUseProgram(satProgram);
        glUniformMatrix4fv(glGetUniformLocation(satProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(satProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        for (auto& sat : satellites) {
            if (sat.destroyed) continue;
            glm::dvec3 pos_vis = sat.position_km * SCALE;
            glm::mat4 satModel = glm::translate(glm::mat4(1.0f), glm::vec3(pos_vis));
            glUniformMatrix4fv(glGetUniformLocation(satProgram, "model"), 1, GL_FALSE, glm::value_ptr(satModel));
            glUniform3f(glGetUniformLocation(satProgram, "color"), sat.color.r, sat.color.g, sat.color.b);
            glBindVertexArray(sat.satVAO);
            glDrawElements(GL_TRIANGLES, sat.satIndexCount, GL_UNSIGNED_INT, 0);
        }

        // Оси
        glDisable(GL_DEPTH_TEST);
        glUseProgram(lineProgram);
        glUniformMatrix4fv(glGetUniformLocation(lineProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(lineProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glm::mat4 identity(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(lineProgram, "model"), 1, GL_FALSE, glm::value_ptr(identity));
        
        glBindVertexArray(axesVAO);
        glUniform3f(glGetUniformLocation(lineProgram, "color"), 1,0,0); glDrawArrays(GL_LINES, 0, 2);
        glUniform3f(glGetUniformLocation(lineProgram, "color"), 0,1,0); glDrawArrays(GL_LINES, 2, 2);
        glUniform3f(glGetUniformLocation(lineProgram, "color"), 0,0,1); glDrawArrays(GL_LINES, 4, 2);
        glUniform3f(glGetUniformLocation(lineProgram, "color"), 1,1,1);
        glBindVertexArray(arrowVAO);
        glDrawArrays(GL_LINES, 0, arrowVerts.size()/3);
        glEnable(GL_DEPTH_TEST);

        if (!destructionMessage.empty()) {
            renderText(textProgram, destructionMessage, 20.0f, SCR_HEIGHT - 50.0f, glm::vec3(1.0f, 0.0f, 0.0f), 2.0f);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (auto& sat : satellites) {
        sat.cleanup();
    }

    glfwTerminate();
    return 0;
}