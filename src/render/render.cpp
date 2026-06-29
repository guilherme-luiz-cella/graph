#include "render.h"
#include <glad/gl.h>

void renderItem(const DrawItem& it, const Shader& s,
                bool multiTexOn, bool normalMapOn, bool envMapOn) {
    s.setMat4("model", it.xform);
    bool multi  = it.useMultiTex  && multiTexOn  && it.diffuse0 && it.diffuse1;
    bool normal = it.useNormalMap && normalMapOn && it.normal0;
    bool env    = it.useEnvMap    && envMapOn;
    s.setInt("useMultiTex",  multi  ? 1 : 0);
    s.setInt("useNormalMap", normal ? 1 : 0);
    s.setInt("useEnvMap",    env    ? 1 : 0);
    // 0 = opaque, 1 = fixed translucency, 2 = texture-driven alpha (glass)
    s.setInt("useAlpha", it.useAlpha ? 1 : (it.useTexAlpha ? 2 : 0));
    s.setInt("useEmissive", 0);
    if (it.diffuse0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, it.diffuse0); s.setInt("tex_diffuse0", 0); }
    if (it.diffuse1) { glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, it.diffuse1); s.setInt("tex_diffuse1", 1); }
    if (it.normal0)  { glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, it.normal0);  s.setInt("tex_normal0", 2); }
    if (it.mesh)  it.mesh->draw(s);
    if (it.model) it.model->draw(s);
}

void renderDepth(const DrawItem& it, const Shader& s) {
    s.setMat4("model", it.xform);
    if (it.mesh)  it.mesh->draw(s);
    if (it.model) it.model->draw(s);
}
