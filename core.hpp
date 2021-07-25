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

                struct
                {
                    int showShadowMap;
                } debug;
            } uniforms;

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

        struct shader_instance
        {
            shader &type;
            struct
            {
                struct
                {
                    glm::vec2 tiling;
                } materialData;
            } uniforms;
            
            /** Binds the shader and sets all of the necessary uniforms. */
            void use() const;
        };

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
            glm::mat4 projection(const math::transform &model);
        };

        struct forward_renderer
        {
            void render(camera &camera,
                        math::transform &transform,
                        mesh &mesh,
                        texture &texture,
                        shader_instance &shader);
        };

        /** Shadow Framebuffer. */
        struct sbuffer
        {
            unsigned int fbo;
            unsigned int depthTexture;
            int width, height;

            sbuffer(int width, int height);
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
            // ~gbuffer();
        };  

        struct deferred_renderer
        {
            gbuffer buffer;
            sbuffer shadowBuffer;
            int width, height;
            bool debugBuffers;
            void begin();
            void render(camera &camera,
                        math::transform &transform,
                        mesh &mesh,
                        texture &texture,
                        shader_instance &shader,
                        shader_instance &shadowShader,
                        const glm::mat4 &lightSpaceMatrix);

            void light(gfx::shader &lightPassShader, gfx::mesh &quadMesh);
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
    namespace window
    {
#pragma region Window
        void logError(const char *message)
        {
            std::cerr << "\033[0;31mError: " << message << "\033[0;0m" << std::endl;
        }

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

            uniforms.transform = getUniform("uTransform");
            uniforms.normalMatrix = getUniform("uNormalMatrix");
            uniforms.texture0 = getUniform("uTexture");
            uniforms.materialData.tiling = getUniform("uMaterialData.tiling");
            uniforms.transformLightSpace = getUniform("uTransformLightSpace");
            uniforms.debug.showShadowMap = getUniform("uDebug_ShowShadowMap");
            setUniform(uniforms.materialData.tiling, glm::vec2(1, 1));
            setUniform(uniforms.texture0, 0);

            setUniform(uniforms.debug.showShadowMap, 0);

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
            return glGetUniformLocation(id, name.c_str());
        }
        void shader::setUniform(int location, int value) const
        {
            glUseProgram(id);
            glUniform1i(location, value);
        }
        void shader::setUniform(int location, bool value) const
        {
            glUseProgram(id);
            glUniform1i(location, value ? 1 : 0);
        }
        void shader::setUniform(int location, float value) const
        {
            glUseProgram(id);
            glUniform1f(location, value);
        }
        void shader::setUniform(int location, const glm::vec4 &value) const
        {
            glUseProgram(id);
            glUniform4f(location, value.x, value.y, value.z, value.w);
        }
        void shader::setUniform(int location, const glm::quat &value) const
        {
            glUseProgram(id);
            glUniform4f(location, value.x, value.y, value.z, value.w);
        }
        void shader::setUniform(int location, const glm::vec3 &value) const
        {
            glUseProgram(id);
            glUniform3f(location, value.x, value.y, value.z);
        }
        void shader::setUniform(int location, const glm::vec2 &value) const
        {
            glUseProgram(id);
            glUniform2f(location, value.x, value.y);
        }
        void shader::setUniform(int location, const glm::mat4 &value) const
        {
            glUseProgram(id);
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
        }
        void shader::setUniform(int location, const glm::mat3 &value) const
        {
            glUseProgram(id);
            glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
        }

        void shader_instance::use() const
        {
            type.use();
            type.setUniform(type.uniforms.materialData.tiling, uniforms.materialData.tiling);
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
        }

        void mesh::bind() const
        {
            glBindVertexArray(vao);
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
        }

        void texture::bind(int unit)
        {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, id);
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

        glm::mat4 camera::projection(const math::transform &model)
        {   
            return projMatrix * viewMatrix * model.matrix; // viewMatrix
        }
#pragma endregion

        void forward_renderer::render(camera &camera,
                                      math::transform &transform,
                                      mesh &mesh,
                                      texture &texture,
                                      shader_instance &shader)
        {
            //? transform.update();
            mesh.bind();
            shader.use();
            texture.bind(0);
            shader.type.setUniform(shader.type.uniforms.transform, camera.projection(transform));
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
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

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
        
        gbuffer::gbuffer(int width, int height)
        {
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            
            // - position color buffer
            glGenTextures(1, &texturePosition);
            glBindTexture(GL_TEXTURE_2D, texturePosition);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texturePosition, 0);
            
            // - normal color buffer
            glGenTextures(1, &textureNormal);
            glBindTexture(GL_TEXTURE_2D, textureNormal);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textureNormal, 0);
            
            // - diffuse color buffer
            glGenTextures(1, &textureDiffuse);
            glBindTexture(GL_TEXTURE_2D, textureDiffuse);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, textureDiffuse, 0);

            // - depth color buffer
            glGenTextures(1, &textureDepthColor);
            glBindTexture(GL_TEXTURE_2D, textureDepthColor);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, textureDepthColor, 0);

            // - light space position color buffer
            glGenTextures(1, &textureLightSpacePosition);
            glBindTexture(GL_TEXTURE_2D, textureLightSpacePosition);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, textureLightSpacePosition, 0);
            
            // - tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
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
        }
        
        void deferred_renderer::begin()
        {
            glEnable(GL_DEPTH_TEST);
            // glClearColor(0, 0, 0, 1);
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

        void deferred_renderer::render(camera &camera,
                                      math::transform &transform,
                                      mesh &mesh,
                                      texture &texture,
                                      shader_instance &shader,
                                      shader_instance &shadowShader,
                                      const glm::mat4 &lightSpaceMatrix)
        {
            // glCullFace(GL_FRONT);
            glViewport(0, 0, shadowBuffer.width, shadowBuffer.height);
            glBindFramebuffer(GL_FRAMEBUFFER, shadowBuffer.fbo);
            mesh.bind();
            shadowShader.use();
            shadowShader.type.setUniform(
                shadowShader.type.uniforms.transformLightSpace,
                lightSpaceMatrix * transform.matrix
            );
            glDrawElements(GL_TRIANGLES, mesh.elementCount, GL_UNSIGNED_INT, 0);

            glCullFace(GL_BACK);
            glViewport(0, 0, width, height);
            glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo);
            mesh.bind();
            shader.use();
            texture.bind(0);
            glm::mat4 normalMatrix = transform.matrix;
            shader.type.setUniform(shader.type.uniforms.transformLightSpace, lightSpaceMatrix * transform.matrix);
            shader.type.setUniform(shader.type.uniforms.normalMatrix, normalMatrix);
            shader.type.setUniform(shader.type.uniforms.transform, camera.projection(transform));
            glDrawElements(GL_TRIANGLES, mesh.elementCount, GL_UNSIGNED_INT, 0);
        }

        void deferred_renderer::light(gfx::shader &lightPassShader, gfx::mesh &quadMesh)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

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
            lightPassShader.setUniform(lightPassShader.uniforms.debug.showShadowMap, this->debugBuffers);
            glDrawElements(GL_TRIANGLES, quadMesh.elementCount, GL_UNSIGNED_INT, 0);
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
              callable<int, int> auto resize,
              callable<int, int, int, int> auto keypress)
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

        glfwSetFramebufferSizeCallback(win, [](GLFWwindow* win, int width, int height){
            glViewport(0, 0, width, height);
            (*(decltype(resize)*)func_resize_pointer)(width, height);
        });
        
        glfwSetKeyCallback(win, [](GLFWwindow* window, int key, int scancode, int action, int mods){
            // const char *key_name = glfwGetKeyName(key, 0);
            // if(action == GLFW_PRESS) printf("Pressed %s!\n", key_name);
            // if(action == GLFW_RELEASE) printf("Released %s!\n", key_name);
            (*(decltype(keypress)*)func_key_pointer)(key, scancode, action, mods);
        });

        glClearColor(0.f, 0.f, 0.f, 1.f);
        while(!glfwWindowShouldClose(win))
        {
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
            
            glfwSwapBuffers(win);
            glfwPollEvents();

            lastTime = currentTime;
        }
    }
}

#endif
#endif // CORE
