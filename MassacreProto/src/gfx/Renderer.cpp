#include "Universe.h"
#include <mcr/gfx/Renderer.h>

#include <mcr/gfx/Texture.h>
#include <mcr/gfx/Material.h>
#include <mcr/gfx/Mesh.h>

#include "GLState.h"
#include "GLEnums.inl"

namespace mcr {
namespace gfx {

Renderer::Renderer():
    m_activeMaterial(),
    m_activeVertexArray()
{
    GLboolean writeMask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &writeMask);
    m_renderState.depthWrite    = GL_TRUE == writeMask;
    m_renderState.depthTest     = GL_TRUE == glIsEnabled(GL_BLEND);
    m_renderState.alphaTest     = GL_TRUE == glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
    m_renderState.blend         = GL_TRUE == glIsEnabled(GL_BLEND);
    m_renderState.cullFace      = GL_TRUE == glIsEnabled(GL_CULL_FACE);
    m_renderState.polygonOffset = GL_TRUE == glIsEnabled(GL_POLYGON_OFFSET_FILL);

    GLint depthFn, blendFnSrc, blendFnDst;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFn);
    glGetIntegerv(GL_BLEND_SRC,  &blendFnSrc);
    glGetIntegerv(GL_BLEND_DST,  &blendFnDst);

    for (uint i = 0; i < DepthFn::NumFns; ++i)
    {
        if (g_depthFnTable[i] == (GLenum) depthFn)
        {
            m_renderState.depthFunc = DepthFn(i);
            break;
        }
    }

    for (uint i = 0; i < BlendFn::NumFactors; ++i)
    {
        if (g_blendFnTable[i] == (GLenum) blendFnSrc)
            m_renderState.blendFunc.srcFactor = BlendFn::Factor(i);

        if (g_blendFnTable[i] == (GLenum) blendFnDst)
            m_renderState.blendFunc.dstFactor = BlendFn::Factor(i);
    }

    m_renderStateHash = m_renderState.hash();
}

Renderer::~Renderer() {}


const irect& Renderer::viewport() const
{
    return g_glState->viewport();
}

void Renderer::setViewport(const irect& vp)
{
    g_glState->setViewport(vp);
}

void Renderer::setRenderState(const RenderState& rs)
{
    if (m_renderState.depthTest != rs.depthTest)
        (rs.depthTest ? glEnable : glDisable)(GL_DEPTH_TEST);

    if (rs.depthTest)
    {
        if (m_renderState.depthFunc != rs.depthFunc)
            glDepthFunc(g_depthFnTable[rs.depthFunc.fn]);

        if (m_renderState.depthWrite != rs.depthWrite)
            glDepthMask(rs.depthWrite);
    }

    if (m_renderState.blend != rs.blend)
        (rs.depthTest ? glEnable : glDisable)(GL_BLEND);

    if (rs.blend && m_renderState.blendFunc != rs.blendFunc)
    {
        glBlendFunc(g_blendFnTable[rs.blendFunc.srcFactor],
                    g_blendFnTable[rs.blendFunc.dstFactor]);
    }

    if (m_renderState.alphaTest != rs.alphaTest)
        (rs.alphaTest ? glEnable : glDisable)(GL_SAMPLE_ALPHA_TO_COVERAGE);

    if (m_renderState.cullFace != rs.cullFace)
        (rs.cullFace ? glEnable : glDisable)(GL_CULL_FACE);

    if (m_renderState.polygonOffset != rs.polygonOffset)
        (rs.polygonOffset ? glEnable : glDisable)(GL_POLYGON_OFFSET_FILL);

    m_renderState     = rs;
    m_renderStateHash = rs.hash();
}

void Renderer::setActiveMaterial(Material* mtl)
{
    if (m_activeMaterial == mtl)
        return;

    if (m_renderStateHash != mtl->renderStateHash())
        setRenderState(mtl->renderState());

    mtl->syncParameters();
    m_activeMaterial = mtl;
}

void Renderer::setActiveVertexArray(const VertexArray* va)
{
    g_glState->bindVertexArray(va ? va->handle() : 0);
}

void Renderer::drawMesh(const Mesh& mesh)
{
    setActiveVertexArray(mesh.buffer);

    glDrawElements(g_primitiveTypeTable[mesh.primitiveType], mesh.numIndices,
        GL_UNSIGNED_INT, reinterpret_cast<const GLvoid*>(mesh.startIndex << 2));
}

void Renderer::clear()
{
    if (!m_renderState.depthWrite)
    {
        m_renderState.depthWrite = true;
        glDepthMask(true);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

} // ns gfx
} // ns mcr