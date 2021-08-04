#ifndef CORE
#define CORE
#include <string>
#include <vector>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

namespace srd::core
{
    namespace window
    {
        template<typename F, typename ...Args>
        concept callable = requires(F f, Args... args) { f(args...); };

        struct window
        {
            int width, height;
            const char *title;
            void *win;

            window(int width, int height, const char *title);
            ~window();

            bool keyPressed(int key);
        };
    }

    namespace math
    {
        struct transform
        {
            glm::vec3 position;
            glm::quat rotation;
            glm::vec3 scale;
            glm::mat4 matrix;
            void update();
        };
    };

    namespace gfx
    {
        struct shader
        {
            unsigned int id;

            shader(const std::string &vertex, const std::string &fragment);
            ~shader();
            void use() const;
            int getUniform(const std::string &name) const;
            void setUniform(int location, int value) const;
            void setUniform(int location, bool value) const;
            void setUniform(int location, float value) const;
            void setUniform(int location, const glm::vec4 &value) const;
            void setUniform(int location, const glm::quat &value) const;
            void setUniform(int location, const glm::vec3 &value) const;
            void setUniform(int location, const glm::vec2 &value) const;
            void setUniform(int location, const glm::mat4 &value) const;
            void setUniform(int location, const glm::mat3 &value) const;
        };

        namespace shaders
        {
            /** Shader used for the geometry pass. */
            struct geometry_shader : public shader
            {
                struct
                {
                    int transform;
                    int transformLightSpace;
                    int normalMatrix;

                    int texture0;

                    struct
                    {
                        int tiling;
                    } materialData; 
                } uniforms;

                geometry_shader(const std::string &vertex, const std::string &fragment);
            };

            struct geometry_shader_instance
            {
                geometry_shader &type;
                struct
                {
                    struct
                    {
                        glm::vec2 tiling = { 1, 1 };
                    } materialData;
                } uniforms;
                
                /** Binds the shader and sets all of the necessary uniforms. */
                void use() const;
            };

            /** Shader used for the shadow geometry pass. */
            struct shadow_shader : public shader
            {
                struct
                {
                    int transformLightSpace;
                } uniforms;

                shadow_shader(const std::string &vertex, const std::string &fragment);
            };

            /** Shader which is used by the deferred renderer. */
            struct screen_shader : public shader
            {
                struct
                {
                    int texturePosition;
                    int textureNormal;
                    int textureDiffuse;
                    int textureDepthColor;
                    int textureShadowDepth;
                    int texturePositionLightSpace;

                    struct
                    {
                        int directional;
                        int ambient;
                    } lighting;

                    struct
                    {
                        int showShadowMap;
                    } debug;
                } uniforms;

                screen_shader(const std::string &vertex, const std::string &fragment);
            };

            struct screen_shader_instance
            {
                screen_shader &type;
                struct
                {
                    struct
                    {
                        glm::vec4 directional = { 0, 1.f, 0, 1.f };
                        float ambient = 0.2f;
                    } lighting;

                    struct
                    {
                        bool showShadowMap = false;
                    } debug;
                } uniforms;

                /** Binds the shader and sets all of the necessary uniforms. */
                void use() const;
            };

            /** Shader used for the sky pass. */
            struct skybox_shader : public shader
            {
                struct
                {
                    int transform;
                    int texture0;
                    int texture1;
                    int tint;
                    int transition;
                } uniforms;

                skybox_shader(const std::string &vertex, const std::string &fragment);
            };

            /** Shader used for the sky pass. */
            struct skybox_shader_instance
            {
                skybox_shader &type;
                struct
                {
                    glm::vec4 tint = { 1, 1, 1, 1 };
                    float transition;
                } uniforms;
                
                void use() const;
            };
        }

        struct vertex
        {
            glm::vec3 position;
            glm::vec3 normal;
            glm::vec2 texcoord;
            glm::vec3 tangent;

            bool operator==(const vertex &other) const;
        };

        struct mesh
        {
            unsigned int vbo, ebo, vao;
            unsigned int elementCount;
            mesh(const std::vector<vertex> &vertices, const std::vector<unsigned int> &indices);
            ~mesh();
            void bind() const;
        };

        struct texture
        {
            struct data
            {
                int width, height, channels;
                unsigned char *data;
            };

            unsigned int id;

            texture(const data &data);
            void bind(int unit);
        };

        struct camera
        {
            math::transform transform;
            glm::mat4 projMatrix;
            glm::mat4 viewMatrix;

            camera(int width, int height, float near, float far);
            void update();
            void resize(int width, int height);
            glm::mat4 projection(const glm::mat4 &model);
        };

        /** \deprecated */
        struct forward_renderer
        {
            void render(camera &camera,
                        math::transform &transform,
                        mesh &mesh,
                        texture &texture,
                        shaders::geometry_shader_instance &shader);
        };

        struct cubemap
        {
            unsigned int id;

            cubemap(const texture::data &xPos,
                    const texture::data &xNeg,
                    const texture::data &yPos,
                    const texture::data &yNeg,
                    const texture::data &zPos,
                    const texture::data &zNeg);

            ~cubemap();

            void bind(int unit);
        };

        /** Shadow Framebuffer. */
        struct sbuffer
        {
            unsigned int fbo;
            unsigned int depthTexture;
            int width, height;

            sbuffer(int width, int height);
            ~sbuffer();
        };

        struct gbuffer
        {
            unsigned int fbo;
            unsigned int texturePosition;
            unsigned int textureNormal;
            unsigned int textureDiffuse;
            unsigned int textureDepthColor;
            unsigned int textureLightSpacePosition;
            unsigned int textureDepth;

            gbuffer(int width, int height);
            ~gbuffer();
        }; 

        struct skybox
        {
            cubemap &texture;
            mesh &skyMesh;
            glm::vec3 scale = { 10.f, 10.f, 10.f };
            glm::mat4 transform;

            /** Updates the transform matrix according to 'scale' and a position. */
            void update(const glm::vec3 &cameraPosition);
        };

        struct render_data
        {
            math::transform &transform;
            mesh &mesh_;
            texture &texture_;
            shaders::geometry_shader_instance &shader;
            shaders::shadow_shader &shadowShader;
            const glm::mat4 &lightSpaceMatrix;
        };

        struct deferred_renderer
        {
            gbuffer buffer;
            sbuffer shadowBuffer;
            int width, height;
            bool debugBuffers;
            void begin();
            void render(camera &camera, const render_data &data);
            void sky(camera &camera, skybox &sky, shaders::skybox_shader_instance &shader);
            void light(shaders::screen_shader_instance &lightPassShader, gfx::mesh &quadMesh);
        };

        
    }
}

namespace std
{
    template<> struct hash<srd::core::gfx::vertex>
    {
        size_t operator()(srd::core::gfx::vertex const& vertex) const;
    };
}

#ifdef SRD_CORE_IMPLEMENTATION
// #pragma message "SRD_CORE_IMPLEMENTATION!"
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace srd::core
{
    void logError(const std::string &message)
    {
        std::cerr << "\033[0;31mError: " << message << "\033[0;0m" << std::endl;
    }

    char const* errorToString_(GLenum const err) noexcept
    {
        switch (err)
        {
        // opengl 2 errors (8)
        case GL_NO_ERROR:
        return "GL_NO_ERROR";

        case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";

        case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";

        case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";

        case GL_STACK_OVERFLOW:
        return "GL_STACK_OVERFLOW";

        case GL_STACK_UNDERFLOW:
        return "GL_STACK_UNDERFLOW";

        case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";

        // opengl 3 errors (1)
        case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";

        // gles 2, 3 and gl 4 error are handled by the switch above
        default:
        assert(!"unknown error");
        return nullptr;
        }
    }

    /** Checks for OpenGL errors. */
    void checkErrors_(const char *msg = 0)
    {
        auto e = glGetError();
        if(e != GL_NO_ERROR)
        {
            logError(std::string("OpenGL Error ") + "[" + msg + "]: " + errorToString_(e));
        }
    }

    namespace window
    {
#pragma region Window
        

        void log(const char *message)
        {
            std::cout << message << std::endl;
        }

        window::window(int width, int height, const char *title)
        {
            this->width = width;
            this->height = height;
            this->title = title;

            glfwInit();
            glfwSetErrorCallback([](int code, const char *message){ logError(message); });

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

            auto win = glfwCreateWindow(width, height, title, NULL, NULL);
            if(win == NULL)
            {
                logError("Failed to create GLFW window");
                glfwTerminate();
                return;
            }

            glfwMakeContextCurrent(win);

            if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            {
                logError("Failed to initialize GLAD");
                return;
            }

            this->win = win;
        }
    
        window::~window()
        {
            glfwDestroyWindow((GLFWwindow*)win);
            glfwTerminate();
        }

        bool window::keyPressed(int key)
        {
            return glfwGetKey((GLFWwindow*)win, key);
        }
#pragma endregion
    }

    namespace math
    {
        void transform::update()
        {
            auto scale = glm::scale(glm::mat4(1.f), this->scale);
            auto rotate = glm::mat4_cast(rotation);
            auto translate = glm::translate(glm::mat4(1.f), position);
            matrix = translate * rotate * scale;
        }
    };

    namespace gfx
    {
        bool vertex::operator==(const vertex &other) const
        {
            return position == other.position
                && normal   == other.normal
                && texcoord == other.texcoord;
        }
#pragma region Shader
        shader::shader(const std::string &vertexSource, const std::string &fragmentSource)
        {
            // std::cout << "shader ctor" << std::endl;
            // std::cout << "-------------  vertex shader  -------------" << std::endl;
            // std::cout << vertexSource << std::endl;
            // std::cout << "------------- fragment shader -------------" << std::endl;
            // std::cout << fragmentSource << std::endl;
            // std::cout << "-------------       end       -------------" << std::endl;

            auto checkShader = [](unsigned int shader, int type)
            {
                int  success;
                char infoLog[512];

                if(type < 2) glGetShaderiv (shader, GL_COMPILE_STATUS, &success);
                else         glGetProgramiv(shader,    GL_LINK_STATUS, &success);

                if(!success)
                {
                    if(type < 2) glGetShaderInfoLog(shader, 512, NULL, infoLog);
                    else         glGetProgramInfoLog(shader, 512, NULL, infoLog);
                    std::cerr << "\033[0;31m" << (type==0?"Vertex Shader":type==1?"Fragment Shader":"Program")
                        << " Compilation Failed!\033[0;0m\n" << infoLog << std::endl;
                }
            };

            auto vertexSourceC = vertexSource.c_str();
            auto fragmentSourceC = fragmentSource.c_str();

            auto vertexShader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertexShader, 1, &vertexSourceC, NULL);
            glCompileShader(vertexShader);
            checkShader(vertexShader, 0);

            auto fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragmentShader, 1, &fragmentSourceC, NULL);
            glCompileShader(fragmentShader);
            checkShader(fragmentShader, 1);

            id = glCreateProgram();
            glAttachShader(id, vertexShader);
            glAttachShader(id, fragmentShader);
            glLinkProgram(id);
            checkShader(id, 2);

            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            checkErrors_(__PRETTY_FUNCTION__);
        }

        namespace shaders
        {
            geometry_shader::geometry_shader(const std::string &vertexSource, const std::string &fragmentSource)
                : shader::shader(vertexSource, fragmentSource)
            {
                uniforms.transform           = getUniform("uTransform");
                uniforms.normalMatrix        = getUniform("uNormalMatrix");
                uniforms.texture0            = getUniform("uTexture");
                uniforms.materialData.tiling = getUniform("uMaterialData.tiling");
                uniforms.transformLightSpace = getUniform("uTransformLightSpace");
                setUniform(uniforms.materialData.tiling, glm::vec2(1, 1));
                setUniform(uniforms.texture0, 0);
            }

            shadow_shader::shadow_shader(const std::string &vertexSource, const std::string &fragmentSource)
                : shader::shader(vertexSource, fragmentSource)
            {
                uniforms.transformLightSpace = getUniform("uTransformLightSpace");
            }

            void geometry_shader_instance::use() const
            {
                type.use();
                type.setUniform(type.uniforms.materialData.tiling, uniforms.materialData.tiling);
            }

            screen_shader::screen_shader(const std::string &vertexSource, const std::string &fragmentSource)
                : shader::shader(vertexSource, fragmentSource)
            {
                uniforms.lighting.ambient     = getUniform("uLighting.ambient");
                uniforms.lighting.directional = getUniform("uLighting.directional");

                uniforms.texturePosition           = getUniform("gPosition");
                uniforms.textureNormal             = getUniform("gNormal");
                uniforms.textureDiffuse            = getUniform("gDiffuse");
                uniforms.textureDepthColor         = getUniform("gDepthColor");
                uniforms.textureShadowDepth        = getUniform("gShadowDepth");
                uniforms.texturePositionLightSpace = getUniform("gPositionLightSpace");

                setUniform(uniforms.texturePosition          , 0);
                setUniform(uniforms.textureNormal            , 1);
                setUniform(uniforms.textureDiffuse           , 2);
                setUniform(uniforms.textureDepthColor        , 3);
                setUniform(uniforms.textureShadowDepth       , 4);
                setUniform(uniforms.texturePositionLightSpace, 5);

                uniforms.debug.showShadowMap = getUniform("uDebug_ShowShadowMap");
                setUniform(uniforms.debug.showShadowMap, 0);
            }

            void screen_shader_instance::use() const
            {
                type.use();
                type.setUniform(type.uniforms.lighting.ambient, uniforms.lighting.ambient);
                type.setUniform(type.uniforms.lighting.directional, uniforms.lighting.directional);
                type.setUniform(type.uniforms.debug.showShadowMap, uniforms.debug.showShadowMap);
            }

            skybox_shader::skybox_shader(const std::string &vertexSource, const std::string &fragmentSource)
                : shader::shader(vertexSource, fragmentSource)
            {
                uniforms.texture0 = getUniform("uTexture0");
                uniforms.texture1 = getUniform("uTexture1");
                uniforms.transform = getUniform("uTransform");
                uniforms.tint       = getUniform("uTint");
                uniforms.transition = getUniform("uTransition");

                setUniform(uniforms.texture0, 0);
                setUniform(uniforms.texture1, 0);
            }

            void skybox_shader_instance::use() const
            {
                type.use();
                type.setUniform(type.uniforms.tint      , uniforms.tint);
                type.setUniform(type.uniforms.transition, uniforms.transition);
            }
        }

        void shader::use() const
        {
            glUseProgram(id);
        }

        shader::~shader()
        {
            glDeleteProgram(id);
        }

        int shader::getUniform(const std::string &name) const
        {
            auto x = glGetUniformLocation(id, name.c_str());
            if(x < 0) { std::cerr << "\033[0;31mNo uniform called " << name << " exists\033[0;0m" << std::endl; }
            return x;
        }
        void shader::setUniform(int location, int value) const
        {
            if(location < 0) return;
            glUseProgram(id);
            glUniform1i(location, value);
        }
        void shader::setUniform(int location, bool value) const
        {
            if(location < 0) return;
            glUseProgram(id);
            glUniform1i(location, value ? 1 : 0);
        }
        void shader::setUniform(int location, float value) const
        {
            if(location < 0) return;
            glUseProgram(id); 
            glUniform1f(location, value);
        }
        void shader::setUniform(int location, const glm::vec4 &value) const
        {
            if(location < 0) return;
            glUseProgram(id);
            glUniform4f(location, value.x, value.y, value.z, value.w);
        }
        void shader::setUniform(int location, const glm::quat &value) const
        {
            if(location < 0) return;
            glUseProgram(id);
            glUniform4f(location, value.x, value.y, value.z, value.w);
        }
        void shader::setUniform(int location, const glm::vec3 &value) const
        {
            if(location < 0) return;
            glUseProgram(id);
            glUniform3f(location, value.x, value.y, value.z);
        }
        void shader::setUniform(int location, const glm::vec2 &value) const
        {
            if(location < 0) return;
            glUseProgram(id);
            glUniform2f(location, value.x, value.y);
        }
        void shader::setUniform(int location, const glm::mat4 &value) const
        {
            if(location < 0) return;
            glUseProgram(id);
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
        }
        void shader::setUniform(int location, const glm::mat3 &value) const
        {
            if(location < 0) return;
            glUseProgram(id);
            glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
        }

        

#pragma endregion
#pragma region Mesh
        mesh::mesh(const std::vector<vertex> &vertices, const std::vector<unsigned int> &indices)
        {
            // std::cout << "mesh ctor" << std::endl;
            elementCount = 0;
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);

            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER,
                vertices.size() * sizeof(vertex),
                &vertices[0],
                GL_STATIC_DRAW
            );

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                indices.size() * sizeof(unsigned int),
                &indices[0],
                GL_STATIC_DRAW
            );

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)0);

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)((3)     * sizeof(float)));

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)((3+3)   * sizeof(float)));

            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)((3+3+2) * sizeof(float)));

            glBindVertexArray(0);

            elementCount = indices.size();

            checkErrors_(__PRETTY_FUNCTION__);
        }

        void mesh::bind() const
        {
            glBindVertexArray(vao);
            checkErrors_(__PRETTY_FUNCTION__);
        }

        mesh::~mesh()
        {
            glDeleteBuffers(1, &vbo);
            glDeleteBuffers(1, &ebo);
            glDeleteVertexArrays(1, &vao);
        }
#pragma endregion
#pragma region Texture
        texture::texture(const texture::data &data)
        {
            // std::cout << "texture ctor" << std::endl;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            if(data.data)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data);
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            checkErrors_(__PRETTY_FUNCTION__);
        }

        void texture::bind(int unit)
        {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, id);
        }

        cubemap::cubemap(const texture::data &xPos,
                         const texture::data &xNeg,
                         const texture::data &yPos,
                         const texture::data &yNeg,
                         const texture::data &zPos,
                         const texture::data &zNeg)
        {
            static int types[] =
            {
                GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_X,

                GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                
                GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            const texture::data *datas[] =
            {
                &xPos, &xNeg, &yPos, &yNeg, &zPos, &zNeg
            };

            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_CUBE_MAP, id);

            for(int i = 0; i < 6; ++i)
            {
                if(!datas[i]->data) {
                    logError(std::string("Bad texture for a cubemap's ") + "XXYYZZ"[i] + "+-+-+-"[i] + " (=null)");
                }
                //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data);
                glTexImage2D(types[i], 0, GL_RGB, datas[i]->width, datas[i]->height, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, datas[i]->data);

                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            }
            checkErrors_(__PRETTY_FUNCTION__);
        }

        void cubemap::bind(int unit)
        {
            glActiveTexture(unit);
            checkErrors_("before cube map");
            glBindTexture(GL_TEXTURE_CUBE_MAP, id);
            checkErrors_(__PRETTY_FUNCTION__);
        }

        cubemap::~cubemap()
        {
            glDeleteTextures(1, &id);
        }
#pragma endregion
#pragma region Camera
        camera::camera(int width, int height, float near, float far)
        {
            // projMatrix = glm::mat4(1.f);
            projMatrix = glm::perspective(glm::pi<float>() * 0.25f, (float)width / (float)height, near, far);
        }

        void camera::resize(int width, int height)
        {
            projMatrix = glm::perspective(glm::pi<float>() * 0.25f, (float)width / (float)height, 0.05f, 100.f);
        }

        void camera::update()
        {
            transform.update();

            glm::vec3 const f(glm::normalize(glm::vec3(0, 0, 1) * transform.rotation));
            glm::vec3 const s(glm::normalize(glm::cross(f, glm::vec3(0, 1, 0))));
            glm::vec3 const u(glm::cross(s, f));

            viewMatrix = glm::mat4(1.f);
            viewMatrix[0][0] =  s.x;
            viewMatrix[1][0] =  s.y;
            viewMatrix[2][0] =  s.z;
            viewMatrix[0][1] =  u.x;
            viewMatrix[1][1] =  u.y;
            viewMatrix[2][1] =  u.z;
            viewMatrix[0][2] = -f.x;
            viewMatrix[1][2] = -f.y;
            viewMatrix[2][2] = -f.z;
            viewMatrix[3][0] = -dot(s, transform.position);
            viewMatrix[3][1] = -dot(u, transform.position);
            viewMatrix[3][2] =  dot(f, transform.position);
        }

        glm::mat4 camera::projection(const glm::mat4 &model)
        {   
            return projMatrix * viewMatrix * model;
        }
#pragma endregion

        void forward_renderer::render(camera &camera,
                                      math::transform &transform,
                                      mesh &mesh,
                                      texture &texture,
                                      shaders::geometry_shader_instance &shader)
        {
            //? transform.update();
            mesh.bind();
            shader.use();
            texture.bind(0);
            shader.type.setUniform(shader.type.uniforms.transform, camera.projection(transform.matrix));
            glDrawElements(GL_TRIANGLES, mesh.elementCount, GL_UNSIGNED_INT, 0);
        }

        sbuffer::sbuffer(int width, int height)
        {
            this->width = width;
            this->height = height;
            glGenFramebuffers(1, &fbo);
            glGenTextures(1, &depthTexture);
            glBindTexture(GL_TEXTURE_2D, depthTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 
                         width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);

            if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                std::cout << "Shadow Framebuffer not complete!" << std::endl;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        sbuffer::~sbuffer()
        {
            glDeleteFramebuffers(1, &fbo);
        }
        
        gbuffer::gbuffer(int width, int height)
        {
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            
            // position color buffer
            glGenTextures(1, &texturePosition);
            glBindTexture(GL_TEXTURE_2D, texturePosition);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texturePosition, 0);
            
            // normal color buffer
            glGenTextures(1, &textureNormal);
            glBindTexture(GL_TEXTURE_2D, textureNormal);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textureNormal, 0);
            
            // diffuse color buffer
            glGenTextures(1, &textureDiffuse);
            glBindTexture(GL_TEXTURE_2D, textureDiffuse);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, textureDiffuse, 0);

            // depth color buffer
            glGenTextures(1, &textureDepthColor);
            glBindTexture(GL_TEXTURE_2D, textureDepthColor);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, textureDepthColor, 0);

            // light space position color buffer
            glGenTextures(1, &textureLightSpacePosition);
            glBindTexture(GL_TEXTURE_2D, textureLightSpacePosition);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, textureLightSpacePosition, 0);
            
            unsigned int attachments[5] = {
                GL_COLOR_ATTACHMENT0,
                GL_COLOR_ATTACHMENT1,
                GL_COLOR_ATTACHMENT2,
                GL_COLOR_ATTACHMENT3,
                GL_COLOR_ATTACHMENT4,
            };

            glDrawBuffers(5, attachments);

            glGenRenderbuffers(1, &textureDepth);
            glBindRenderbuffer(GL_RENDERBUFFER, textureDepth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, textureDepth);

            if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::cerr << "Framebuffer not complete!" << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            checkErrors_(__PRETTY_FUNCTION__);
        }

        gbuffer::~gbuffer()
        {
            glDeleteFramebuffers(1, &fbo);
        }

        void deferred_renderer::begin()
        {
            glEnable(GL_DEPTH_TEST);
            glClearColor(0, 0, 0, 1);
            glViewport(0, 0, shadowBuffer.width, shadowBuffer.height);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowBuffer.fbo);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_DEPTH_BUFFER_BIT);

            // glClearColor(0.2, 0.3, 0.5, 1);
            glViewport(0, 0, width, height);
            glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo);
            glEnable(GL_DEPTH_TEST);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        void deferred_renderer::render(camera &camera, const render_data &data)
        {
            // glCullFace(GL_FRONT);
            // glDisable(GL_STENCIL_TEST);
            // glEnable(GL_DEPTH_TEST);
            // glDepthMask(GL_TRUE);

            glViewport(0, 0, shadowBuffer.width, shadowBuffer.height);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowBuffer.fbo);
            data.mesh_.bind();
            data.shadowShader.use();
            data.shadowShader.setUniform(
                data.shadowShader.uniforms.transformLightSpace,
                data.lightSpaceMatrix * data.transform.matrix
            );
            glDrawElements(GL_TRIANGLES, data.mesh_.elementCount, GL_UNSIGNED_INT, 0);

            glCullFace(GL_BACK);
            glViewport(0, 0, width, height);
            glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo);
            data.mesh_.bind();
            data.shader.use();
            data.texture_.bind(0);
            glm::mat4 normalMatrix = data.transform.matrix;
            data.shader.type.setUniform(data.shader.type.uniforms.transformLightSpace, data.lightSpaceMatrix * data.transform.matrix);
            data.shader.type.setUniform(data.shader.type.uniforms.normalMatrix, normalMatrix);
            data.shader.type.setUniform(data.shader.type.uniforms.transform, camera.projection(data.transform.matrix));
            glDrawElements(GL_TRIANGLES, data.mesh_.elementCount, GL_UNSIGNED_INT, 0);
        }

        void deferred_renderer::sky(camera &camera, skybox &sky, shaders::skybox_shader_instance &shader)
        {
            glCullFace(GL_FRONT);
            glDepthFunc(GL_LEQUAL);
            glViewport(0, 0, width, height);
            glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo);
            checkErrors_("sky: before bind");

            // glm::mat3 rot = glm::mat3_cast(camera.transform.rotation);
            // glm::mat4 tr  = camera.transform.matrix;
            // tr

            sky.skyMesh.bind();
            shader.use();
            sky.texture.bind(GL_TEXTURE0); //  * sky.transform
            shader.type.setUniform(
                shader.type.uniforms.transform,
                camera.projMatrix *
                camera.viewMatrix *
                camera.transform.matrix *
                glm::mat4(glm::inverse(glm::mat3_cast(camera.transform.rotation))));

            glDrawElements(GL_TRIANGLES, sky.skyMesh.elementCount, GL_UNSIGNED_INT, 0);
            glDepthFunc(GL_LESS);
            checkErrors_(__PRETTY_FUNCTION__);
        }

        void deferred_renderer::light(shaders::screen_shader_instance &lightPassShader, gfx::mesh &quadMesh)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);
            // glDepthMask(GL_FALSE);
            // glEnable(GL_STENCIL_TEST);
            // glStencilMask(0xFF);
            // glStencilFunc(GL_ALWAYS, 1, 0xFF);
            // glStencilMask(0xFF);
            // glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); 

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, buffer.texturePosition);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, buffer.textureNormal);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, buffer.textureDiffuse);

            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, buffer.textureDepthColor);

            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, shadowBuffer.depthTexture);

            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, buffer.textureLightSpacePosition);

            quadMesh.bind();
            lightPassShader.use();
            glDrawElements(GL_TRIANGLES, quadMesh.elementCount, GL_UNSIGNED_INT, 0);
        }

        void skybox::update(const glm::vec3 &cameraPosition)
        {
            // transform = glm::mat4(1.0f);
            // transform = glm::translate(glm::mat4(1.0f), cameraPosition);
            transform = glm::scale(glm::mat4(1.0f), scale);
        }

        
    }
}

size_t std::hash<srd::core::gfx::vertex>::operator()(srd::core::gfx::vertex const& vertex) const {
    return ((hash<glm::vec3>()(vertex.position) ^
            (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
            (hash<glm::vec2>()(vertex.texcoord) << 1);
}


namespace srd::core::window
{
    void show(window &r,
              callable<float> auto update,
              callable<int, int> auto resize, bool bind_resize,
              callable<int, int, int, int> auto keypress, bool bind_keypress,
              callable auto before_render,
              callable auto after_render)
    {
        static void *func_resize_pointer = nullptr;
        static void *func_key_pointer = nullptr;

        auto win = (GLFWwindow*)r.win;
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_CULL_FACE);

        auto titleLength = std::strlen(r.title);
        float lastTime = 0.f, lastFpsTime = 0.f;
        int frameCount = 0;

        func_resize_pointer = &resize;
        func_key_pointer = &keypress;

        {
            int w, h;
            glfwGetFramebufferSize(win, &w, &h);
            resize(w, h);
        }

        if(bind_resize)
            glfwSetFramebufferSizeCallback(win, [](GLFWwindow* win, int width, int height){
                glViewport(0, 0, width, height);
                (*(decltype(resize)*)func_resize_pointer)(width, height);
            });
        
        if(bind_keypress)
            glfwSetKeyCallback(win, [](GLFWwindow* window, int key, int scancode, int action, int mods){
                // const char *key_name = glfwGetKeyName(key, 0);
                // if(action == GLFW_PRESS) printf("Pressed %s!\n", key_name);
                // if(action == GLFW_RELEASE) printf("Released %s!\n", key_name);
                (*(decltype(keypress)*)func_key_pointer)(key, scancode, action, mods);
            });

        glClearColor(0.f, 0.f, 0.f, 1.f);
        while(!glfwWindowShouldClose(win))
        {
            before_render();
            float currentTime = glfwGetTime();
            float delta = currentTime - lastTime;

            // if((currentTime - lastFpsTime) >= 1) {
            //     std::string title;
            //     title.reserve(titleLength + 2 + 5 + 4);
            //     title += r.title;
            //     title += " ("; 
            //     title += std::to_string(frameCount);
            //     title += " FPS)";
            //     glfwSetWindowTitle(win, title.c_str()); // The title
            //     frameCount = 0;
            //     lastFpsTime = currentTime;
            // } else {
            //     ++frameCount;
            // }
            
            update(delta);
            
            after_render();

            glfwSwapBuffers(win);
            glfwPollEvents();

            lastTime = currentTime;
        }
    }
}

#endif
#endif // CORE
