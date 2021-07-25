// Example usage of srd::core

#include <unordered_map>
#include <fstream>
#include <sstream>
#include <memory>

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

struct Entity
{
    core::math::transform transform;
    core::gfx::mesh &mesh;
    core::gfx::texture &texture;
    core::gfx::shader_instance shader;

    void render(
        core::gfx::camera &camera,
        core::gfx::deferred_renderer &renderer,
        core::gfx::shader_instance &shadowShader,
        const glm::mat4 &lightSpaceMatrix)
    {
        renderer.render(camera, transform, mesh, texture, shader, shadowShader, lightSpaceMatrix);
    }
};

struct Scene
{
    std::vector<Entity> entities;

    void render(
        core::gfx::camera &camera,
        core::gfx::deferred_renderer &renderer,
        core::gfx::shader_instance &shadowShader,
        const glm::mat4 &lightSpaceMatrix)
    {
        for(auto &e : entities) e.render(camera, renderer, shadowShader, lightSpaceMatrix);
    }
};

struct ResourceManager
{
    std::unordered_map<std::string, std::unique_ptr<core::gfx::mesh>> meshes;
    std::unordered_map<std::string, std::unique_ptr<core::gfx::texture>> textures;
    std::unordered_map<std::string, std::unique_ptr<core::gfx::shader>> shaders;
};

int main()
{
    core::window::window win { 1920/2, 1080/2, "" }; // This should be initialized first!!
    core::gfx::deferred_renderer renderer {
        .buffer = core::gfx::gbuffer{1920, 1080},
        .shadowBuffer = core::gfx::sbuffer{1920/2, 1080/2},
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

    // -----------============ Shader Loading ============----------- //

    resourceManager.shaders["lit"].reset(
        new core::gfx::shader{readFile("data/shaders/lit.vertex"), readFile("data/shaders/dlit.fragment")
    });
    {
        auto &shader = resourceManager.shaders["lit"];
        shader->setUniform(shader->uniforms.materialData.tiling, glm::vec2(2, 2));
    }

    resourceManager.shaders["shadow"].reset(
        new core::gfx::shader{readFile("data/shaders/shadow.vertex"), readFile("data/shaders/shadow.fragment")
    });
    {
        auto &shader = resourceManager.shaders["shadow"];
    }

    resourceManager.shaders["screen"].reset(
        new core::gfx::shader{
            readFile("data/shaders/deferred/screen.vertex"),
            readFile("data/shaders/deferred/screen.fragment")
        }
    );

    auto lightValue = glm::vec4(1.f, 1.2f, 0.3f, 1.f);
    
    {
        auto &shader = resourceManager.shaders["screen"];
        shader->setUniform(shader->getUniform("uLighting.directional"), lightValue);
        shader->setUniform(shader->getUniform("uLighting.ambient"), 0.05f);

        shader->setUniform(shader->getUniform("gPosition"), 0);
        shader->setUniform(shader->getUniform("gNormal"), 1);
        shader->setUniform(shader->getUniform("gDiffuse"), 2);
        shader->setUniform(shader->getUniform("gDepthColor"), 3);
        shader->setUniform(shader->getUniform("gShadowDepth"), 4);
        shader->setUniform(shader->getUniform("gPositionLightSpace"), 5);
    }


    // -----------============  Camera Setup  ============----------- //

    core::gfx::camera camera(win.width, win.height, 0.1f, 100.f);
    camera.transform.position = { 0, 1, -5 };
    camera.transform.rotation = glm::identity<glm::quat>();
    camera.transform.scale    = { 1, 1, 1 };
    camera.update();



    // -----------============  World Setup  ============----------- //
    
    Scene scene;
    
    {
        Entity e {
            .mesh = *resourceManager.meshes["sphere"],
            .texture = *resourceManager.textures["wall"],
            .shader = { .type = *resourceManager.shaders["lit"] },
        };

        e.shader.uniforms.materialData.tiling = glm::vec2{ 2, 2 };
        e.transform.position = { 0, 0, 0 };
        e.transform.rotation = glm::identity<glm::quat>();
        e.transform.scale    = { 1, 1, 1 };
        e.transform.update();

        scene.entities.push_back(std::move(e));
    }


    {
        Entity e {
            .mesh = *resourceManager.meshes["sphere"],
            .texture = *resourceManager.textures["wall"],
            .shader = { .type = *resourceManager.shaders["lit"] },
        };

        e.shader.uniforms.materialData.tiling = glm::vec2{ 2, 2 };
        e.transform.position = { 4, -0.4, 6 };
        e.transform.rotation = glm::identity<glm::quat>();
        e.transform.scale    = { 1, 1, 1 };
        e.transform.update();

        scene.entities.push_back(std::move(e));
    }

    {
        Entity e {
            .mesh = *resourceManager.meshes["quad"],
            .texture = *resourceManager.textures["cobblestone"],
            .shader = { .type = *resourceManager.shaders["lit"] },
        };

        e.shader.uniforms.materialData.tiling = glm::vec2{ 25, 25 };

        e.transform.position = { 0, -1, 0 };
        e.transform.rotation = glm::angleAxis(glm::pi<float>() * 0.5f, glm::vec3(1, 0, 0));
        e.transform.scale    = { 10, 10, 10 };
        e.transform.update();

        scene.entities.push_back(std::move(e));
    }

    Entity &myCube = scene.entities[2];

    core::gfx::mesh &quadMesh = *resourceManager.meshes["quad"];
    core::gfx::shader &lightPassShader = *resourceManager.shaders["screen"];
    
    float size = 25.f;
    glm::mat4 lightProjMatrix  = glm::ortho(-size, size, -size, size, -10.f, 40.f);
    glm::mat4 lightViewMatrix  = glm::lookAt(glm::vec3(lightValue), glm::vec3{ 0, 0, 0 }, glm::vec3{ 0, 1, 0 });
    glm::mat4 lightModelMatrix = glm::mat4(1.0f);

    glm::mat4 lightMVPMatrix = lightProjMatrix * lightViewMatrix;
    core::gfx::shader_instance shadowShader { .type = *resourceManager.shaders["shadow"] };

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
        


        /**/ if(win.keyPressed(GLFW_KEY_X))
        {
            myCube.transform.rotation *= glm::angleAxis(rotSpeed * dt, glm::vec3(1, 0, 0));
            myCube.transform.update();
        }
        else if(win.keyPressed(GLFW_KEY_Z))
        {
            myCube.transform.rotation *= glm::angleAxis(-rotSpeed * dt, glm::vec3(1, 0, 0));
            myCube.transform.update();
        }

        /**/ if(win.keyPressed(GLFW_KEY_R))
        {
            myCube.transform.rotation *= glm::angleAxis(rotSpeed * dt, glm::vec3(0, 0, 1));
            myCube.transform.update();
        }
        else if(win.keyPressed(GLFW_KEY_F))
        {
            myCube.transform.rotation *= glm::angleAxis(-rotSpeed * dt, glm::vec3(0, 0, 1));
            myCube.transform.update();
        }


        
    
        if(move != glm::vec3{ 0, 0, 0 } || rotated)
        {
            camera.transform.position += move * camera.transform.rotation;
            return true;
        }
        return false;
    };

    core::window::show(win,
    
    /* On Update */
    [&](float dt)
    {
        if(handleInput(dt)) camera.update();

        renderer.begin();
        scene.render(camera, renderer, shadowShader, lightMVPMatrix);
        renderer.light(lightPassShader, quadMesh);
    },

    /* On Resize */
    [&](int w, int h)
    {
        // camera.resize(w, h);
    },
    
    /* On Keypress */
    [&](int key, int scancode, int action, int mods)
    {
        /**/ if(action == GLFW_PRESS && key == GLFW_KEY_B)
            renderer.debugBuffers = true;
        else if(action == GLFW_RELEASE && key == GLFW_KEY_B)
            renderer.debugBuffers = false;
    });
}
