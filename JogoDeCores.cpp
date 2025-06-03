#include <iostream>
#include <cmath>
#include <ctime>
#include <sstream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;
using namespace glm;

const GLuint WIDTH    = 800;
const GLuint HEIGHT   = 600;
const GLuint ROWS     = 6;
const GLuint COLS     = 8;
const GLuint QUAD_W   = 100;
const GLuint QUAD_H   = 100;
const float dMax = sqrt(3.0f);
const float COLOR_TOLERANCE = 0.2f;

struct Quad {
    vec3 position;
    vec3 dimensions;
    vec3 color;
    bool eliminated;
};

static Quad grid[ROWS][COLS];
static int attempts   = 0;
static int score      = 0;
static bool gameOver  = false;
static int iSelected  = -1;
static GLFWwindow* gWindow = nullptr;
static GLuint shaderID;
static GLuint VAO;
static GLint uniColorLoc;
static GLint uniModelLoc;
static GLint uniProjectionLoc;

const GLchar* vertexShaderSource = R"glsl(
#version 400 core
layout(location = 0) in vec3 vp;
uniform mat4 projection;
uniform mat4 model;
void main()
{
    gl_Position = projection * model * vec4(vp, 1.0);
}
)glsl";

const GLchar* fragmentShaderSource = R"glsl(
#version 400 core
uniform vec4 fc;
out vec4 frg;
void main()
{
    frg = fc;
}
)glsl";

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
GLuint setupShader();
GLuint createQuad();
int eliminarSimilares(float toleranciaNormalized);
bool anyActiveCell();
void updateWindowTitle();
void resetGame();

int main()
{
    srand(static_cast<unsigned int>(time(nullptr)));

    if (!glfwInit())
    {
        return -1;
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    gWindow = glfwCreateWindow(WIDTH, HEIGHT, "Jogo das Cores", nullptr, nullptr);
    if (!gWindow)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(gWindow);
    glfwSetKeyCallback(gWindow, key_callback);
    glfwSetMouseButtonCallback(gWindow, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwTerminate();
        return -1;
    }

    int fbW, fbH;
    glfwGetFramebufferSize(gWindow, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    shaderID = setupShader();
    glUseProgram(shaderID);

    VAO = createQuad();

    uniColorLoc      = glGetUniformLocation(shaderID, "fc");
    uniModelLoc      = glGetUniformLocation(shaderID, "model");
    uniProjectionLoc = glGetUniformLocation(shaderID, "projection");

    glm::mat4 projection = glm::ortho(
        0.0f, float(WIDTH),
        float(HEIGHT), 0.0f,
        -1.0f, 1.0f
    );
    glUniformMatrix4fv(uniProjectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    resetGame();

    while (!glfwWindowShouldClose(gWindow))
    {
        glfwPollEvents();

        if (iSelected >= 0 && !gameOver)
        {
            int removedCount = eliminarSimilares(COLOR_TOLERANCE);
            if (removedCount > 0)
            {
                attempts++;
                int penalty = attempts;
                score += (removedCount - penalty);
                if (score < 0) score = 0;
                cout << "Tentativa " << attempts
                     << ": removidos " << removedCount
                     << " -> +" << removedCount
                     << " - " << penalty
                     << " = Score: " << score << endl;
            }
            if (!anyActiveCell())
            {
                gameOver = true;
                cout << "FIM DE JOGO! Pontuacao final: " << score << endl;
            }
            updateWindowTitle();
            iSelected = -1;
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderID);
        glBindVertexArray(VAO);

        for (int i = 0; i < int(ROWS); i++)
        {
            for (int j = 0; j < int(COLS); j++)
            {
                if (!grid[i][j].eliminated)
                {
                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, grid[i][j].position);
                    model = glm::scale(model, grid[i][j].dimensions);
                    glUniformMatrix4fv(uniModelLoc, 1, GL_FALSE, glm::value_ptr(model));

                    vec3 c = grid[i][j].color;
                    glUniform4f(uniColorLoc, c.r, c.g, c.b, 1.0f);

                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }
            }
        }

        glBindVertexArray(0);
        glfwSwapBuffers(gWindow);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderID);
    glfwDestroyWindow(gWindow);
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    else if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        resetGame();
        cout << "Jogo reiniciado!\n";
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !gameOver)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        int col = static_cast<int>((xpos - QUAD_W / 2) / QUAD_W);
        int row = static_cast<int>((ypos - QUAD_H / 2) / QUAD_H);

        if (col >= 0 && col < int(COLS) && row >= 0 && row < int(ROWS))
        {
            if (!grid[row][col].eliminated)
            {
                iSelected = row * COLS + col;
            }
        }
    }
}

GLuint setupShader()
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexShaderSource, nullptr);
    glCompileShader(vs);
    GLint success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(vs, 512, nullptr, log);
        cerr << "ERRO::VERTEX::COMPILATION_FAILED\n" << log << endl;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(fs, 512, nullptr, log);
        cerr << "ERRO::FRAGMENT::COMPILATION_FAILED\n" << log << endl;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        cerr << "ERRO::PROGRAM::LINKING_FAILED\n" << log << endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

GLuint createQuad()
{
    GLuint VAO_local, VBO;
    GLfloat vertices[] = {
        -0.5f,  0.5f,  0.0f,
        -0.5f, -0.5f,  0.0f,
         0.5f,  0.5f,  0.0f,
         0.5f, -0.5f,  0.0f
    };

    glGenVertexArrays(1, &VAO_local);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO_local);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(GLfloat),
        (void*)0
    );
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return VAO_local;
}

int eliminarSimilares(float toleranciaNormalized)
{
    if (iSelected < 0) return 0;

    int row = iSelected / COLS;
    int col = iSelected % COLS;

    grid[row][col].eliminated = true;
    vec3 target = grid[row][col].color;

    int removedCount = 1;

    for (int i = 0; i < int(ROWS); i++)
    {
        for (int j = 0; j < int(COLS); j++)
        {
            if (!grid[i][j].eliminated)
            {
                vec3 c = grid[i][j].color;
                float dist = sqrtf(
                    (c.r - target.r) * (c.r - target.r) +
                    (c.g - target.g) * (c.g - target.g) +
                    (c.b - target.b) * (c.b - target.b)
                );
                float normalizedDist = dist / dMax;
                if (normalizedDist <= toleranciaNormalized)
                {
                    grid[i][j].eliminated = true;
                    removedCount++;
                }
            }
        }
    }

    iSelected = -1;
    return removedCount;
}

bool anyActiveCell()
{
    for (int i = 0; i < int(ROWS); i++)
    {
        for (int j = 0; j < int(COLS); j++)
        {
            if (!grid[i][j].eliminated)
                return true;
        }
    }
    return false;
}

void updateWindowTitle()
{
    std::ostringstream oss;
    oss << "Jogo das Cores — Score: " << score
        << " — Tentativas: " << attempts;
    if (gameOver)
    {
        oss << " — FIM DE JOGO! Aperte R para reiniciar.";
    }
    glfwSetWindowTitle(gWindow, oss.str().c_str());
}

void resetGame()
{
    attempts  = 0;
    score     = 0;
    gameOver  = false;
    iSelected = -1;

    for (int i = 0; i < int(ROWS); i++)
    {
        for (int j = 0; j < int(COLS); j++)
        {
            Quad& q = grid[i][j];
            q.position = vec3(
                j * float(QUAD_W) + float(QUAD_W) / 2.0f,
                i * float(QUAD_H) + float(QUAD_H) / 2.0f,
                0.0f
            );
            q.dimensions = vec3(
                float(QUAD_W),
                float(QUAD_H),
                1.0f
            );
            float r = float(rand() % 256) / 255.0f;
            float g = float(rand() % 256) / 255.0f;
            float b = float(rand() % 256) / 255.0f;
            q.color = vec3(r, g, b);
            q.eliminated = false;
        }
    }

    updateWindowTitle();
}
