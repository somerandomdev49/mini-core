// Example usage of srd::core

#include <unordered_map>
#include <fstream>
#include <sstream>
#include <memory>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
// #include <reactphysics3d/reactphysics3d.h>
// namespace rp3d = reactphysics3d;
#include <RigidBox/RigidBox.h>
// #include <q3.h>

#define SRD_CORE_IMPLEMENTATION
#include "core.hpp"
#undef  SRD_CORE_IMPLEMENTATION

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#undef TINYOBJLOADER_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION

#include "log.hpp"
#include "util.hpp"

using namespace srd;

class Entity
{
public:
    core::math::transform transform;
    core::gfx::mesh &mesh;
    core::gfx::texture &texture;
    core::gfx::shaders::geometry_shader_instance shader;

    Entity(
        core::gfx::mesh &mesh,
        core::gfx::texture &texture,
        core::gfx::shaders::geometry_shader_instance &&shader)
        : mesh(mesh), texture(texture), shader(std::move(shader))
    {
        transform = {
            .position = { 0, 0, 0},
            .rotation = { 1, 0, 0, 0 },
            .scale = { 1, 1, 1 },
        };
        transform.update();
    }

    virtual ~Entity() {}
    virtual void update(float dt) {}
    virtual void beforeUpdate(float dt) {}

    void render(
        core::gfx::camera &camera,
        core::gfx::deferred_renderer &renderer,
        core::gfx::shaders::shadow_shader &shadowShader,
        const glm::mat4 &lightSpaceMatrix)
    {
        renderer.render(camera, {
            transform,
            mesh,
            texture,
            shader,
            shadowShader,
            lightSpaceMatrix
        });
    }
};

class Scene
{
public:
    std::vector<std::unique_ptr<Entity>> entities;

    void beforeUpdate(float dt)
    {
        for(auto &e : entities) e->beforeUpdate(dt);
    }

    void update(float dt)
    {
        for(auto &e : entities) e->update(dt);
    }

    void render(
        core::gfx::camera &camera,
        core::gfx::deferred_renderer &renderer,
        core::gfx::shaders::shadow_shader &shadowShader,
        const glm::mat4 &lightSpaceMatrix)
    {
        for(auto &e : entities) e->render(camera, renderer, shadowShader, lightSpaceMatrix);
    }
};

struct ResourceManager
{
    std::unordered_map<std::string, std::unique_ptr<core::gfx::mesh>> meshes;
    std::unordered_map<std::string, std::unique_ptr<core::gfx::texture>> textures;
    std::unordered_map<std::string, std::unique_ptr<core::gfx::shader>> shaders;
    std::unordered_map<std::string, std::unique_ptr<core::gfx::cubemap>> cubemaps;
};

// struct Box
// {
//     glm::vec3 a;
//     glm::vec3 b;

//     static Box fromSize(glm::vec3 position, glm::vec3 size)
//     {
//         return { .a = position, .b = position + size };
//     }
// };

// bool operator ^(const Box &a, const Box &b)
// {
//     return (a.a.x <= b.b.x && a.b.x >= b.a.x)
//         && (a.a.y <= b.b.y && a.b.y >= b.a.y)
//         && (a.a.z <= b.b.z && a.b.z >= b.a.z);
// }

// struct Collider { Box shape; bool dynamic; int id; };

// class PhysicsWorld
// {
// public:
//     std::vector<Collider> colliders;

//     Collider &add(const Box &box, bool dynamic)
//     {
//         static int nextId = 0;
//         colliders.push_back(Collider { .shape = std::move(box), .dynamic = dynamic, .id = nextId++ });
//         return colliders[colliders.size() - 1];
//     }
// };

glm::vec3 rb2glm(const rbVec3 &v)
{
    return { v.x, v.y, v.z };
}

glm::quat rb2glm(const rbMtx3 &v)
{
    return glm::quat_cast(glm::mat3 { rb2glm(v.r[0]), rb2glm(v.r[1]), rb2glm(v.r[2]) });
}

rbVec3 glm2rb(const glm::vec3 &v)
{
    return { v.x, v.y, v.z };
}

rbMtx3 glm2rb(const glm::mat3 &v)
{
    rbMtx3 m;
    m.r[0] = glm2rb(v[0]);
    m.r[1] = glm2rb(v[1]);
    m.r[2] = glm2rb(v[2]);
    return m;
}

class PhysicsEntity : public Entity
{
    rbEnvironment *env;
    rbRigidBody rb;
    rbVec3 gravity;
public:

    struct PhysicsData
    {
        bool isStatic;
        glm::vec3 size;
        glm::vec3 gravity;
    };

    PhysicsEntity(core::gfx::mesh &mesh,
                  core::gfx::texture &texture,
                  core::gfx::shaders::geometry_shader_instance &&shader,
                  rbEnvironment *env,
                  const PhysicsData &data)

        : Entity(mesh, texture, std::move(shader)), env(env), gravity(glm2rb(data.gravity))
    {
        rb.SetShapeParameter(
            data.size.x * data.size.y * data.size.z,
            data.size.x, data.size.y, data.size.z,
            0.1f, 0.1f);

        if(data.isStatic) rb.EnableAttribute(rbRigidBody::Attribute_Fixed);
        env->Register(&rb);
    }

    ~PhysicsEntity()
    {
        env->Unregister(&rb);
    }
    
    void beforeUpdate(float) override
    {
        rb.SetForce(gravity);
    }

    void update(float) override
    {
        transform.position = rb2glm(rb.Position());
        transform.rotation = rb2glm(rb.Orientation());
        transform.update();
    }
};

struct PlayerController
{
    core::window::window &win;

    void update(float dt)
    {
        //win.keyPressed()
    }
};

int main(int argc, char *argv[])
{
    core::window::window win { 1920/2, 1080/2, "" }; // This should be initialized first!!

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)win.win, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    core::gfx::deferred_renderer renderer {
        .buffer = core::gfx::gbuffer{1920, 1080},
        .shadowBuffer = core::gfx::sbuffer{1920, 1080},
        .width = 1920,
        .height = 1080, // fix this! (needs to be 1080/2 instead of 1080!)
        .debugBuffers = false
    };

    ResourceManager resourceManager;
    
    // -----------============ Mesh Loading ============----------- //

    std::vector<core::gfx::vertex> vertices;
    std::vector<unsigned int> indices;

    {
        readMesh("data/sphere.obj", vertices, indices);
        resourceManager.meshes["sphere"].reset(new core::gfx::mesh{ vertices, indices });
    }

    vertices.clear(); indices.clear();

    {
        readMesh("data/quad.obj", vertices, indices);
        resourceManager.meshes["quad"].reset(new core::gfx::mesh{ vertices, indices });
    }

    vertices.clear(); indices.clear();

    {
        readMesh("data/cube.obj", vertices, indices);
        resourceManager.meshes["cube"].reset(new core::gfx::mesh{ vertices, indices });
    }

    vertices.clear(); indices.clear();

    {
        readMesh("data/Portal2UV_Fixed.obj", vertices, indices);
        resourceManager.meshes["portal"].reset(new core::gfx::mesh{ vertices, indices });
    }


    // -----------============ Texture Loading ============----------- //

    {
        auto data = readTexture("data/wall.jpeg", false);
        resourceManager.textures["wall"].reset(new core::gfx::texture{ data });
        deleteTexture(data);
    }

    {
        auto data = readTexture("data/wall-sandstone.jpeg", false);
        resourceManager.textures["sandstone"].reset(new core::gfx::texture{ data });
        deleteTexture(data);
    }

    {
        auto data = readTexture("data/floor-cobblestone.jpeg", false);
        resourceManager.textures["cobblestone"].reset(new core::gfx::texture{ data });
        deleteTexture(data);
    }

    {
        auto data = readTexture("data/StonePortal2.jpg", false);
        resourceManager.textures["portal"].reset(new core::gfx::texture{ data });
        deleteTexture(data);
    }

    {
        auto xPos = readTexture("data/textures/skybox/skybox_px.jpg", true);
        auto xNeg = readTexture("data/textures/skybox/skybox_nx.jpg", true);
        auto yPos = readTexture("data/textures/skybox/skybox_py.jpg", true);
        auto yNeg = readTexture("data/textures/skybox/skybox_ny.jpg", true);
        auto zPos = readTexture("data/textures/skybox/skybox_pz.jpg", true);
        auto zNeg = readTexture("data/textures/skybox/skybox_nz.jpg", true);
        resourceManager.cubemaps["skybox"].reset(new core::gfx::cubemap
        {
            xPos,
            xNeg,
            yPos,
            yNeg,
            zPos,
            zNeg
        });
        deleteTexture(xPos);
        deleteTexture(xNeg);
        deleteTexture(yPos);
        deleteTexture(yNeg);
        deleteTexture(zPos);
        deleteTexture(zNeg);

    }

    // -----------============ Shader Loading ============----------- //

    log::csec << "Lit Shader" << log::endl;
    resourceManager.shaders["lit"].reset(
        new core::gfx::shaders::geometry_shader{readFile("data/shaders/lit.vertex"), readFile("data/shaders/dlit.fragment")
    });
    {
        auto shader = (core::gfx::shaders::geometry_shader*)(resourceManager.shaders["lit"].get());
        shader->setUniform(shader->uniforms.materialData.tiling, glm::vec2(1, 1));
    }

    log::csec << "Shadow Shader" << log::endl;
    resourceManager.shaders["shadow"].reset(
        new core::gfx::shaders::shadow_shader{readFile("data/shaders/shadow.vertex"), readFile("data/shaders/shadow.fragment")
    });

    log::csec << "Skybox Shader" << log::endl;
    resourceManager.shaders["skybox"].reset(
        new core::gfx::shaders::skybox_shader{readFile("data/shaders/skybox.vertex"), readFile("data/shaders/skybox.fragment")
    });

    log::csec << "Screen Shader" << log::endl;
    resourceManager.shaders["screen"].reset(
        new core::gfx::shaders::screen_shader{
            readFile("data/shaders/deferred/screen.vertex"),
            readFile("data/shaders/deferred/screen.fragment")
        }
    );

    
    // -----------============  Camera Setup  ============----------- //

    core::gfx::camera camera(win.width, win.height, 0.1f, 100.f);
    camera.transform.position = { 0, 1, -5 };
    camera.transform.rotation = glm::identity<glm::quat>();
    camera.transform.scale    = { 1, 1, 1 };
    camera.update();



    // -----------============  World Setup  ============----------- //
    
    rbEnvironment *env = new rbEnvironment(rbEnvironment::Config {
        .RigidBodyCapacity = 20,
        .ContactCapacty = 100,
    });

    const glm::vec3 GRAVITY = { 0.f, -10.f, 0.f };

    Scene scene;
    
    {
        auto e = new PhysicsEntity {
            *resourceManager.meshes["portal"],
            *resourceManager.textures["portal"],
            core::gfx::shaders::geometry_shader_instance {
                .type = *static_cast<core::gfx::shaders::geometry_shader*>(resourceManager.shaders["lit"].get())
            },
            env,
            PhysicsEntity::PhysicsData { .isStatic = true, .size = { 1, 1, 1 }, .gravity = GRAVITY }
        };

        e->shader.uniforms.materialData.tiling = glm::vec2{ 1, 1 };
        e->transform.position = { 0, -1.f, 0 };
        e->transform.rotation = glm::angleAxis(glm::radians(180.f), glm::vec3(0, 1, 0));
        e->transform.scale    = { 1.5f, 1.5f, 1.5f };
        e->transform.update();

        scene.entities.push_back(std::unique_ptr<Entity>(e));
    }


    {
        auto e = new PhysicsEntity {
            *resourceManager.meshes["sphere"],
            *resourceManager.textures["wall"],
            core::gfx::shaders::geometry_shader_instance {
                .type = *static_cast<core::gfx::shaders::geometry_shader*>(resourceManager.shaders["lit"].get())
            },
            env,
            PhysicsEntity::PhysicsData { .isStatic = true, .size = { 1, 1, 1 }, .gravity = GRAVITY }
        };

        e->shader.uniforms.materialData.tiling = glm::vec2{ 2, 2 };
        e->transform.position = { 4, -0.4, 6 };
        e->transform.rotation = glm::identity<glm::quat>();
        e->transform.scale    = { 1, 1, 1 };
        e->transform.update();

        scene.entities.push_back(std::unique_ptr<Entity>(e));
    }

    {
        auto e = new PhysicsEntity {
            *resourceManager.meshes["quad"],
            *resourceManager.textures["cobblestone"],
            core::gfx::shaders::geometry_shader_instance {
                .type = *static_cast<core::gfx::shaders::geometry_shader*>(resourceManager.shaders["lit"].get())
            },
            env,
            PhysicsEntity::PhysicsData { .isStatic = true, .size = { 1, 1, 1 }, .gravity = GRAVITY }
        };

        e->shader.uniforms.materialData.tiling = glm::vec2{ 25, 25 };
        e->transform.position = { 0, -1, 0 };
        e->transform.rotation = glm::angleAxis(glm::pi<float>() * 0.5f, glm::vec3(1, 0, 0));
        e->transform.scale    = { 10, 10, 10 };
        e->transform.update();

        scene.entities.push_back(std::unique_ptr<Entity>(e));
    }

    auto &myCube = *scene.entities[2].get();

    core::gfx::skybox skybox {
        .texture = *resourceManager.cubemaps["skybox"],
        .skyMesh = *resourceManager.meshes["cube"],
    };

    skybox.update(camera.transform.position);

    // auto &skyboxShader = *resourceManager.shaders["skybox"];

    core::gfx::shaders::skybox_shader_instance skyboxShader {
        .type = *static_cast<core::gfx::shaders::skybox_shader*>(resourceManager.shaders["skybox"].get())
    };

    
    core::gfx::mesh &quadMesh = *resourceManager.meshes["quad"];
    core::gfx::shaders::screen_shader_instance lightPassShader {
        .type = *static_cast<core::gfx::shaders::screen_shader*>(resourceManager.shaders["screen"].get())
    };

    auto lightValue = glm::vec4(1.f, 1.f, 0.3f, 1.4f);
    lightPassShader.uniforms.lighting.directional = lightValue;
    
    float size = 25.f;
    glm::mat4 lightProjMatrix;
    glm::mat4 lightViewMatrix;
    glm::mat4 lightMVPMatrix;
    const auto updateLightMatrix = [&]()
    {
        lightProjMatrix = glm::ortho(-size, size, -size, size, -10.f, 40.f);
        lightViewMatrix = glm::lookAt(glm::vec3(lightValue), glm::vec3{ 0, 0, 0 }, glm::vec3{ 0, 1, 0 });
        lightMVPMatrix = lightProjMatrix * lightViewMatrix;
    };

    updateLightMatrix();

    auto &shadowShader = *static_cast<core::gfx::shaders::shadow_shader*>(resourceManager.shaders["shadow"].get());

    // -----------============   Game Loop   ============----------- //

    const auto handleInput = [&](float dt) -> bool
    {
        static float speed = 3.f;
        static float rotSpeed = 1.f;
        bool havent_changed;
        glm::vec3 move { 0, 0, 0 };

        /**/ if(win.keyPressed(GLFW_KEY_A))
            move.x -= speed * dt;
        else if(win.keyPressed(GLFW_KEY_D))
            move.x += speed * dt;

        /**/ if(win.keyPressed(GLFW_KEY_LEFT_SHIFT))
            move.y -= speed * dt;
        else if(win.keyPressed(GLFW_KEY_SPACE))
            move.y += speed * dt;

        /**/ if(win.keyPressed(GLFW_KEY_S))
            move.z -= speed * dt;
        else if(win.keyPressed(GLFW_KEY_W))
            move.z += speed * dt;

        bool rotated = false;
        /**/ if(win.keyPressed(GLFW_KEY_Q))
        {
            camera.transform.rotation *= glm::angleAxis(rotSpeed * dt, glm::vec3(0, 1, 0));
            rotated = true;
        }
        else if(win.keyPressed(GLFW_KEY_E))
        {
            camera.transform.rotation *= glm::angleAxis(-rotSpeed * dt, glm::vec3(0, 1, 0));
            rotated = true;
        }
    
        if(move != glm::vec3{ 0, 0, 0 } || rotated)
        {
            camera.transform.position += move * camera.transform.rotation;
            return true;
        }
        return false;
    };

    constexpr float maxSunPos = 20;

    const auto drawGui = [&]()
    {
        ImGui::Begin("Debug Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Checkbox("Shadow Buffer", &lightPassShader.uniforms.debug.showShadowMap);
        float sunX = lightValue.x;
        if(ImGui::SliderFloat("Sun X", &sunX, -maxSunPos, maxSunPos))
        {
            lightValue.x = sunX;
            updateLightMatrix();
        }

        float sunY = lightValue.y;
        if(ImGui::SliderFloat("Sun Y", &sunY, -maxSunPos, maxSunPos))
        {
            lightValue.y = sunY;
            updateLightMatrix();
        }

        float sunZ = lightValue.z;
        if(ImGui::SliderFloat("Sun Z", &sunZ, -maxSunPos, maxSunPos))
        {
            lightValue.z = sunZ;
            updateLightMatrix();
        }

        float sunI = lightValue.w;
        if(ImGui::SliderFloat("Sun I", &sunI, -maxSunPos, maxSunPos))
        {
            lightValue.w = sunI;
            lightPassShader.uniforms.lighting.directional = lightValue;
        }

        ImGui::End();
    };

    core::window::show(win,
    
    /* On Update */
    [&](float dt)
    {
        if(!io.WantCaptureKeyboard) if(handleInput(dt)) camera.update();
        // scene.beforeUpdate(dt);
        // env->Update(dt, 3);
        // scene.update(dt);

        renderer.begin();
        scene.render(camera, renderer, shadowShader, lightMVPMatrix);
        renderer.sky(camera, skybox, skyboxShader);
        renderer.light(lightPassShader, quadMesh);
        drawGui();
    },

    /* -Disabled- On Resize */
    [&](int w, int h) { }, false,

    /* -Disabled- On Keypress */
    [&](int key, int scancode, int action, int mods) {}, false,
    
    /* On Before Render */
    [&]()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    },
    
    /* On After Render */
    [&]()
    {   
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    });

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(nullptr);
}
