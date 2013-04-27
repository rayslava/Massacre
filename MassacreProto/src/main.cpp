#include "Universe.h"

#define GLFW_NO_GLU
#include <GL/glfw.h>

#include <mcr/Config.h>
#include <mcr/Timer.h>

#include <mcr/Camera.h>
#include <mcr/gfx/Context.h>
#include <mcr/gfx/renderables/Mesh.h>
#include <mcr/gfx/renderables/SpriteBatch.h>
#include <mcr/gfx/MaterialManager.h>

#include <mcr/gfx/experimental/IMeshImporter.h>

using namespace mcr;

class Game;

// GameLayer, stub
class GameLayer
{
public:
    GameLayer(Game* game): m_game(game) {}
    virtual ~GameLayer() {}

    virtual void onLoad() = 0;
    virtual void onUnload() = 0;

    virtual void onActivate() = 0;
    virtual void onDeactivate() = 0;

    virtual bool onRun() = 0;
    virtual void onResize(const ivec2& newSize) = 0;

protected:
    Game* m_game;
};

// Game
class Game
{
public:
    Game();
    ~Game();

    void initGL();
    void run();

protected:
    GameLayer* m_layer;

private:
    static void onWindowSize(int w, int h);

    static bool s_windowResized;
    static ivec2 s_newWindowSize;
};

class MeshManager
{
public:
    MeshManager(FileSystem* fs): m_fs(fs)
    {
        m_buffer = gfx::VertexArray::create();
        m_loader = gfx::experimental::createSimpleMeshLoader();
    }

    bool load(const char* filename, ...)
    {
        using namespace gfx::experimental;

        std::vector<rcptr<IMeshImportTask>> tasks;

        for (auto arg = &filename; *arg; ++arg)
            tasks.push_back(m_loader->createTask(m_fs->openFile(*arg)));

        IMeshImportTask::MeshInfo total;
        total.numVertices = total.numIndices = 0;

        bool fail = false;

        BOOST_FOREACH (IMeshImportTask* task, tasks)
        {
            IMeshImportTask::MeshInfo info;

            if (!task->estimate(info))
            {
                fail = true;
                break;
            }

            rcptr<Mesh> mesh = new Mesh;

            mesh->buffer = m_buffer;
            mesh->startVertex = total.numVertices;
            mesh->numVertices = info.numVertices;
            mesh->startIndex = total.numIndices;
            mesh->numIndices = info.numIndices;
            mesh->primitiveType = GL_TRIANGLES;

            m_meshes.push_back(mesh);

            total.numVertices += info.numVertices;
            total.numVertices += info.numVertices;

            if (total.vertexFormat.numAttribs() == 0)
                total.vertexFormat = info.vertexFormat;
        }

        if (!fail)
        {
            m_buffer->setFormat(total.vertexFormat);
            m_buffer->vertices()->init(total.numVertices * total.vertexFormat.stride(), gfx::GBuffer::StaticDraw);
            m_buffer->indices()->init(total.numIndices * sizeof(uint), gfx::GBuffer::StaticDraw);

            for (uint i = 0; i < tasks.size(); ++i)
            {
                if (!tasks[i]->import(total.vertexFormat, *m_meshes[i]))
                {
                    fail = true;
                    break;
                }
            }
        }

        if (fail)
            m_meshes.clear();

        return !fail;
    }

private:
    FileSystem* m_fs;
    rcptr<gfx::VertexArray> m_buffer;
    rcptr<gfx::experimental::IMeshImporter> m_loader;
    std::vector<rcptr<gfx::experimental::Mesh>> m_meshes;
};

//////////////////////////////////////////////////////////////////////////
// Game layer stuff

class BattleScreen: public GameLayer
{
public:
    BattleScreen(Game* game): GameLayer(game ){}

    void onLoad()
    {
        m_camera.setZRange(vec2(1.f, 3000.f));
        m_camera.update();

#if defined(MCR_PLATFORM_WINDOWS)

        if (!m_mm.fs()->attachResource("DataArena/"))
        {
            debug("Can't find data directory");
            exit(1);
        }

#elif defined(MCR_PLATFORM_LINUX)

        // We should also check system wide data repository for linux
        if (!m_mm.fs()->attachResource("DataArena/")
        &&  !m_mm.fs()->attachResource("/usr/share/massacre/"))
        {
            debug("Can't find data directory");
            exit(1);
        }

#endif
        m_config.load(m_mm.fs()->openFile("mainconf.yaml"));

        m_turnSpeed = m_config["turn_speed"] || 60.f;
        m_velocity  = m_config["velocity"]   || 100.f;
        m_reach     = m_config["reach"]      || 780.f;

        loadArena();
        loadSky();

        m_quasicrystal = m_mm.getMaterial("quasicrystal.mtl");

        m_gateBatch = gfx::SpriteBatch::create(2);

        m_gateBatch->addSprite(rect(-.5f, 0, .5f, 1), rect(1, 2),
            math::buildTransform(vec3(957, -1.f, 5), vec3(0, 90, 0), vec3(125, 250, 1)));

        m_gateBatch->addSprite(rect(-.5f, 0, .5f, 1), rect(1, 2),
            math::buildTransform(vec3(-837, -1.f, 22), vec3(0, -90, 0), vec3(125, 250, 1)));

        m_gateBatch->applyChanges();
    }

    void loadArena()
    {
        m_opaque      = m_mm.getMaterial("Models/Arena/arena.mtl");
        m_transparent = m_mm.getMaterial("Models/Arena/branch.mtl");
        m_translucent = m_mm.getMaterial("Models/Arena/leaves.mtl");

        auto opaqueAtlas      = m_mm.parseAtlasTMP("Models/Arena/arena.json");
        auto transparentAtlas = m_mm.parseAtlasTMP("Models/Arena/trans.json");

        MeshManager mgr(m_mm.fs());
        //mgr.load("foo", "bar", "baz", nullptr);

        auto mesh     = gfx::Mesh::createFromFile(m_mm.fs()->openFile("Models/Arena/arena.mesh"));
        m_arenaBuffer = const_cast<gfx::VertexArray*>(mesh->buffer()); // hack

        // move
        m_arenaBuffer->transformAttribs(0, math::buildTransform(vec3(-600, 0, 100)));

        // flip by v
        m_arenaBuffer->transformAttribs(2, math::buildTransform(vec3(0, 1, 0), vec3(), vec3(1, -1, 0)));

        for (uint i = 0; i < mesh->numAtoms(); ++i)
        {
            auto        atom = const_cast<gfx::RenderAtom*>(mesh->atom(i));
            std::string tex  = Path::filename(atom->material->texture(0)->filename().c_str());

            bool transformBit = false;

            auto it = opaqueAtlas.find(tex);
            if (it != opaqueAtlas.end())
            {
                transformBit = true;
                atom->material = m_opaque;
            }
            else
            {
                it = transparentAtlas.find(tex);
                if (it != transparentAtlas.end())
                {
                    transformBit = true;

                    if (tex.substr(0, 6) == "leaves")
                        atom->material = m_translucent;
                    else
                        atom->material = m_transparent;
                }
            }

            if (transformBit)
            {
                auto tf = math::buildTransform(
                    vec3(it->second.botLeft(), 0.f),
                    vec3(),
                    vec3(it->second.size(), 0.f));

                m_arenaBuffer->transformAttribsIndexed(2, tf, atom->start, atom->size);
                continue;
            }

            atom->material->initProgram(
                m_opaque->program()->shader(0),
                m_opaque->program()->shader(1));
        }

        mesh->optimizeAtomsTMP();

        for (uint i = 0; i < mesh->numAtoms(); ++i)
        {
            auto atom = mesh->atom(i);

            if (atom->material == m_opaque)
                m_opaqueAtoms.push_back(atom);

            else if (atom->material == m_transparent)
                m_transparentAtoms.push_back(atom);

            else if (atom->material == m_translucent)
                m_translucentAtoms.push_back(atom);

            else
            {
                m_other.push_back(atom->material);
                m_otherAtoms.push_back(atom);
            }

        }

        m_meshes.insert(mesh);
    }

    void loadSky()
    {
        auto vert = m_mm.getShader("Shaders/sky_t0.vert");
        auto frag = m_mm.getShader("Shaders/diffuse.frag");

        auto mesh   = gfx::Mesh::createFromFile(m_mm.fs()->openFile("Models/Sky/sky.mesh"));
        m_skyBuffer = const_cast<gfx::VertexArray*>(mesh->buffer()); // hack

        // flip by v
        m_skyBuffer->transformAttribs(2, math::buildTransform(vec3(0, 1, 0), vec3(), vec3(1, -1, 0)));

        for (uint i = 0; i < mesh->numAtoms(); ++i)
        {
            gfx::Material* mtl = mesh->atom(i)->material;
            
            mtl->initProgram(vert, frag);

            mtl->set(&gfx::RenderState::depthTest, false);
            mtl->setPassHint(-1);

            m_sky.push_back(mtl);
            m_skyAtoms.push_back(mesh->atom(i));
        }

        m_meshes.insert(mesh);
    }

    void onUnload() {}


    void onActivate()
    {
        m_pos.setY(20);
    }

    void onDeactivate() {}


    bool onRun()
    {
        m_timer.refresh();

        handleKeys();
        render();

        return handleEvents();
    }

    void onResize(const ivec2& size)
    {
        gfx::Context::active().setViewport(size);
        m_camera.setAspectRatio((float) size.x() / size.y());
        m_camera.update();
    }

    void renderAtom(const gfx::RenderAtom* atom)
    {
        glDrawElements(GL_TRIANGLES, atom->size, GL_UNSIGNED_INT,
            reinterpret_cast<const GLvoid*>(sizeof(uint) * atom->start));
    }

    void renderAtoms(const std::vector<const gfx::RenderAtom*>& atoms)
    {
        for (auto i = 0u; i < atoms.size(); ++i)
            renderAtom(atoms[i]);
    }

    void render()
    {
        auto& ctx = gfx::Context::active();

        ctx.clear();

        m_camera.dumpMatrices();

        ctx.setActiveVertexArray(m_skyBuffer);
        for (auto i = 0u; i < m_sky.size(); ++i)
        {
            ctx.setActiveMaterial(m_sky[i]);
            renderAtom(m_skyAtoms[i]);
        }

        ctx.setActiveVertexArray(m_arenaBuffer);

        ctx.setActiveMaterial(m_opaque);
        renderAtoms(m_opaqueAtoms);

        ctx.setActiveMaterial(m_transparent);
        renderAtoms(m_transparentAtoms);

        ctx.setActiveMaterial(m_translucent);
        renderAtoms(m_translucentAtoms);

        for (auto i = 0u; i < m_other.size(); ++i)
        {
            ctx.setActiveMaterial(m_other[i]);
            renderAtom(m_otherAtoms[i]);
        }

        ctx.setActiveVertexArray(const_cast<gfx::VertexArray*>(m_gateBatch->buffer()));
        ctx.setActiveMaterial(m_quasicrystal);
        renderAtom(m_gateBatch->atom(0));

        glfwSwapBuffers();
    }

    void handleKeys()
    {
        auto mv = math::normalize(vec3(
                0.f + glfwGetKey('E') - glfwGetKey('Q'), 0,
                0.f + glfwGetKey('S') - glfwGetKey('W')));

        auto yAngle = math::deg2rad * m_rot.y();
        auto ySine = sin(yAngle), yCosine = cos(yAngle);

        auto dir = mv.x() * vec3(yCosine, 0, -ySine) + mv.z() * vec3(ySine, 0, yCosine);
        auto velocity = m_velocity;


        if (!math::equals(m_reach, 0.f))
        {
            auto bias     = math::normalize(vec2(m_pos.x(), m_pos.z()));
            auto affinity = .7071068f * math::length(bias + vec2(dir.x(), dir.z()));
            auto tensity  = math::length(vec2(m_pos.x(), m_pos.z())) / m_reach;

            velocity *= std::max(0.f, 1.f - affinity * tensity);
        }

        m_pos += (float) m_timer.dseconds() * velocity * dir;

        auto turnStep = (float) m_timer.dseconds() * -m_turnSpeed;
        m_rot += vec3(0, turnStep * (glfwGetKey('D') - glfwGetKey('A')), 0);


        static const vec3 camOffset(0, 35, 0);

        auto tf = math::buildTransform(vec3(), m_rot);

        m_camera.setViewMatrix(tf, true);
        m_camera.setPosition(m_pos + camOffset * tf);
    }

    // [Esc] and window resize
    bool handleEvents()
    {
        if (glfwGetKey(GLFW_KEY_ESC))
            glfwCloseWindow();

        return !!glfwGetWindowParam(GLFW_OPENED);
    }

private:
    Config m_config;
    Timer m_timer;

    Camera m_camera;
    gfx::MaterialManager m_mm;

    std::set<rcptr<gfx::Mesh>> m_meshes;

    rcptr<gfx::Material> m_opaque, m_transparent, m_translucent, m_quasicrystal;
    std::vector<rcptr<gfx::Material>> m_other, m_sky;

    rcptr<gfx::VertexArray> m_arenaBuffer, m_skyBuffer;
    rcptr<gfx::SpriteBatch> m_gateBatch;

    std::vector<const gfx::RenderAtom*>
        m_opaqueAtoms,
        m_transparentAtoms,
        m_translucentAtoms,
        m_otherAtoms,
        m_skyAtoms;

    vec3 m_pos, m_rot;
    uint m_moveFlags;
    float m_turnSpeed, m_velocity, m_reach;
};


//////////////////////////////////////////////////////////////////////////
// Game stuff

bool Game::s_windowResized = false;
ivec2 Game::s_newWindowSize;

Game::Game()
{
    g_enableLoggingToFile = true;

    initGL();

    m_layer = new BattleScreen(this);
}

Game::~Game()
{
    delete m_layer;
}


void Game::initGL()
{
    if (!glfwInit())
        debug("glfwInit failed");

    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 1);
    glfwOpenWindowHint(GLFW_FSAA_SAMPLES, 3);

    glfwOpenWindow(1024, 768, 8, 8, 8, 8, 24, 8, GLFW_WINDOW);
    glfwSetWindowTitle("MassacreProto");

    glfwSetWindowSizeCallback(&Game::onWindowSize);

    GLFWvidmode desktop;
    glfwGetDesktopMode(&desktop);
    glfwSetWindowPos((desktop.Width - 1024) / 2, (desktop.Height - 768) / 2 - 30); // slightly above the center
    
    auto glewErr = glewInit();
    if (glewErr != GLEW_OK)
        debug("glewInit failed: %s", glewGetErrorString(glewErr));

    new gfx::Context;
    debug("OpenGL %s", glGetString(GL_VERSION));

    GLint numExts;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExts);

    debug("Extensions (%d):", numExts);

    for (int i = 0; i < numExts; ++i)
        debug("\t%s", glGetStringi(GL_EXTENSIONS, i));

    //glClearColor(.7f, .7f, .7f, 1);
    //glClearColor(.2f, .2f, .2f, 1);
    glClearColor(0, 0, 0, 1);
    //glPolygonOffset(-1.f, 5.f);

    const char* reqexts[] =
    {
        "GL_ARB_multisample",
        "GL_ARB_vertex_buffer_object",
        "GL_ARB_vertex_array_object",
        "GL_ARB_map_buffer_range",
        "GL_ARB_uniform_buffer_object",
        "GL_ARB_explicit_attrib_location"
    };

    BOOST_FOREACH (auto ext, reqexts)
        if (!glewIsSupported(ext))
            debug("%s: MISSING", ext);

    const struct
    {
        GLenum name;
        const char* string;
    }
    valuesOfInterest[] =
    {
        {GL_SAMPLES,                          "Samples"         },
        {GL_MAX_VERTEX_ATTRIBS,               "Attributes"      },
        {GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, "Texture units"   },
        {GL_MAX_UNIFORM_BUFFER_BINDINGS,      "Max UBO bindings"},
        {GL_MAX_UNIFORM_BLOCK_SIZE,           "Max UBO size"    },
        {GL_MAX_VERTEX_UNIFORM_BLOCKS,        "Max UBOs, vert"  },
        {GL_MAX_GEOMETRY_UNIFORM_BLOCKS,      "Max UBOs, geom"  },
        {GL_MAX_FRAGMENT_UNIFORM_BLOCKS,      "Max UBOs, frag"  }
    };

    BOOST_FOREACH (auto& voi, valuesOfInterest)
    {
        GLint val;
        glGetIntegerv(voi.name, &val);
        debug("%s: %d", voi.string, val);
    }
}

void Game::run()
{
    m_layer->onLoad();
    m_layer->onActivate();

    const int64 fpsDisplayInterval = 500; // num frames between updates
    Timer timer;

    for (int64 frames = 0;; ++frames)
    {
        if (!m_layer->onRun())
            break;

        if (s_windowResized)
        {
            m_layer->onResize(s_newWindowSize);
            s_windowResized = false;
        }

        if (frames == fpsDisplayInterval)
        {
            frames = 0;

            timer.refresh();
            double fps = fpsDisplayInterval / timer.dseconds();

            char caption[32];
            sprintf(caption, "%.2f FPS", fps);
            glfwSetWindowTitle(caption);
        }
    }
}

void Game::onWindowSize(int w, int h)
{
    s_windowResized = true;
    s_newWindowSize.set(w, h);
}

// 8008135
//////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    Game game;
    game.run();

    return 0;
}

#ifdef MCR_PLATFORM_WINDOWS
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    std::string commandLine = GetCommandLineA();

    std::vector<std::string> arguments;
    boost::split(arguments, commandLine, boost::is_space());

    std::vector<char*> argv;
    BOOST_FOREACH (auto& arg, arguments)
    {
        boost::trim_if(arg, boost::is_any_of("\""));

        if (arg.size())
            argv.push_back(&arg[0]);
    }

    main((int) argv.size(), &argv[0]);
}
#endif
