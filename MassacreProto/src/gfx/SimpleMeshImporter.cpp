#include "Universe.h"
#include <mcr/gfx/IMeshImporter.h>

#include <SimpleMesh4.h>

namespace mcr {
namespace gfx {

class SimpleMeshImportTask: public IMeshImportTask
{
public:
    using RefCounted::operator new;
    using RefCounted::operator delete;

    MCR_INTERN SimpleMeshImportTask(IFile* file): m_file(file) {}

    MCR_INTERN bool estimate(MeshInfo& infoOut)
    {
        if (m_file->read(m_header) != sizeof(m_header))
            return false;

        if (!m_header.checkSignature() || m_header.version != SIMPLE_MESH_VERSION)
            return false;

        auto attribs = new SimpleMesh::VertexAttribute[m_header.numAttributes];

        m_file->seek(m_header.attributeDataOffset);
        m_file->read(attribs, m_header.numAttributes);

        for (uint i = 0; i < m_header.numAttributes; ++i)
            m_info.vertexFormat.addAttrib(GType(GType::SByte + attribs[i].type), attribs[i].length); // sic!

        delete [] attribs;

        m_info.numVertices = m_header.numVertices;
        m_info.numIndices  = m_header.numIndices;

        infoOut = m_info;
        return true;
    }

    MCR_INTERN bool import(const VertexFormat& bufferFormat, Mesh& mesh)
    {
        // not capable of converting
        if (m_info.vertexFormat.numAttribs() != bufferFormat.numAttribs())
            return false;

        for (uint i = 0; i < bufferFormat.numAttribs(); ++i)
        {
            auto &myAttrib  = m_info.vertexFormat.attrib(i),
                 &refAttrib = bufferFormat.attrib(i);

            if (myAttrib.type != refAttrib.type || myAttrib.length != refAttrib.length)
                return false;
        }

        mesh.primitiveType = PrimitiveType::Triangles;

        std::size_t read = 0;

        auto vertices = mesh.buffer->vertices()->map(
            m_header.vertexSize * mesh.startVertex,
            m_header.vertexSize * mesh.numVertices,
            GBuffer::ReplaceSome);

        auto indices = (uint*) mesh.buffer->indices()->map(
            sizeof(uint) * mesh.startIndex,
            sizeof(uint) * mesh.numIndices,
            GBuffer::ReplaceSome);

        m_file->seek(m_header.vertexDataOffset);
        read += m_file->read(vertices, m_header.vertexSize * m_header.numVertices);

        m_file->seek(m_header.indexDataOffset);
        read += m_file->read(indices, m_header.numIndices);

        mesh.buffer->indices()->unmap();
        mesh.buffer->vertices()->unmap();

        return read == m_header.vertexSize * m_header.numVertices
                     + sizeof(uint) * m_header.numIndices;
    }

private:
    rcptr<IFile> m_file;
    MeshInfo m_info;
    SimpleMesh::Header m_header;
};


class SimpleMeshImporter: public IMeshImporter
{
public:
    using RefCounted::operator new;
    using RefCounted::operator delete;

    MCR_INTERN rcptr<IMeshImportTask> createTask(IFile* file) const
    {
        return new SimpleMeshImportTask(file);
    }
};

rcptr<IMeshImporter> createSimpleMeshImporter()
{
    return new SimpleMeshImporter;
}

} // ns gfx
} // ns mcr
