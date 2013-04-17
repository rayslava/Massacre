#pragma once

#include <gfx/experimental/IMeshExporter.h>

namespace mcr          {
namespace gfx          {
namespace experimental {

class SimpleMeshExporter: public IMeshExporter
{
public:
    bool export_(const Mesh& mesh, const char* filename) const;
};

} // ns experimental
} // ns gfx
} // ns mcr