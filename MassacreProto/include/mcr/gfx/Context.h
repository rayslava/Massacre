#pragma once

#include <vector>
#include <mcr/math/Rect.h>
#include <mcr/gfx/RenderState.h>
#include <mcr/gfx/GBuffer.h>

namespace mcr {
namespace gfx {

class ShaderProgram;
class Texture;
class Material;
class VertexArray;

namespace experimental {

class MeshStorage;
struct Mesh;

} // ns experimental

enum TexTarget
{
    Tex1D,
    Tex2D,
    Tex3D,
    TexCube,
    NumTexTargets
};

struct TextureUnit
{
    Texture* textures[NumTexTargets];
    uint refs;
};

class Context
{
public:
    Context();
    ~Context(); // inherit not

    static Context&     active();
    bool                activate();

    const irect&        viewport() const;
    void                setViewport(const irect& viewport);
                        
    const RenderState&  renderState() const;
    void                setRenderState(const RenderState& rs);
                        
    ShaderProgram*      activeProgram() const;
    void                setActiveProgram(ShaderProgram* program);
                        
    uint                activeTextureUnit() const;
    void                setActiveTextureUnit(uint texUnit);
                        
    Texture*            activeTexture(TexTarget target, uint texUnit);
    void                bindTexture(TexTarget target, Texture* tex);
                        
    uint                allocTextureUnit(uint* refs = nullptr);
    void                freeTextureUnit(uint texUnit);
                        
    Material*           activeMaterial() const;
    void                setActiveMaterial(Material* mtl);
                        
    GBuffer*            activeBuffer(GBuffer::Type type);
    GBuffer*            activeBuffer(GBuffer::Type type, uint bufUnit);

    void                bindBuffer(GBuffer* buffer);
    void                bindBufferBase(uint bufUnit, GBuffer* buffer, uint offset);
    void                bindBufferRange(uint bufUnit, GBuffer* buffer, uint offset, uint count);

    VertexArray*        activeVertexArray() const;
    void                setActiveVertexArray(VertexArray* va);

    void                clear();

    void                drawMesh(const experimental::Mesh& mesh);

protected:
    static Context* s_active;

    irect          m_viewport;
    RenderState    m_renderState;
    uint           m_renderStateHash;
    ShaderProgram* m_activeProgram;

    uint                     m_activeTextureUnit;
    std::vector<TextureUnit> m_textureUnits;
    uint                     m_nextFreeTextureUnit;

    Material* m_activeMaterial;

    GBuffer*              m_buffers[GBuffer::NumTypes];
    std::vector<GBuffer*> m_bufferUnits[GBuffer::NumTypes];
    uint                  m_nextFreeBufferUnit;

    VertexArray* m_activeVertexArray;
};

} // ns gfx
} // ns mcr

#include "Context.inl"