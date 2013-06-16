#include "Universe.h"
#include <mcr/gfx/Renderer.h>

#include "mcr/gfx/GLState.h"
#include "mcr/gfx/GLEnums.inl"

namespace mcr {
namespace gfx {

Renderer::Renderer():
    m_activeMaterial()
{
    *g_glState;

    glClearColor(0, 0, 0, 1);

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

    for (uint i = 0; i < mtl::DepthFn::NumFns; ++i)
    {
        if (g_depthFnTable[i] == (GLenum) depthFn)
        {
            m_renderState.depthFunc = mtl::DepthFn(i);
            break;
        }
    }

    for (uint i = 0; i < mtl::BlendFn::NumFactors; ++i)
    {
        if (g_blendFnTable[i] == (GLenum) blendFnSrc)
            m_renderState.blendFunc.srcFactor = mtl::BlendFn::Factor(i);

        if (g_blendFnTable[i] == (GLenum) blendFnDst)
            m_renderState.blendFunc.dstFactor = mtl::BlendFn::Factor(i);
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

void Renderer::setRenderState(const mtl::RenderState& rs)
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

void Renderer::setActiveMaterial(mtl::Material* material)
{
    if (m_activeMaterial == material)
        return;

    if (m_renderStateHash != material->renderStateHash())
        setRenderState(material->renderState());

    material->syncParams();
    m_activeMaterial = material;
}

void Renderer::drawMesh(const geom::Mesh& mesh)
{
    g_glState->bindBuffer(GL_ARRAY_BUFFER, mesh.vertices->vbo());
    g_glState->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices->vbo());

    for (uint i = 0; i < mesh.vertexFormat.numAttribs(); ++i)
    {
        auto& attrib = mesh.vertexFormat.attrib(i);

        if (attrib.length)
        {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i,
                attrib.length, g_attribTypeTable[attrib.type], GL_FALSE,
                mesh.vertexFormat.stride(), reinterpret_cast<const GLvoid*>(attrib.offset));
        }
        else
        {
            glDisableVertexAttribArray(i);
        }
    }

    glDrawElementsBaseVertex(
        g_primitiveTypeTable[mesh.primitiveType], mesh.numIndices(),
        GL_UNSIGNED_INT, const_cast<GLvoid*>(mesh.indices->offset()),
        GLint((std::size_t) mesh.vertices->offset() / mesh.vertexFormat.stride()));
}

void Renderer::clear()
{
    if (m_renderState.depthWrite)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    glDepthMask(true);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(false);
}

} // ns gfx
} // ns mcr
