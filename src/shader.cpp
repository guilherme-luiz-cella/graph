#include "shader.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

static std::string slurp(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "shader open fail: " << path << "\n"; return {}; }
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

static unsigned int compile(unsigned int type, const std::string& src) {
    unsigned int s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    int ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << "shader compile fail:\n" << log << "\n";
    }
    return s;
}

void Shader::load(const std::string& vsPath, const std::string& fsPath) {
    unsigned int vs = compile(GL_VERTEX_SHADER, slurp(vsPath));
    unsigned int fs = compile(GL_FRAGMENT_SHADER, slurp(fsPath));
    id = glCreateProgram();
    glAttachShader(id, vs); glAttachShader(id, fs);
    glLinkProgram(id);
    int ok = 0; glGetProgramiv(id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(id, 1024, nullptr, log);
        std::cerr << "shader link fail:\n" << log << "\n";
    }
    glDeleteShader(vs); glDeleteShader(fs);
}

void Shader::use() const { glUseProgram(id); }
void Shader::setInt(const std::string& n, int v) const { glUniform1i(glGetUniformLocation(id, n.c_str()), v); }
void Shader::setFloat(const std::string& n, float v) const { glUniform1f(glGetUniformLocation(id, n.c_str()), v); }
void Shader::setVec3(const std::string& n, const glm::vec3& v) const { glUniform3fv(glGetUniformLocation(id, n.c_str()), 1, glm::value_ptr(v)); }
void Shader::setVec4(const std::string& n, const glm::vec4& v) const { glUniform4fv(glGetUniformLocation(id, n.c_str()), 1, glm::value_ptr(v)); }
void Shader::setMat4(const std::string& n, const glm::mat4& m) const { glUniformMatrix4fv(glGetUniformLocation(id, n.c_str()), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::setMat4Array(const std::string& n, const glm::mat4* m, int count) const { glUniformMatrix4fv(glGetUniformLocation(id, n.c_str()), count, GL_FALSE, glm::value_ptr(m[0])); }
