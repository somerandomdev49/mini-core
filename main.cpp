// Example usage of srd::core
// NB: This is bad code, I tried to make it somewhat good, but it is definetly not a good way to make a game.

// #define GLM_FORCE_LEFT_HANDED

// Standard Library
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <memory>
#include <thread>
#include <queue>
#include <chrono>

// 3rd-Party Header-Only Libraries
#include <inipp.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <RigidBox/RigidBox.h>
#include <reactphysics3d/reactphysics3d.h>

// Mini-Core
#define SRD_CORE_IMPLEMENTATION
#include "core.hpp"
#undef  SRD_CORE_IMPLEMENTATION

// Tiny .Obj Loader - Loading .Obj Files.
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#undef TINYOBJLOADER_IMPLEMENTATION

// STB Image - Loading images.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION

// Logging Library
#include "log.hpp"
// Utilities (loadTexture, loadMesh, ...)
#include "util.hpp"
// Convert values from physics libs.
#include "physics.hpp"

// Less characters.
using namespace srd;
namespace rp3d = reactphysics3d;

/** Converts a 2d vector to a string (format is "[x] [y]") */
std::string vectorToString(const glm::vec2 &v2)
{ return std::to_string(v2.x) + " " + std::to_string(v2.y); }

/** Converts a 3d vector to a string (format is "[x] [y] [z]") */
std::string vectorToString(const glm::vec3 &v3)
{ return std::to_string(v3.x) + " " + std::to_string(v3.y) + " " + std::to_string(v3.z); }

/** Converts a quaternion to a string (format is "[x] [y] [z] [w]") */
std::string vectorToString(const glm::quat &v4)
{ return std::to_string(v4.x) + " " + std::to_string(v4.y) + " " + std::to_string(v4.z)
                                                           + " " + std::to_string(v4.w); }

/** Converts a string to a quaternion. */
auto stringToQuat(const std::string &value) -> glm::quat
{
    float x, y, z, w;
    std::cout << "stringToQuat('" << value << "')" << std::endl;
    std::istringstream iss(value);
    iss >> x >> y >> z >> w;
    return glm::quat(w, x, y, z);
}

/** Converts a string to a 3d vector. */
auto stringToVec3(const std::string &value) -> glm::vec3
{
    float x, y, z;
    std::istringstream iss(value);
    iss >> x >> y >> z;
    return glm::vec3(x, y, z);
}

/** Converts a string to a 3d vector. */
auto stringToVec2(const std::string &value) -> glm::vec2
{
    float x, y;
    std::istringstream iss(value);
    iss >> x >> y;
    return glm::vec2(x, y);
}

/** Converts a string to a float. */
auto stringToFloat(const std::string &value) -> float
{
    return std::stof(value);
}

/** Converts a string to an int. */
auto stringToInt(const std::string &value) -> int
{
    return std::stoi(value);
}

/** Creates a quaternion based on euler angles (order of application is Z, Y, X) */
glm::quat rotateZYX(float x, float y, float z)
{
    float sx = glm::sin(x/2); float sy = glm::sin(y/2); float  sz = glm::sin(z/2);
    float cx = glm::cos(x/2); float cy = glm::cos(y/2); float  cz = glm::cos(z/2);
    return glm::quat(cx*cy*cz - sx*sy*sz,
                     sx*cy*cz + cx*sy*sz,
                     cx*sy*cz - sx*cy*sz,
                     cx*cy*sz + sx*sy*cz);
}

// https://stackoverflow.com/a/2333816/9110517
/** Returns a value from an associative container if found, a default value specified otherwise. */
template <template<class,class,class...> class C, typename K, typename V, typename... Args>
V getOrDefault(const C<K,V,Args...>& m, K const& key, const V & defval)
{
    typename C<K,V,Args...>::const_iterator it = m.find( key );
    if (it == m.end())
        return defval;
    return it->second;
}

/** Value information for configuration. NOTE: Basically anything a value in a config file can be.*/
struct ValueInfo
{
    enum Type
    {
        NONE, VEC3, VEC2, QUAT, STRING, FLOAT, INT
    } type;

    ValueInfo() : type(NONE) { }

    union
    {
        glm::quat    valQuat;
        glm::vec3    valVec3;
        glm::vec2    valVec2;
        std::string *valString;
        float        valFloat;
        int          valInt;
    };

    ~ValueInfo()
    {
        log::cerr << "~ValueInfo()!" << log::endl;
        if(type == STRING) delete valString;
    }
};

/** Container for all resources. */
struct ResourceManager
{
    std::unordered_map<std::string, std::unique_ptr<core::gfx::mesh>> meshes;
    std::unordered_map<std::string, std::unique_ptr<core::gfx::texture>> textures;
    std::unordered_map<std::string, std::unique_ptr<core::gfx::shader>> shaders;
    std::unordered_map<std::string, std::unique_ptr<core::gfx::cubemap>> cubemaps;
};

/** This class is a holder for all resources that need to be quicky accessible. */
class ResourceGlobals
{
public:
    static inline core::gfx::shaders::shadow_shader *shadowShader = nullptr;
    static inline glm::mat4 *lightSpaceMatrix = nullptr;

    // static inline rbVec3 GRAVITY = {0, -1.0f, 0};
    // static inline rbEnvironment *physicsEnv = nullptr;

    static inline rp3d::PhysicsCommon *physicsCommon;
    static inline rp3d::PhysicsWorld *physicsWorld;
    static inline core::window::window *window;
};

// Forward-Decl.
class Entity;

/** Built-In Components... TODO: Somehow store a component's type... */
enum class EntityComponentType
{
    RigidBody,
    StaticMesh
};

/** Represents a component of an entity. */
class EntityComponent
{
public:
    EntityComponentType type;
    Entity *entity;

    virtual ~EntityComponent()
    {
        log::cerr << "~EntityComponent()!" << log::endl;
    }

    /** Called when added to the entity. */
    virtual void load(
        std::unordered_map<std::string, ValueInfo> &info,
        ResourceManager &resourceManager) {} /** FIXME: @note NOTE: */
    
    /** Called before the first update/render cycle. NOTE: All components are accessble. */
    virtual void start() {}

    /** Called before physics world update */
    virtual void update(float dt) {}

    /** Called after physics world update. */
    virtual void afterUpdate(float dt) {}

    /** Called when the scene is rendered. */
    virtual void render(core::gfx::camera &camera, core::gfx::deferred_renderer &renderer) {}
};

/** Represents an entity in a world. */
class Entity
{
public:
    core::math::transform transform;
    std::vector<EntityComponent*> components;
    size_t componentCount = 0;

    Entity()
    {
        transform = {
            .position = { 0, 0, 0 },
            .rotation = { 1, 0, 0, 0 },
            .scale    = { 1, 1, 1 },
        };
        transform.update();
    }

    virtual ~Entity()
    {
        log::cerr << "~Entity()!" << log::endl;
        for(auto &c : components) delete c;
    }

    EntityComponent *findComponentByType(EntityComponentType type)
    {
        // TODO: Fix this sh*t...
        for(size_t i = 0; i < componentCount; ++i)
            if(components[i]->type == type) return components[i];
        return nullptr;
    }

    virtual void update(float dt)
    {
        for(size_t i = 0; i < componentCount; ++i)
            components[i]->update(dt);
    }

    virtual void afterUpdate(float dt)
    {
        for(size_t i = 0; i < componentCount; ++i)
            components[i]->afterUpdate(dt);
    }

    virtual void start()
    {
        for(size_t i = 0; i < componentCount; ++i)
            components[i]->start();
    }

    void add(EntityComponent *component)
    {
        log::cout << "Entity::add(" << component << ")" << log::endl;
        if(!component) log::cerr << "Entity::add(nullptr)!" << log::endl;
        log::cout << "component->entity (" << component->entity << ") = " << this << log::endl;
        component->entity = this;
        components.push_back(component);
        ++componentCount;
        log::cout << "Entity: Added component! Component Count: " << componentCount << log::endl;
    }

    void render(
        core::gfx::camera &camera,
        core::gfx::deferred_renderer &renderer)
    {
        for(size_t i = 0; i < componentCount; ++i)
            components[i]->render(camera, renderer);
    }
};



#define EC_STATIC_CREATE(NAME) \
    static EntityComponent *create(Entity *e, ResourceManager &rm, std::unordered_map<std::string, ValueInfo> &info) \
    { \
        auto c = new NAME(); \
        c->entity = e; \
        c->load(info, rm); \
        return c; \
    }

class ECStaticMesh : EntityComponent
{
public:
    core::gfx::mesh *mesh;
    core::gfx::texture *texture;
    core::gfx::shaders::geometry_shader_instance *shader;

    ECStaticMesh() { type = EntityComponentType::StaticMesh; }
    ~ECStaticMesh()
    {
        log::cerr << "~ECStaticMesh()!" << log::endl;
        delete shader;
    }

    virtual void load(
        std::unordered_map<std::string, ValueInfo> &info,
        ResourceManager &resourceManager) override
    {
        mesh = resourceManager.meshes[*info["staticmesh.mesh"].valString].get();
        texture = resourceManager.textures[*info["staticmesh.texture"].valString].get();
        shader = new core::gfx::shaders::geometry_shader_instance {
            .type = *static_cast<core::gfx::shaders::geometry_shader*>(resourceManager.shaders["lit"].get())
        };
        shader->uniforms.materialData.tiling = info["staticmesh.texture.tiling"].valVec2;
    }

    virtual void render(core::gfx::camera &camera, core::gfx::deferred_renderer &renderer) override
    {
        // puts("StaticMesh: render()");
        renderer.render(camera, {
            entity->transform,
            *mesh,
            *texture,
            *shader,
            *ResourceGlobals::shadowShader,
            *ResourceGlobals::lightSpaceMatrix
        });
    }

    EC_STATIC_CREATE(ECStaticMesh);
};


/** Represent an entity that physics is applied to.  */
class ECRigidBody : public EntityComponent
{
    rp3d::PhysicsWorld *world;
    rp3d::RigidBody *rb;

    glm::vec3 offset;
public:
    ECRigidBody() { type = EntityComponentType::RigidBody; }

    /** Called when added to the entity. */
    virtual void load(
        std::unordered_map<std::string, ValueInfo> &info,
        ResourceManager &resourceManager)
    {
        log::cout << "ECRigidBody::load(); entity = " << entity << log::endl; // entity is NULL!!!
        world = ResourceGlobals::physicsWorld;
        auto common = ResourceGlobals::physicsCommon;

        // Initial transform:
        rb = world->createRigidBody(rp3d::Transform(
            rp3d::Vector3(0, 0, 0),
            rp3d::Quaternion::identity()
        ));

        log::cout << "setting up size & static" << log::endl;
        auto size = info["rigidbody.size"].valVec3;
        auto isStatic = info["rigidbody.static"].valInt; // TODO Booleans

        if(isStatic) rb->setType(rp3d::BodyType::STATIC);
        

        // log::cout << "set parameter" << log::endl;
        // rb.SetShapeParameter(
        //     size.x * size.y * size.z,
        //     size.x, size.y, size.z,
        //     0.1f, 0.1f);

        // log::cout << "isStatic enable attr." << log::endl;
        // if(isStatic) rb.EnableAttribute(rbRigidBody::Attribute_Fixed);
        // env->Register(&rb);

        // this->offset = info["rigidbody.offset"].valVec3;
        // rb.SetPosition(glm2rb(entity->transform.position + offset * entity->transform.rotation));
        // rb.SetOrientation(glm2rb(glm::mat3_cast(entity->transform.rotation)));
    }
    
    virtual void update(float dt)
    {
        // rb.SetForce(ResourceGlobals::GRAVITY);
    }

    virtual void afterUpdate(float dt)
    {
        // puts("Updating physics object");
        
        // // TODO: find a faster way to do this!!!
        // glm::quat rot = rb2glm(rb.Orientation());
        // glm::vec3 eulerPrev = glm::eulerAngles(entity->transform.rotation);
        // glm::vec3 euler = glm::eulerAngles(rot);
        // rot = glm::quat(glm::vec3(eulerPrev.x, euler.y, eulerPrev.z)); // Only allow rotation on Y axis.

        // entity->transform.rotation = rot;
        // rb.SetOrientation(glm2rb(glm::mat3_cast(rot)));
        // entity->transform.position = rb2glm(rb.Position()) - offset * entity->transform.rotation;
        // // Removes the offset applied before!              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        
        // entity->transform.update();
    }
    
    void addForce(const glm::vec3 &force)
    {
        rb->applyForceToCenterOfMass(physics::glm2rp3d(force));
    }

    /** Change the position of an object with respect to collisions.
     * NOTE: transform.position is not updated until the next loop
     **/
    void setPositionColliding(const glm::vec3 &newPosition)
    {
        rb->setTransform(
            rp3d::Transform(physics::glm2rp3d(newPosition),
            rb->getTransform().getOrientation()));
    }

    // TODO? unload?
    ~ECRigidBody()
    {
        log::cerr << "~ECRigidBody()!" << log::endl;
        // env->Unregister(&rb);
    }

    EC_STATIC_CREATE(ECRigidBody);
};

class ECPlayerController : EntityComponent
{
    ECRigidBody *rb;
    float speed = 10.f;

public:
    virtual void start() override
    {
        // who needs c++'s casts, right?
        rb = (ECRigidBody*)entity->findComponentByType(EntityComponentType::RigidBody);
    }

    virtual void update(float dt) override
    {
        if(rb == nullptr) return; // todo: move to start() or something, this is bad...? im not sure tho

        // // TODO: fix (Z X Y) to be (X Y Z)... maybe rotateZXY is doing something?
        glm::vec3 move = { 0, 0, 0 };

        /**/ if(ResourceGlobals::window->keyPressed(GLFW_KEY_W))
            move.z = +speed;
        else if(ResourceGlobals::window->keyPressed(GLFW_KEY_S))
            move.z = -speed;
        
        /**/ if(ResourceGlobals::window->keyPressed(GLFW_KEY_D))
            move.x = +speed;
        else if(ResourceGlobals::window->keyPressed(GLFW_KEY_A))
            move.x = -speed;
        
        rb->setPositionColliding(
            entity->transform.position +
            move * dt * entity->transform.rotation);
    }
    
    EC_STATIC_CREATE(ECPlayerController);
};

/** Container for entities. */
class Scene
{
public:
    std::vector<std::unique_ptr<Entity>> entities;

    void start()
    {
        for(auto &e : entities) e->start();
    }

    void afterUpdate(float dt)
    {
        for(auto &e : entities) e->afterUpdate(dt);
    }

    void update(float dt)
    {
        // TODO? Make similar to loop in Entity.update()? Optimize?
        for(auto &e : entities) e->update(dt);
    }

    void render(
        core::gfx::camera &camera,
        core::gfx::deferred_renderer &renderer)
    {
        for(auto &e : entities) e->render(camera, renderer);
    }
};


/**
 * Prefer to use inipp::Ini<char> instead of this, this exists
 * so that SceneInfo can load multiple entities of the same type.
 * 
 * As I said before, this is bad code...
 */
struct MultiIni
{
    using Section = std::map<std::string, std::string>;
    using Sections = std::map<std::string, std::vector<Section>>;
    Sections sections;

    MultiIni(const std::string &filename)
    {
        std::ifstream ifs(filename);
        int lineno = 0;
        std::string section;
        for(std::string line; std::getline(ifs, line);)
        {
            // log::cout << "MultiIni: Reading line: '" << line << "'" << log::endl;
            ++lineno;

            // Trim the line:
            // [https://stackoverflow.com/a/217605/9110517]
            line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](auto ch) {
                return !std::isspace(ch);
            }));
            line.erase(std::find_if(line.rbegin(), line.rend(), [](auto ch) {
                return !std::isspace(ch);
            }).base(), line.end());

            // Line is empty or is a comment:
            if(line.size() == 0 || *line.begin() == ';')
            {
                log::cout << "MultiIni: Comment/Empty" << log::endl;
                // continue; - Skip the line.
            }
            else if(*line.begin() == '[')
            {
                if(*line.rbegin() != ']')
                {
                    log::cerr << filename << ':' << lineno << ": Expected ending ']'" << log::endl;
                    log::cerr << lineno << " | " << line << log::endl;
                }
                else
                {
                    // log::cout << "MultiIni: Section" << log::endl;
                    sections[section = line.substr(1, line.size() - 2)].push_back(Section());
                }
            }
            else if(line.find('=') != std::string::npos)
            {   //  ^^^^^^^^^^^^^^───────────┬───[1]
                // [TODO] Optimize line.find └───vvvvvvvvvvvvvv 
                std::string name = line.substr(0, line.find('=') - 1);
                // log::cout << "MultiIni: Equals" << log::endl;

                // Right trim the name `<name>   =   <value>`
                //        This ───────────────^^^
                name.erase(std::find_if(name.rbegin(), name.rend(), [](auto ch) {
                    return !std::isspace(ch);
                }).base(), name.end());

                // Left trim the value `<name>   =   <value>`
                //       This ────────────────────^^^
                std::string value = line.substr(line.find('=') + 1); // [1] This also
                
                value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](auto ch) {
                    return !std::isspace(ch);
                }));

                // log::cout << "MultiIni: '" << name << "' = '" << value << "'" << log::endl;

                sections[section].back()[name] = value;
            }
            else
            {
                log::cerr << filename << ':' << lineno << ": Bad format..." << log::endl;
                log::cerr << lineno << " | " << line << log::endl;
            }
        }
    }
    
    auto getQuat(const std::string &sec, const std::string &name, int index = 0, const glm::quat &def = { 1, 0, 0, 0}) -> glm::quat
    {
        try { return stringToQuat(sections[sec].at(index)[name]); } catch(std::out_of_range&) { return def; }
    };

    auto getVec3(const std::string &sec, const std::string &name, int index = 0, const glm::vec3 &def = { 0, 0, 0 }) -> glm::vec3
    {
        try { return stringToVec3(sections[sec].at(index)[name]); } catch(std::out_of_range&) { return def; }
    }

    auto getVec2(const std::string &sec, const std::string &name, int index = 0, const glm::vec2 &def = { 0, 0 }) -> glm::vec2
    {
        try { return stringToVec2(sections[sec].at(index)[name]); } catch(std::out_of_range&) { return def; }
    }

    auto getFloat(const std::string &sec, const std::string &name, int index = 0, float def = 0) -> float
    {
        try { return stringToFloat(sections[sec].at(index)[name]); } catch(std::out_of_range&) { return def; }
    }

    auto getInt(const std::string &sec, const std::string &name, int index = 0, int def = 0) -> int
    {
        try { return stringToInt(sections[sec].at(index)[name]); } catch(std::out_of_range&) { return def; }
    }

    auto getString(const std::string &sec,
                         const std::string &name,
                         int index = 0,
                         const std::string &def = "") -> std::string
    {
        try { return sections[sec].at(index)[name]; } catch(std::out_of_range&) { return def; }
    }
};

using CreateEntityComponentFunc =
    EntityComponent*(*)(Entity*, ResourceManager&, std::unordered_map<std::string, ValueInfo>&);

struct EntityComponentInfo
{
    CreateEntityComponentFunc createFunc;
    std::unordered_map<std::string, ValueInfo::Type> info;
};

std::unordered_map<std::string_view, EntityComponentInfo> entityComponentInfo =
{
    {
        "StaticMesh",
        EntityComponentInfo {
            .createFunc = &ECStaticMesh::create,
            .info = {
                { "staticmesh.mesh", ValueInfo::STRING },
                { "staticmesh.texture", ValueInfo::STRING },
                { "staticmesh.texture.tiling", ValueInfo::VEC2 }
            }
        }
    },
    {
        "RigidBody",
        EntityComponentInfo {
            .createFunc = &ECRigidBody::create, //E::create,
            .info = { { "rigidbody.size", ValueInfo::VEC3 }, { "rigidbody.offset", ValueInfo::VEC3 }, { "rigidbody.static", ValueInfo::INT } }
        }
    },
    {
        "PlayerController",
        EntityComponentInfo {
            .createFunc = &ECPlayerController::create, //E::create,
            .info = { }
        }
    },
};


struct EntityInfo
{
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    std::vector<CreateEntityComponentFunc> components;
    std::unordered_map<std::string, ValueInfo> info;

    EntityInfo(MultiIni::Section &section)
    {
        for(const auto &kv : section)
        {
            std::cout << "Entity Section '" << kv.first << "' -> '" << kv.second << "'" << std::endl;
        }

        position = stringToVec3(getOrDefault(section, std::string("position"), std::string("0 0 0")));
        rotation = stringToQuat(getOrDefault(section, std::string("rotation"), std::string("1 0 0 0")));
        scale = stringToVec3(getOrDefault(section, std::string("scale"), std::string("1 1 1")));

        std::cout << "EntityInfo:"
            << "\n  position: " << vectorToString(position)
            << "\n  rotation: " << vectorToString(rotation)
            << "\n  scale   : " << vectorToString( scale  ) << std::endl;

        std::cout << "Components: " << section["components"] << std::endl; 
        std::istringstream components(section["components"]);
        for(std::string component; components >> component;)
        {
            if(entityComponentInfo.find(component) == entityComponentInfo.end())
                log::cerr << "Unknown Entity Component: '" << component << "'!" << log::endl;

            // Nice thing about the error above and this ──────────┐
            // Missing comp. types are reported only once.         │
            //                                        vvvvvvvvvvv──┘
            for(auto &prop : entityComponentInfo[component].info)
            {
                switch(prop.second)
                {
                case ValueInfo::STRING: info[prop.first].valString = new std::string(section[prop.first]); break;
                case ValueInfo::VEC3  : info[prop.first].valVec3   = stringToVec3 (section[prop.first]); break;
                case ValueInfo::VEC2  : info[prop.first].valVec2   = stringToVec2 (section[prop.first]); break;
                case ValueInfo::QUAT  : info[prop.first].valQuat   = stringToQuat (section[prop.first]); break;
                case ValueInfo::FLOAT : info[prop.first].valFloat  = stringToFloat(section[prop.first]); break;
                case ValueInfo::INT   : info[prop.first].valInt    = stringToInt  (section[prop.first]); break;
                default: log::cerr << "Internal error: Bad prop type: " << prop.second << log::endl; break;
                }
            }

            this->components.push_back(entityComponentInfo[component].createFunc);
        }
    }
};

struct SceneInfo
{
    std::vector<EntityInfo> entities;
    std::string name;

    SceneInfo(MultiIni &c)
    {
        log::cout << "SceneInfo ctor:" << log::endl;
        name = c.getString("_info", "name");
        log::cout << "name = '" << name << '\'' << log::endl;
        bool shouldCheckInfo = true; /* Tiny optimization... */
        for(auto &section : c.sections)
        {
            log::cout << "Section: '" << section.first << '\'' << log::endl;
            if(shouldCheckInfo && section.first == "_info") { shouldCheckInfo = false; continue; }
            for(auto &sect : section.second)
                entities.push_back(EntityInfo(sect));
        }
    }
};

/** Container for to-be loaded resources. */
struct ResourceLoader
{
    using ResourceLoadMap = std::unordered_map<std::string, std::string>;
    ResourceLoadMap meshes;
    ResourceLoadMap textures;
    ResourceLoadMap shaders;
    ResourceLoadMap cubemaps;

    int count = 0;

    void beforeLoader()
    {
        count = meshes.size() + textures.size() + shaders.size() + cubemaps.size();
    }
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

/** Utility wrapper class for inipp::Ini<char>. */
struct Configuration
{
    inipp::Ini<char> &config;

    auto getQuat(const std::string &sec, const std::string &name) -> glm::quat
    {
        std::string s = "1 0 0 0";
        inipp::get_value(config.sections[sec], name, s);
        return stringToQuat(s);
    };

    auto getVec3(const std::string &sec, const std::string &name) -> glm::vec3
    {
        std::string s = "0 0 0";
        inipp::get_value(config.sections[sec], name, s);
        return stringToVec3(s);
    };

    auto getVec2(const std::string &sec, const std::string &name) -> glm::vec2
    {
        std::string s = "0 0";
        inipp::get_value(config.sections[sec], name, s);
        return stringToVec2(s);
    };

    auto getFloat(const std::string &sec, const std::string &name, float def = 0) -> float
    {
        float x = def;
        inipp::get_value(config.sections[sec], name, x);
        return x;
    };

    auto getInt(const std::string &sec, const std::string &name, int def = 0) -> int
    {
        int x = def;
        inipp::get_value(config.sections[sec], name, x);
        return x;
    };

    auto getString(const std::string &sec,
                         const std::string &name,
                         const std::string &def = "") -> std::string
    {
        std::string s = def;
        inipp::get_value(config.sections[sec], name, s);
        return s;
    };
};

/** Function for saving the configuration, assigned in `main()` */
std::function<void()> saveGlobalConfig;

/** Represents a controller of the game. */
class Composition
{
public:
    virtual ~Composition()
    {
        log::cerr << "~Composition()!" << log::endl;
    }

    virtual void saveOwnConfig(inipp::Ini<char> &config) {}

    virtual void load(
        core::window::window *win,
        Configuration &config,
        ResourceManager &resourceManager,
        ResourceLoader &resourceLoader) {}
    
    virtual void loop(
        float dt,
        core::gfx::deferred_renderer &renderer,
        ImGuiIO &io,
        ResourceManager &resourceManager,
        ResourceLoader &resourceLoader) {}
    
    virtual void mousemove(double xpos, double ypos) {}
};

class LoadingComposition : public Composition
{
public:
    enum { Meshes, Textures, Shaders, Cubemaps, Done } loadingWhat;
    ResourceLoader::ResourceLoadMap::const_iterator currentResource;
    bool isDone = false;
    int loadedCount = 0;
    std::string currentLoading = "";
    
    core::gfx::texture *screenTextures;
    ImVec2 *screenTextureSizes;
    int currentTexture = 0;

    std::function<void(int newIndex, bool shouldLoad)> changeComposition;

    LoadingComposition(const std::function<void(int newIndex, bool shouldLoad)> &changeComposition)
        : changeComposition(changeComposition) {}

    ~LoadingComposition()
    {
        log::cerr << "~LoadingComposition()!" << log::endl;
        delete screenTextures;
        delete screenTextureSizes;
    }

    void saveOwnConfig(inipp::Ini<char> &config) override {}

    void load(
        core::window::window *win,
        Configuration &config,
        ResourceManager &resourceManager,
        ResourceLoader &resourceLoader) override
    {
        loadingWhat = Meshes;
        currentResource = resourceLoader.meshes.begin();
        currentLoading = resourceLoader.meshes.begin()->first;

        auto data1 = readTexture("data/textures/Screen_PortalImage1.png", true);
        screenTextures = new core::gfx::texture[1] { core::gfx::texture{data1, false} };
        screenTextureSizes = new ImVec2[1] { ImVec2(data1.width, data1.height) };
        deleteTexture(data1);
    }
    
    void loop(
        float dt,
        core::gfx::deferred_renderer &renderer,
        ImGuiIO &io,
        ResourceManager &resourceManager,
        ResourceLoader &resourceLoader) override
    {
        // renderer.clear();
        if(!isDone)
        {
            switch(loadingWhat)
            {
                case Meshes:
                {
                    std::vector<core::gfx::vertex> vertices;
                    std::vector<unsigned int> indices;
                    readMesh(currentResource->second.c_str(), vertices, indices);
                    resourceManager.meshes[currentResource->first] = std::move(
                        std::unique_ptr<core::gfx::mesh>(new core::gfx::mesh{ vertices, indices }));

                    ++currentResource;
                    ++loadedCount;
                    if(currentResource == resourceLoader.meshes.end())
                    {
                        loadingWhat = Textures;
                        currentResource = resourceLoader.textures.begin();
                    }
                    currentLoading = currentResource->second;
                } break;
                case Textures:
                {
                    auto data = readTexture(currentResource->second.c_str());
                    resourceManager.textures[currentResource->first] = std::move(
                        std::unique_ptr<core::gfx::texture>(new core::gfx::texture{ data }));
                    deleteTexture(data);

                    ++currentResource;
                    ++loadedCount;
                    if(currentResource == resourceLoader.textures.end())
                    {
                        loadingWhat = Done;
                        isDone = true;
                    }
                    else
                    {
                        currentLoading = currentResource->second;
                    }
                } break;

                default: break;
            }

            // [NOTE] For debugging.
            // std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        float progress = float(loadedCount) / float(resourceLoader.count);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::Begin("LoadingScreen", nullptr, // ImGuiWindowFlags_NoBackground | 
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

        ImGui::BeginChild("Screen Image", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing()));

        // glm::lookAtLH()

        float actualHeight = std::min(screenTextureSizes[currentTexture].y,
            ImGui::GetWindowSize().y - ImGui::GetTextLineHeightWithSpacing());
        
        float autoWidth =
            (screenTextureSizes[currentTexture].x / screenTextureSizes[currentTexture].y) * actualHeight;
        ImGui::Image((void*)(intptr_t)screenTextures[currentTexture].id, ImVec2(autoWidth, actualHeight));

        ImGui::EndChild();

        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
        float font_size = ImGui::GetFontSize() * currentLoading.size() / 2;
        ImGui::SameLine(
            ImGui::GetWindowSize().x / 2 -
            font_size + (font_size / 2)
        );
        ImGui::Text(currentLoading.c_str());
        ImGui::End();

        if(isDone) changeComposition(1, true);
    }
    
    void mousemove(double xpos, double ypos) override {}
};



class MainComposition : public Composition
{
public:
    Scene scene;
    core::gfx::camera *camera;

    core::gfx::skybox *skybox;
    core::gfx::shaders::skybox_shader_instance *skyboxShader;
    core::gfx::shaders::screen_shader_instance *screenShader;
    core::gfx::shaders::shadow_shader *shadowShader;

    glm::vec4 lightValue;
    glm::mat4 lightProjMatrix;
    glm::mat4 lightViewMatrix;
    glm::mat4 lightMVPMatrix;

    float lightScreenSize = 25.f;
    float cameraSpeed;

    float cameraYaw = 0, cameraPitch = 0;
    float lastMouseX, lastMouseY;
    bool firstMouse = true;

    core::gfx::mesh *quadMesh;

    core::window::window *win;

    rbEnvironment *env;

    const glm::vec3 GRAVITY = { 0.f, -1.f, 0.f };

    ~MainComposition()
    {
        log::cerr << "~MainComposition()!" << log::endl;
        delete camera;
        delete skybox;
        delete skyboxShader;
        delete screenShader;

        // delete env;
    }

    void load(
        core::window::window *win,
        Configuration &config,
        ResourceManager &resourceManager,
        ResourceLoader &resourceLoader) override
    {
        log::cout << "MainComposition::load()" << log::endl;
        this->win = win;

        // -----------============  Camera Setup  ============----------- //

        log::cout << "Setting Up The Camera..." << log::endl;
        cameraSpeed = config.getFloat("world", "cameraSpeed", 3.5f);
        camera = new core::gfx::camera(win->width, win->height, 0.1f, 100.f);
        camera->transform.position = config.getVec3("world", "cameraPos");
        camera->transform.rotation = config.getQuat("world", "cameraRotation");
        camera->transform.scale    = { 1, 1, 1 };
        camera->update();

        // -----------============  World Setup  ============----------- //

        log::cout << "Setting Up Physics Enviroment..." << log::endl;
        // env = new rbEnvironment(rbEnvironment::Config {
        //     .RigidBodyCapacity = 20,
        //     .ContactCapacty = 100,
        // });

        rp3d::PhysicsCommon physicsCommon;
        ResourceGlobals::physicsCommon = &physicsCommon;
        ResourceGlobals::physicsWorld  = physicsCommon.createPhysicsWorld();

        skybox = new core::gfx::skybox {
            .texture = *resourceManager.cubemaps["skybox"],
            .skyMesh = *resourceManager.meshes["cube"],
        };

        skybox->update(camera->transform.position);

        // auto &skyboxShader = *resourceManager.shaders["skybox"];

        skyboxShader = new core::gfx::shaders::skybox_shader_instance {
            .type = *static_cast<core::gfx::shaders::skybox_shader*>(resourceManager.shaders["skybox"].get())
        };
        
        // core::gfx::mesh &quadMesh = *resourceManager.meshes["quad"];
        screenShader = new core::gfx::shaders::screen_shader_instance {
            .type = *static_cast<core::gfx::shaders::screen_shader*>(resourceManager.shaders["screen"].get())
        };

        auto lightPosition = config.getVec3("lighting", "lightPosition"); // glm::vec4(1.f, 1.f, 0.3f, 1.4f);
        lightValue    = glm::vec4(lightPosition, config.getFloat("lighting", "lightIntensity", 1.0f));
        screenShader->uniforms.lighting.directional = lightValue;
        screenShader->uniforms.lighting.directionalTint = config.getVec3("lighting", "lightColor");

        updateLightMatrix();

        shadowShader = static_cast<core::gfx::shaders::shadow_shader*>(resourceManager.shaders["shadow"].get());
        ResourceGlobals::shadowShader = shadowShader;
        quadMesh = resourceManager.meshes["quad"].get();

        log::cout << "Loading scene configuration..." << log::endl;
        MultiIni iniConfig("data/scenes/main.ini");
        log::cout << "SceneInfo initializer..." << log::endl;
        SceneInfo sceneInfo(iniConfig);
        log::cout << "Iterating Entities..." << log::endl;
        for(auto &einfo : sceneInfo.entities)
        {
            log::cout << "Loading entity..." << log::endl;
            auto e = new Entity();
            e->transform.position = einfo.position;
            e->transform.rotation = einfo.rotation;
            e->transform.scale = einfo.scale;
            for(const auto &comp : einfo.components)
            {
                log::cout << "  Loading entity component..." << log::endl;
                log::cout << "e - " << e << log::endl;
                log::cout << "comp - " << (void*)comp << log::endl;
                auto cp = comp(e, resourceManager, einfo.info);
                log::cout << "cp - " << cp << log::endl;
                e->add(cp);
                log::cout << "  Done loading entity component!" << log::endl;
            }
            scene.entities.push_back(std::unique_ptr<Entity>(e));
            log::cout << "Done loading entity!" << log::endl;
        }
        log::cout << "Done loading scene!" << log::endl;
        log::cout << "Scene entity count: " << scene.entities.size() << log::endl;

        
        // {
        //         *resourceManager.meshes["portal"],
        //         *resourceManager.textures["portal"],
        //         core::gfx::shaders::geometry_shader_instance {
        //             .type = *static_cast<core::gfx::shaders::geometry_shader*>(resourceManager.shaders["lit"].get())
        //         },
        //         env,
        //         PhysicsEntity::PhysicsData { .isStatic = true, .size = { 0.3f, 3, 1 }, .gravity = GRAVITY }
        //     auto e = new Entity;
            
        //     auto cStaticMesh = new ECStaticMesh; 
        //     auto cRigidBody = new ECRigidBody; 

        //     cStaticMesh->shader.uniforms.materialData.tiling = glm::vec2{ 1, 1 };
        //     e->transform.position = { 0, -1.f, 0 };
        //     e->transform.rotation = glm::angleAxis(glm::radians(180.f), glm::vec3(0, 1, 0));
        //     e->transform.scale    = { 1.5f, 1.5f, 1.5f };
        //     e->transform.update();
        //     e->setPhysics({ 0, 2, 0 });
        //     // Offset for th physics shape.

        //     scene.entities.push_back(std::unique_ptr<Entity>(e));
        // }


        // {
        //     auto e = new PhysicsEntity {
        //         *resourceManager.meshes["sphere"],
        //         *resourceManager.textures["wall"],
        //         core::gfx::shaders::geometry_shader_instance {
        //             .type = *static_cast<core::gfx::shaders::geometry_shader*>(resourceManager.shaders["lit"].get())
        //         },
        //         env,
        //         PhysicsEntity::PhysicsData { .isStatic = true, .size = { 1, 1, 1 }, .gravity = GRAVITY }
        //     };

        //     e->shader.uniforms.materialData.tiling = glm::vec2{ 2, 2 };
        //     e->transform.position = { 4, -0.4, 6 };
        //     e->transform.rotation = glm::identity<glm::quat>();
        //     e->transform.scale    = { 1, 1, 1 };
        //     e->transform.update();
        //     e->setPhysics();

        //     scene.entities.push_back(std::unique_ptr<Entity>(e));
        // }

        // {
        //     auto e = new PhysicsEntity {
        //         *resourceManager.meshes["quad"],
        //         *resourceManager.textures["cobblestone"],
        //         core::gfx::shaders::geometry_shader_instance {
        //             .type = *static_cast<core::gfx::shaders::geometry_shader*>(resourceManager.shaders["lit"].get())
        //         },
        //         env,
        //         PhysicsEntity::PhysicsData { .isStatic = true, .size = { 10, 10, 1 }, .gravity = GRAVITY }
        //     };

        //     e->shader.uniforms.materialData.tiling = glm::vec2{ 20, 20 };
        //     e->transform.position = { 0, -1, 0 };
        //     e->transform.rotation = glm::angleAxis(glm::pi<float>() * 0.5f, glm::vec3(1, 0, 0));
        //     e->transform.scale    = { 10, 10, 10 };
        //     e->transform.update();
        //     e->setPhysics({ 0, 0, -1 });

        //     scene.entities.push_back(std::unique_ptr<Entity>(e));
        // }

        // {
        //     auto e = new PhysicsEntity {
        //         *resourceManager.meshes["cube"],
        //         *resourceManager.textures["sandstone"],
        //         core::gfx::shaders::geometry_shader_instance {
        //             .type = *static_cast<core::gfx::shaders::geometry_shader*>(resourceManager.shaders["lit"].get())
        //         },
        //         env,
        //         PhysicsEntity::PhysicsData { .isStatic = false, .size = { 1, 1, 1 }, .gravity = GRAVITY }
        //     };

        //     e->shader.uniforms.materialData.tiling = glm::vec2{ 10, 10 };
        //     e->transform.position = { 2, 30, 0 };
        //     e->transform.rotation = glm::identity<glm::quat>();
        //     e->transform.scale    = { 1, 1, 1 };
        //     e->transform.update();
        //     e->setPhysics();

        //     scene.entities.push_back(std::unique_ptr<Entity>(e));
        // }

        // auto &myCube = *scene.entities[2].get();

        // TODO: add start() to Composition.
        scene.start();
    }

    void saveOwnConfig(inipp::Ini<char> &config) override
    {
        config.sections["world"]["cameraPos"] = vectorToString(camera->transform.position);
        config.sections["world"]["cameraRotation"] = vectorToString(camera->transform.rotation);
        config.sections["world"]["cameraSpeed"] = std::to_string(cameraSpeed);
        
        config.sections["lighting"]["lightPosition"] = vectorToString(glm::vec3(lightValue));
        config.sections["lighting"]["lightIntensity"] = std::to_string(lightValue.w);
        config.sections["lighting"]["lightColor"] =
            vectorToString(screenShader->uniforms.lighting.directionalTint);
    }

    void updateLightMatrix()
    {
        lightProjMatrix = glm::ortho(
            -lightScreenSize, lightScreenSize,
            -lightScreenSize, lightScreenSize,
            -10.f, 40.f);
        lightViewMatrix = glm::lookAt(glm::vec3(lightValue), glm::vec3{ 0, 0, 0 }, glm::vec3{ 0, 1, 0 });
        lightMVPMatrix = lightProjMatrix * lightViewMatrix;
        ResourceGlobals::lightSpaceMatrix = &lightMVPMatrix;
    }

    bool handleInput(float dt)
    {
        // glm::vec3 move { 0, 0, 0 };

        // /**/ if(win->keyPressed(GLFW_KEY_A))
        //     move.x -= cameraSpeed * dt;
        // else if(win->keyPressed(GLFW_KEY_D))
        //     move.x += cameraSpeed * dt;

        // /**/ if(win->keyPressed(GLFW_KEY_LEFT_SHIFT))
        //     move.y -= cameraSpeed * dt;
        // else if(win->keyPressed(GLFW_KEY_SPACE))
        //     move.y += cameraSpeed * dt;

        // /**/ if(win->keyPressed(GLFW_KEY_S))
        //     move.z -= cameraSpeed * dt;
        // else if(win->keyPressed(GLFW_KEY_W))
        //     move.z += cameraSpeed * dt;

        if(win->keyPressed(GLFW_KEY_ESCAPE))
        {
            win->lockCursor(false);
        }
    
        // if(move != glm::vec3{ 0, 0, 0 })
        // {
        //     camera->transform.position += move * camera->transform.rotation;
        //     return true;
        // }
        return false;
    }

    void drawGui()
    {
        constexpr float maxSunPos = 20;
        ImGui::Begin("Debug Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Checkbox("Shadow Buffer", &screenShader->uniforms.debug.showShadowMap);
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
            screenShader->uniforms.lighting.directional = lightValue;
        }

        ImGui::ColorEdit3("Sun Color", &screenShader->uniforms.lighting.directionalTint.x);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SliderFloat("Camera Speed", &cameraSpeed, 0, 5);

        ImGui::End();

        if(ImGui::BeginMainMenuBar())
        {
            if(ImGui::BeginMenu("File"))
            {
                if(ImGui::MenuItem("Save Config", "META+SHIFT+S"))
                {
                    saveGlobalConfig();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    void loop(
        float dt,
        core::gfx::deferred_renderer &renderer,
        ImGuiIO &io,
        ResourceManager &resourceManager,
        ResourceLoader &resourceLoader) override
    {
        if(!io.WantCaptureKeyboard) if(handleInput(dt)) camera->update();
        if(!io.WantCaptureMouse)
        {
            if(glfwGetMouseButton((GLFWwindow*)win->win, 0) == GLFW_PRESS)
                win->lockCursor(true);
        }

        // glClear(GL_COLOR_BUFFER_BIT);
        scene.update(dt);
        env->Update(dt, 3);
        scene.afterUpdate(dt);
        renderer.begin();
        scene.render(*camera, renderer);
        renderer.sky(*camera, *skybox, *skyboxShader);
        renderer.light(*screenShader, *quadMesh);
        drawGui();
    }

    void mousemove(double xpos, double ypos) override
    {
        if(!win->isCursorLocked) return;
        if(firstMouse)
        {
            lastMouseX = xpos;
            lastMouseY = ypos;
            firstMouse = false;
        }

        float xoffset = lastMouseX - xpos;
        float yoffset = lastMouseY - ypos;
        lastMouseX = xpos;
        lastMouseY = ypos;

        float sensitivity = 1.f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        cameraYaw += xoffset;
        cameraPitch += yoffset;

        if(cameraPitch >  glm::pi<float>()*2) cameraPitch =  glm::pi<float>() * 2;
        if(cameraPitch < -glm::pi<float>())   cameraPitch = -glm::pi<float>();

        // auto xRotation = glm::angleAxis(cameraYaw  , glm::vec3(0, 1, 0));
        // auto yRotation = glm::angleAxis(cameraPitch, glm::vec3(1, 0, 0));
        camera->transform.rotation = rotateZYX(cameraPitch, cameraYaw, 0);
        camera->update();
    }


};

int main(int argc, char *argv[])
{
    // INIReader configReader("config.ini");
    inipp::Ini<char> iniConfig;
    std::ifstream configInputStream("config.ini");
    iniConfig.parse(configInputStream);
    configInputStream.close();
    iniConfig.strip_trailing_comments();
    iniConfig.default_section(iniConfig.sections["DEFAULT"]);
    iniConfig.interpolate();
    log::csec << "INI File configuration:" << log::endl;
    iniConfig.generate(std::cout);
    Configuration config { .config = iniConfig };

    log::csec << "Program:" << log::endl;

    auto windowSize = config.getVec2("window", "size");
    core::window::window win { int(windowSize.x), int(windowSize.y), "Main" }; // This should be initialized first!!
    ResourceGlobals::window = &win;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)win.win, true);
    ImGui_ImplOpenGL3_Init("#version 410");
    ImGui::GetStyle().Alpha = config.getFloat("window", "alpha", 1.0f);
    ImGui::GetStyle().WindowTitleAlign = ImVec2(0.5, 0.5);
    io.Fonts->AddFontFromFileTTF("data/fonts/UbuntuMono-Regular.ttf", config.getInt("window", "fontSize", 13));
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    core::gfx::deferred_renderer renderer {
        .buffer = core::gfx::gbuffer{int(windowSize.x*2), int(windowSize.y*2)},
        .shadowBuffer = core::gfx::sbuffer{int(windowSize.x*2), int(windowSize.y*2)},
        .width = int(windowSize.x*2),
        .height = int(windowSize.y*2), // fix this! (needs to be 1080/2 instead of 1080!)
        .debugBuffers = false
    };

    ResourceManager resourceManager;
    ResourceLoader resourceLoader;
    
    // -----------============ Mesh Loading ============----------- //

    resourceLoader.meshes["sphere"] = "data/models/sphere.obj";
    resourceLoader.meshes["quad"] = "data/models/quad.obj";
    resourceLoader.meshes["cube"] = "data/models/cube.obj";
    resourceLoader.meshes["portal"] = "data/models/Portal2UV_Fixed.obj";

    // -----------============ Texture Loading ============----------- //

    resourceLoader.textures["wall"] = "data/textures/wall.jpeg";
    resourceLoader.textures["sandstone"] = "data/textures/wall-sandstone.jpeg";
    resourceLoader.textures["cobblestone"] = "data/textures/floor-cobblestone.jpeg";
    resourceLoader.textures["portal"] = "data/textures/StonePortal2.jpg";
    
    {
        auto SKYBOX_PATH = config.getString("data", "skyboxPrefix", "data/textures/skybox/skybox_");
        auto SKYBOX_EXT  = config.getString("data", "skyboxSuffix", ".jpg");
        auto xPos = readTexture(SKYBOX_PATH + "px" + SKYBOX_EXT, true);
        auto xNeg = readTexture(SKYBOX_PATH + "nx" + SKYBOX_EXT, true);
        auto yPos = readTexture(SKYBOX_PATH + "py" + SKYBOX_EXT, true);
        auto yNeg = readTexture(SKYBOX_PATH + "ny" + SKYBOX_EXT, true);
        auto zPos = readTexture(SKYBOX_PATH + "pz" + SKYBOX_EXT, true);
        auto zNeg = readTexture(SKYBOX_PATH + "nz" + SKYBOX_EXT, true);
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

    resourceManager.shaders["lit"].reset(
        new core::gfx::shaders::geometry_shader{readFile("data/shaders/lit.vertex"), readFile("data/shaders/dlit.fragment")
    });
    {
        auto shader = (core::gfx::shaders::geometry_shader*)(resourceManager.shaders["lit"].get());
        shader->setUniform(shader->uniforms.materialData.tiling, glm::vec2(1, 1));
    }

    resourceManager.shaders["shadow"].reset(
        new core::gfx::shaders::shadow_shader{readFile("data/shaders/shadow.vertex"), readFile("data/shaders/shadow.fragment")
    });

    resourceManager.shaders["skybox"].reset(
        new core::gfx::shaders::skybox_shader{readFile("data/shaders/skybox.vertex"), readFile("data/shaders/skybox.fragment")
    });

    resourceManager.shaders["screen"].reset(
        new core::gfx::shaders::screen_shader{
            readFile("data/shaders/deferred/screen.vertex"),
            readFile("data/shaders/deferred/screen.fragment")
        }
    );



    // -----------============   Game Loop   ============----------- //

    win.lockCursor(true);

    // float cameraRotSpeed = 1.f;

    Composition *composition = nullptr;
    Composition **compositions = nullptr;

    auto changeComposition = [&](int newIndex, bool shouldLoad)
    {
        log::cwrn << "Change Composition to #" << newIndex << (shouldLoad ? " (should load)" : "") << '!' << log::endl;
        log::cout << "compositions = " << compositions << log::endl;
        composition = compositions[newIndex];
        if(shouldLoad)
        {
            log::cout << "Loading Composition #" << newIndex << log::endl;
            composition->load(&win, config, resourceManager, resourceLoader);
        }
        log::cwrn << "Composition changed." << log::endl;
    };

    compositions = new Composition*[2] {
        new LoadingComposition(changeComposition),
        new MainComposition
    };

    composition = compositions[0];

    saveGlobalConfig = [&]() -> void
    {
        iniConfig.sections["window"]["size"] = vectorToString(windowSize);
        composition->saveOwnConfig(iniConfig);
        std::ofstream ofs("config.ini");
        iniConfig.generate(ofs);
        ofs.close();
    };

    win.lockCursor(false);

    // [NOTE] We load the LoaderComposition here, which does not require the resourceManager to be loaded.
    log::cout << "composition->load()" << log::endl;
    composition->load(&win, config, resourceManager, resourceLoader);
    log::cout << "composition->load() done." << log::endl;

    // struct ThreadCorrectOverload_ {};

    resourceLoader.beforeLoader();

    // std::thread resourceLoadingThread(
    //     [&]()
    //         // ResourceLoader &resourceLoader,
    //         // ResourceManager &resourceManager,
    //         // Composition **&compositions,
    //         // Composition *&composition,
    //         // core::window::window &win,
    //         // Configuration &config)
    //     {
    //         resourceLoader.loadMeshes(resourceManager);
    //         resourceLoader.loadTextures(resourceManager);
    //         composition = compositions[1];
    //         log::cwrn << "MainComposition::load()" << log::endl;
    //         composition->load(&win, config, resourceManager, resourceLoader);
    //         log::cwrn << "MainComposition::load() done." << log::endl;
    //     }
    //     // ,
    //     // std::ref(resourceLoader),
    //     // std::ref(resourceManager),
    //     // std::ref(compositions),
    //     // std::ref(compositions),
    //     // std::ref(win),
    //     // std::ref(config)
    // );

    log::csec << "Show Window:" << log::endl;
    core::window::show(win,
    
    /* On Update */
    [&](float dt)
    {
        composition->loop(dt, renderer, io, resourceManager, resourceLoader);
        // ImGui::ShowDemoWindow();
    },

    /* -Disabled- On Resize */
    [&](int w, int h) { }, false,

    /* -Disabled- On Keypress */
    [&](int key, int scancode, int action, int mods) {}, false,


    [&](double xpos, double ypos)
    {
        composition->mousemove(xpos, ypos);
    }, true,
    
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

    log::csec << "Deleting Compositions..." << log::endl;
    log::cout << "comp loop." << log::endl;
    for(int i = 0; i < 2; ++i) delete compositions[i];
    log::cout << "delete [compositions]" << log::endl;
    delete compositions;
    log::cout << "main() end." << log::endl;
    // resourceLoadingThread.join();
}
