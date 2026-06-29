#pragma once
#include <string>
#include <glm/glm.hpp>

struct Shader {
    unsigned int id = 0;
    void load(const std::string& vsPath, const std::string& fsPath);
    void use() const;
    void setInt(const std::string& name, int v) const;
    void setFloat(const std::string& name, float v) const;
    void setVec3(const std::string& name, const glm::vec3& v) const;
    void setVec4(const std::string& name, const glm::vec4& v) const;
    void setMat4(const std::string& name, const glm::mat4& m) const;
    void setMat4Array(const std::string& name, const glm::mat4* m, int count) const;
};
