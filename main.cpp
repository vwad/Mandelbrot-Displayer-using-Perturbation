#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "shaderProgram.h"
#include <glm/glm.hpp>
#include "gmpxx.h"
#include <vector>
#include <iomanip>
#include "cmpl/complexBigNum.hpp"

struct MandStep {
    double dz_x;
    double dz_y;
    double delta_x;
    double delta_y;
    int iter;
    int index;
    int is_finished;
};

int width = 500;
int height = 500;
int max_iter = 10000;
int iter_round_size = 50;
int orbit_iter = max_iter;
int current_iteration = iter_round_size;
int curr_orbit_iter = 0;
int hist_size = 2048;
int gradient_scale = 512;

mpf_class c_x = 0.0;
mpf_class c_y = 0.0;
mpf_class w = 2.0;

std::vector<glm::dvec2> orbit;
ComplexBigNum z (0,0);
GLuint ssbo[3];

void reference_orbit(std::vector<glm::dvec2> &orbit, int &max_iter, int &iter) {
    ComplexBigNum center (c_x,c_y);

    while (z.norm() < 4.0 && iter < max_iter) {
        orbit.push_back(glm::dvec2(z.re.get_d(),z.im.get_d()));
        z = z * z + center;
        iter++;
    }
}

void generate_histogram_gradients(std::vector<glm::vec4> &gradientColors) {
    std::vector<glm::vec3> colors = {
        glm::vec3(0.0f,         7.0f/255,   100.0f/255),
        glm::vec3(32.0f/255,    107.0f/255, 203.0f/255),
        glm::vec3(237.0f/255,   1.0f,       1.0f),
        glm::vec3(1.0f,         170.0f/255, 0.0f),
        glm::vec3(0.0f,         2.0f/255,   0.0f)
    };
    std::vector<float> fractions = {
        0.0f,
        0.16f,
        0.42f,
        0.6425f,
        0.8575f
    };

    int size = gradientColors.size();

    for (int i = 0; i < colors.size(); i++) {
        glm::vec3 from = colors[i];
        glm::vec3 to = i != colors.size() - 1 ? colors[i + 1] : colors[0];

        int start = (int) (fractions[i] * (size - 1));
        int end = i != colors.size() - 1 ? (int) (fractions[i + 1] * (size - 1)) : size - 1;

        for (int v = start; v <= end; v++) {
            float scalar = ((float) (v - start)) / (end - start);

            float red = ((to.x - from.x) * scalar + from.x);
            float green = ((to.y - from.y) * scalar + from.y);
            float blue = ((to.z - from.z) * scalar + from.z);

            gradientColors[v] = glm::vec4(red, green, blue, 1.0f);

        }
    }
}

mpf_class exact_remap(mpf_class x, mpf_class a0, mpf_class a1, mpf_class b0, mpf_class b1) {
    mpf_class res = b0 + (x - a0) * (b1 - b0) / (a1 - a0);
    return res;
}

void reset_orbit_ssbo() {
    curr_orbit_iter = 0;
    orbit_iter = max_iter;
    orbit.clear();
    z = ComplexBigNum(0,0);
    reference_orbit(orbit,orbit_iter,curr_orbit_iter);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, orbit.size() * sizeof(glm::dvec2), orbit.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void reset_ssbo() {
    int size = width * height;
    mpf_class aspect_ratio = (double)(height)/width;
    mpf_class h = mpf_class(w * aspect_ratio);
    std::vector<MandStep> batch(size);
    #pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int i = y * width + x;
            batch[i].dz_x = 0;
            batch[i].dz_y = 0;
            batch[i].iter = 0;
            batch[i].is_finished = 0;
            batch[i].index = 0;
            mpf_class x_val = exact_remap(x,0,width,-w,w);
            mpf_class y_val = exact_remap(y,0,height,-h,h);
            batch[i].delta_x = x_val.get_d();
            batch[i].delta_y = y_val.get_d();
        }
    }
    current_iteration = iter_round_size;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size * sizeof(MandStep), batch.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void zoom(double mouseX, double mouseY, double zoomFactor) {
    mpf_class aspect_ratio = (double)(height)/width;
    mpf_class Px = exact_remap(mouseX, 0, width, c_x-w, c_x+w);
    mpf_class z = zoomFactor;
    mpf_class Py = exact_remap(mouseY, 0, height,c_y+w*aspect_ratio,c_y-w*aspect_ratio);
    std::cout << std::setprecision(40) << "x: " << Px << ", y:" << Py << std::endl;
    c_x = Px + (c_x - Px) * z;
    c_y = Py + (c_y - Py) * z;

    w = w * z;

   reset_ssbo();
    reset_orbit_ssbo();
}
void zoom_at_pos(mpf_class x, mpf_class y, double zoomFactor) {
    mpf_class z = zoomFactor;
    c_x = x + (c_x - x) * z;
    c_y = y + (c_y - y) * z;

    w = w * z;

    reset_ssbo();
    reset_orbit_ssbo();
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (key == GLFW_KEY_EQUAL && action == GLFW_PRESS) {
        max_iter *= 2;
        reset_ssbo();
        reset_orbit_ssbo();
    } else if (key == GLFW_KEY_MINUS && action == GLFW_PRESS) {
        max_iter /= 2;
        reset_ssbo();
        reset_orbit_ssbo();
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    double zoomFactor = (yoffset > 0) ? 0.5f : 2.0f;

    zoom(mouseX,mouseY,zoomFactor);
}

void mouse_cursor_callback( GLFWwindow * window,  int button, int action, int mods)
{
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    mouseY = height - mouseY;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if(GLFW_PRESS == action) {
            zoom(mouseX,mouseY,0.5f);
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if(GLFW_PRESS == action) {
            zoom(mouseX,mouseY,2.0f);
        }
    }
}

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, 
                                GLenum severity, GLsizei length, 
                                const GLchar* message, const void* userParam) {
    if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM) {
        std::cerr << "GL CALLBACK: " << (type == GL_DEBUG_TYPE_ERROR ? "** ERROR **" : "") 
                  << " type = 0x" << std::hex << type 
                  << ", severity = 0x" << severity 
                  << ", message = " << message << std::endl;
    }
}

int main(int argc, char** argv) {
    
    mpf_set_default_prec(5000);
    if (argc == 4) {
        c_x = mpf_class(argv[1]);
        c_y = mpf_class(argv[2]);
        w = mpf_class(argv[3]);
    } else {
        c_x = mpf_class(0);
        c_y = mpf_class(0);
        w = mpf_class(2);
    }

    ComplexBigNum z(0, 0);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS, GLFW_LOSE_CONTEXT_ON_RESET);
    GLFWwindow* window = glfwCreateWindow(width, height, "OpenGL Triangle", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window,scroll_callback);
    glfwSetMouseButtonCallback(window,mouse_cursor_callback);
    

    gladLoadGL();

    const unsigned char* renderer = glGetString(GL_RENDERER);
    std::cout << "Currently using GPU: " << renderer << "\n";
    ShaderProgram prog ("shaders/vert.glsl","shaders/frag.glsl");

    GLint loc_w = glGetUniformLocation(prog.program, "width");
    GLint loc_h = glGetUniformLocation(prog.program, "height");
    GLint loc_tl = glGetUniformLocation(prog.program, "tl");
    GLint loc_br = glGetUniformLocation(prog.program, "br");
    GLint loc_iter = glGetUniformLocation(prog.program, "iter_round_size");
    GLint loc_grad = glGetUniformLocation(prog.program, "gradient_scale");
    GLint loc_hist = glGetUniformLocation(prog.program, "histogram_size");
    GLint loc_cen = glGetUniformLocation(prog.program, "center");

    glUniform2f(loc_tl,-1,1);
    glUniform2f(loc_br,1,-1);

    double vertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f,  1.0f,
        1.0f, -1.0f,
        1.0f,  1.0f,
        -1.0f,  1.0f
    };

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_DOUBLE, GL_FALSE, 2 * sizeof(double), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glViewport(0,0,width,height);
    glfwSetWindowSize(window, width, height);

    int size = width * height;
    mpf_class aspect_ratio = (double)(height)/width;
    mpf_class h = w * aspect_ratio;
    std::vector<MandStep> batch(size);
    std::vector<glm::vec4> gradientColors(hist_size);
    generate_histogram_gradients(gradientColors);
    reset_orbit_ssbo();
    glGenBuffers(3, ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size * sizeof(MandStep), batch.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, orbit.size() * sizeof(glm::dvec2), orbit.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[2]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gradientColors.size() * sizeof(glm::vec4), gradientColors.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo[1]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssbo[2]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);
    
    while (!glfwWindowShouldClose(window))
    {
        int new_width;
        int new_height;
        glfwGetFramebufferSize(window, &new_width, &new_height);

        if (new_width != width || new_height != height) {
            width = new_width;
            height = new_height;
            glViewport(0, 0, width, height);

            reset_ssbo();
        }
        
        glClear(GL_COLOR_BUFFER_BIT);
        prog.Activate();
        glUniform1i(loc_w, width);
        glUniform1i(loc_h, height);
        glUniform1i(loc_iter, current_iteration);

        glUniform1i(loc_grad, gradient_scale);
        glUniform1i(loc_hist, hist_size);

        glUniform2f(loc_cen, (float)c_x.get_d(),(float)c_y.get_d());

        glBindVertexArray(VAO);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();

        current_iteration += iter_round_size;
        current_iteration = std::fmin(current_iteration, max_iter);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(3, ssbo);
    prog.Delete();
    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}