#include <cmath>
#include <filesystem>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "shaderProgram.h"
#include <glm/glm.hpp>
#include <gmpxx.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include "cmpl/complexBigNum.hpp"
#include <iostream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define IMAGE_PATH "img/"
#define NORMAL_IMAGE "s_"
#define HIGH_RES_IMAGE "hs_"

using namespace mpfr;

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

int s_width = 8000;
int s_height = 8000;

bool high_res_mode = false;

int max_iter = 1000;
int iter_round_size = 50;
int orbit_iter = max_iter;
int max_ref_iter = 0;
int current_iteration = iter_round_size;
int curr_orbit_iter = 0;
int hist_size = 1024;
int gradient_scale = 256;

double mpower = 2;

bool holding_shift = false;

mpreal c_x = 0.0;
mpreal c_y = 0.0;
mpreal w = 2.0;

std::vector<glm::dvec2> orbit;
ComplexBigNum z (0,0);
GLuint ssbo[3];

std::string mpf_to_string(const mpreal& value) {
    mp_bitcnt_t prec_bits = value.get_prec();
    
    int decimal_digits = std::ceil(prec_bits * 0.3010299956639812);
    
    std::stringstream ss;
    ss << std::setprecision(decimal_digits) << value;
    
    return ss.str();
}

std::string get_pos_string() {
    return mpf_to_string(c_x) + "_" + mpf_to_string(c_y) + "_" + mpf_to_string(w);
}

void check_folder() {
    if (!std::filesystem::exists(IMAGE_PATH)) {
        std::filesystem::create_directories(IMAGE_PATH);
    }
}

void save_screenshot(int width, int height) {
    std::vector<unsigned char> pixels(width * height * 4);

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    stbi_flip_vertically_on_write(true);
    std::string res = IMAGE_PATH;
    res += NORMAL_IMAGE + get_pos_string() + ".png";
    check_folder();
    stbi_write_png(res.c_str(), width, height, 4, pixels.data(), width * 4);
}

ComplexBigNum complex_power(const ComplexBigNum& num, double power_double) {
    mpreal p = power_double;

    mpreal r = num.re * num.re + num.im * num.im;

    mpreal theta = atan2(num.im,num.re);

    mpreal rp = pow(r, p/2.0);

    mpreal a = p * theta;
    mpreal c = cos(a);
    mpreal s = sin(a);

    return ComplexBigNum{ rp * c, rp * s };
}

void reference_orbit(std::vector<glm::dvec2> &orbit, int &max_iter, int &iter) {
    ComplexBigNum center (c_x,c_y);

    while (z.norm() < 4.0 && iter < max_iter) {
        orbit.push_back(glm::dvec2(z.re.toDouble(),z.im.toDouble()));
        if (mpower != 2.0) {
            z = complex_power(z,mpower) + center;
        } else {
            z = z * z + center;
        }
        iter++;
    }
    max_ref_iter = iter;
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

mpreal exact_remap(mpreal x, mpreal a0, mpreal a1, mpreal b0, mpreal b1) {
    mpreal res = b0 + (x - a0) * (b1 - b0) / (a1 - a0);
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
    mpreal aspect_ratio = (double)(height)/width;
    mpreal h = mpreal(w * aspect_ratio);
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
            mpreal x_val = exact_remap(x,0,width,-w,w);
            mpreal y_val = exact_remap(y,0,height,-h,h);
            batch[i].delta_x = x_val.toDouble();
            batch[i].delta_y = y_val.toDouble();
        }
    }
    current_iteration = iter_round_size;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size * sizeof(MandStep), batch.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void zoom(double mouseX, double mouseY, double zoomFactor) {
    mpreal aspect_ratio = (double)(height)/width;
    mpreal Px = exact_remap(mouseX, 0, width, c_x-w, c_x+w);
    mpreal z = zoomFactor;
    mpreal Py = exact_remap(mouseY, 0, height,c_y+w*aspect_ratio,c_y-w*aspect_ratio);
    c_x = Px + (c_x - Px) * z;
    c_y = Py + (c_y - Py) * z;

    w = w * z;

    // std::cout << std::setprecision(4) << "width: " << w << std::endl;
    reset_ssbo();
    reset_orbit_ssbo(); 
}
void zoom_at_pos(mpreal x, mpreal y, double zoomFactor) {
    mpreal z = zoomFactor;
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
    } else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        w = 2;
        c_x = 0;
        c_y = 0;
        reset_ssbo();
        reset_orbit_ssbo();
    } else if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        save_screenshot(width,height);
    } else if (key == GLFW_KEY_H && action == GLFW_PRESS) { 
        high_res_mode = true;
    } else if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS) {
        holding_shift = true;
    } else if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_RELEASE) {
        holding_shift = false;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    double zoomFactor = (yoffset > 0) ? 0.5f : 2.0f;
    if (!holding_shift) {
        zoom(mouseX,mouseY,zoomFactor);
    } else {
        mpower += (yoffset > 0) ? 0.1 : -0.1;
        reset_ssbo();
        reset_orbit_ssbo(); 
    }
}

void move(double x, double y) {
    mpreal aspect_ratio = (double)(height)/width;
    mpreal Px = exact_remap(x, 0, width, c_x-w, c_x+w);
    mpreal Py = exact_remap(y, 0, height,c_y+w*aspect_ratio,c_y-w*aspect_ratio);
    c_x = Px;
    c_y = Py;
    reset_ssbo();
    reset_orbit_ssbo(); 
}

void mouse_cursor_callback( GLFWwindow * window,  int button, int action, int mods)
{
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if(GLFW_PRESS == action) {
            move(mouseX,mouseY);
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
    
    mpreal::set_default_prec(500);
    if (argc == 4) {
        c_x = mpreal(argv[1]);
        c_y = mpreal(argv[2]);
        w = mpreal(argv[3]);
    } else if (argc == 2) {
        c_x = mpreal(0);
        c_y = mpreal(0);
        w = mpreal(2);
        mpower = std::stod(argv[1]);
    } else{
        c_x = mpreal(0);
        c_y = mpreal(0);
        w = mpreal(2);
    }

    ComplexBigNum z(0, 0);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS, GLFW_LOSE_CONTEXT_ON_RESET);
    GLFWwindow* window = glfwCreateWindow(width, height, "Mandelbort", glfwGetPrimaryMonitor(), NULL);
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

    GLint max_ssbo_size = 0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_ssbo_size);
    std::cout << "Max SSBO Size: " << (max_ssbo_size / 1024 / 1024) << " MB\n";

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
    GLint loc_pow = glGetUniformLocation(prog.program, "mpower");
    GLint loc_cen = glGetUniformLocation(prog.program, "center");
    GLint loc_ref = glGetUniformLocation(prog.program, "max_ref_iter");

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
    mpreal aspect_ratio = (double)(height)/width;
    mpreal h = w * aspect_ratio;
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

    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLuint textureColorbuffer;
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_width, s_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

    // Unbind FBO to return to default rendering
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    bool is_capturing_high_res = false;
    while (!glfwWindowShouldClose(window))
    {
        int new_width;
        int new_height;
        glfwGetFramebufferSize(window, &new_width, &new_height);

        if (!is_capturing_high_res && (new_width != width || new_height != height)) {
            width = new_width;
            height = new_height;
            glViewport(0, 0, new_width, new_height);
            reset_ssbo();
        }

        if (high_res_mode && !is_capturing_high_res) {
            is_capturing_high_res = true;
            high_res_mode = false;
            width = s_height;
            height = s_height;
            reset_ssbo();
        }

        if (is_capturing_high_res) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glViewport(0,0,width,height);
            if (current_iteration >= max_iter) {
                std::vector<unsigned char> pixels(s_width * s_height * 4);
                glReadPixels(0, 0, s_width, s_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                stbi_flip_vertically_on_write(true);
                std::string res = IMAGE_PATH;
                res += HIGH_RES_IMAGE + get_pos_string() + ".png";
                check_folder();
                stbi_write_png(res.c_str(), s_width, s_height, 4, pixels.data(), s_width * 4);
                is_capturing_high_res = false;
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                width = new_width;
                height = new_height;
                glViewport(0, 0, width, height);
                reset_ssbo();
            }
        }
        glClear(GL_COLOR_BUFFER_BIT);
        prog.Activate();
        glUniform1i(loc_w, width);
        glUniform1i(loc_h, height);
        glUniform1i(loc_iter, current_iteration);
        glUniform1i(loc_grad, gradient_scale);
        glUniform1i(loc_hist, hist_size);
        glUniform1d(loc_pow,mpower);
        glUniform2d(loc_cen,c_x.toDouble(),c_y.toDouble());
        glUniform1i(loc_ref, max_ref_iter);
        
        glBindVertexArray(VAO);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
        current_iteration += iter_round_size;
        current_iteration = std::fmin(current_iteration, max_iter);

        printf("%d\n",current_iteration);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(3, ssbo);
    prog.Delete();
    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}