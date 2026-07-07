#include<shaderProgram.h>

std::string getFileContents(const char* file) {
	std::ifstream in(file, std::ios::binary);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	throw(errno);
}

ShaderProgram::ShaderProgram(const char* vertexFile, const char* fragmentFile) {
	std::string vertexCode = getFileContents(vertexFile);
	std::string fragmentCode = getFileContents(fragmentFile);

    const char* vertSrc = vertexCode.c_str();
	const char* fragSrc = fragmentCode.c_str();

    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader,1,&vertSrc,NULL);
    glCompileShader(vShader);
    compileErrors(vShader,"VERTEX");

    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader,1,&fragSrc,NULL);
    glCompileShader(fShader);
    compileErrors(fShader,"FRAGMENT");

    program = glCreateProgram();

    glAttachShader(program,vShader);
    glAttachShader(program,fShader);
    glLinkProgram(program);
    compileErrors(program,"PROGRAM");

    glDeleteShader(vShader);
    glDeleteShader(fShader);
}

void ShaderProgram::Activate() {
    glUseProgram(program);
}

void ShaderProgram::Delete() {
    glDeleteProgram(program);
}

void ShaderProgram::compileErrors(GLuint shader, const char* type)
{
	// Stores status of compilation
	GLint hasCompiled;
	// Character array to store error message in
	char infoLog[1024];
	if (type != "PROGRAM")
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_COMPILATION_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "SHADER_LINKING_ERROR for:" << type << "\n" << infoLog << std::endl;
		}
	}
}