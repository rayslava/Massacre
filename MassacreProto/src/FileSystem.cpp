#include "Universe.h"
#include "FileSystem.h"

#include <algorithm>
#include <fstream>

using namespace mcr;

//////////////////////////////////////////////////////////////////////////
// path functions

namespace
{
    char toPathChar(char c)
    { return c == '\\' ? '/' : c; }

    bool isSlash(char c)
    { return c == '/' || c == '\\'; }

    bool isDot(char c)
    { return c == '.'; }

    template <typename F>
    const char* lastPartFrom(const char* path, F pred)
    {
        const char* result = path;

        for (; *path; ++path)
            if (pred(*path))
                result = path + 1;

        return result;
    }
}

std::string Path::format(const char* path)
{
    std::string result = path;
    std::transform(result.cbegin(), result.cend(), result.begin(), toPathChar);

    if (result.back() == '/')
        result.pop_back();

    return result;
}

std::string Path::dir(const char* path)
{
    std::string dir = path;

    auto slash = dir.find_last_of("\\/");
    return slash != std::string::npos ? dir.substr(0, slash) : ".";
}

const char* Path::filename(const char* path)
{
    return lastPartFrom(path, isSlash);
}

const char* Path::ext(const char* path)
{
    return lastPartFrom(path, isDot);
}

//////////////////////////////////////////////////////////////////////////
// resource management

bool FileSystem::attachResource(const char* path)
{
    return m_paths.insert(Path::format(path)).second;
}

bool FileSystem::detachResource(const char* path)
{
    return m_paths.erase(Path::format(path)) != 0;
}

//////////////////////////////////////////////////////////////////////////
// standard iostream-based file reader

namespace
{
    class StdStreamFile: public IFile
    {
    public:
        StdStreamFile(FileSystem* fs, const char* filename):
            m_fs(fs),
            m_filename(filename),
            m_stream(filename, std::ios::binary),
            m_size(0u)
        {
            if (m_stream.good())
            {
                m_stream.seekg(0, std::ios::end);
                m_size = m_stream.tellg();
                m_stream.seekg(0, std::ios::beg);
            }
        }

        size_t read(void* buffer, size_t size)
        {
            m_stream.read(static_cast<char*>(buffer), size);
            return (size_t) m_stream.gcount();
        }

        uint64 size() const
        {
            return m_size;
        }

        void seek(uint64 pos)
        {
            m_stream.seekg(pos);
        }

        uint64 tell() const
        {
            return m_stream.tellg();
        }

        const char* filename() const
        {
            return m_filename.c_str();
        }

        FileSystem* fs() const
        {
            return m_fs;
        }

    protected:
        FileSystem* m_fs;
        std::string m_filename;
        mutable std::ifstream m_stream;
        uint64 m_size;
    };
}

//////////////////////////////////////////////////////////////////////////
// file search & access

rcptr<IFile> FileSystem::openFile(const char* filename, std::string* path)
{
    rcptr<StdStreamFile> file = new StdStreamFile(this, filename);

    if (file->size())
    {
        if (path)
            *path = filename;

        return file;
    }

    for (auto it = m_paths.begin(); it != m_paths.end(); ++it)
    {
        auto guess = *it + '/' + filename;

        file = new StdStreamFile(this, guess.c_str());

        if (file->size())
        {
            if (path)
                *path = guess;

            break;
        }
    }

    return file;
}
