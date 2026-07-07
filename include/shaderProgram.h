#ifndef SHADER_PROGRAM_H
#define SHADER_PROGRAM_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <filesystem>

class ShaderProgram {
    private:
        void compileErrors(GLuint shader, const char* type);
    public:
        GLuint program;
        ShaderProgram(const char* vertexFile, const char* fragmentFile);
        void Activate();
        void Delete();

};

#endif