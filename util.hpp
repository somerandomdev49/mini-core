#ifndef UTIL
#define UTIL
#include "log.hpp"
#include <stb_image.h>
#include <tiny_obj_loader.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include "core.hpp"

std::string readFile(const char *path)
{
    using namespace srd;
    std::string s;
    std::ifstream ifs(path);
    if(!ifs.is_open()) {
        log::cerr << "Could not read file at '" << path << "'!" << log::endl;
        return "";
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string str = ss.str();
    ifs.close();
    return str;
}

srd::core::gfx::texture::data readTexture(const std::string &path, bool flip = false)
{
    using namespace srd;
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(flip ? 0 : 1);
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
    if(!data)
    {
        log::cerr << "Failed to read image at '" << path << "'!"<< log::endl;
        std::cout << "Reason:\n  " << stbi_failure_reason() << std::endl;
    }
    return { .width = width, .height = height, .channels = nrChannels, .data = data };
}

void deleteTexture(const srd::core::gfx::texture::data &data)
{
    stbi_image_free(data.data);
}

void readMesh(const char *path, std::vector<srd::core::gfx::vertex> &vertices, std::vector<unsigned int> &indices)
{
    using namespace srd;
    std::ifstream ifs { path };
    if(!ifs.is_open())
    {
        log::cerr << "Could not load mesh at '" << path << "'!" << log::endl;
        return;
    }

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::unordered_map<core::gfx::vertex, unsigned int> vertexMap;

    std::string warn;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &ifs);

    if(!warn.empty()) {
        log::cwrn << "Warning: " << warn << log::endl;
    }

    if(!err.empty()) {
        log::cerr << "Error: " << err << log::endl;
        // std::printf("\033[0;31mError: '%s'!\033[0;0m\n", path.c_str());
        ifs.close();
        return;
    }

    if(!ret) {
        ifs.close();
        return;
    }

    for(const auto& shape : shapes) {
        for(const auto& index : shape.mesh.indices) {
            core::gfx::vertex vertex{};

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2],
            };

            vertex.texcoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                attrib.texcoords[2 * index.texcoord_index + 1]
            };

            if (vertexMap.count(vertex) == 0) {
                vertexMap[vertex] = static_cast<unsigned int>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(vertexMap[vertex]);
        }
    }

    // bool shouldLog = indices.size() < 100;
    log::cout << "Mesh info: index count: " << indices.size() << ", vertex count: " << vertices.size() << log::endl;
    
    // if(shouldLog) log::sec(log::call);

    // for(unsigned int i = 0; i < indices.size(); i += 3)
    // {
    //     if(shouldLog)
    //         log::cout << "Generating tangents for " << i << ", " << i+1 << ", " << i+2 << log::endl;
    //     auto &x0 = vertices[indices[i]];
    //     auto &x1 = vertices[indices[i+1]];
    //     auto &x2 = vertices[indices[i+2]];

    //     auto &v0 = x0.position;
    //     auto &v1 = x1.position;
    //     auto &v2 = x2.position;

    //     auto &uv0 = x0.texcoord;
    //     auto &uv1 = x1.texcoord;
    //     auto &uv2 = x2.texcoord;

    //     glm::vec3 deltaPos1 = v1-v0;
    //     glm::vec3 deltaPos2 = v2-v0;

    //     // UV delta
    //     glm::vec2 deltaUV1 = uv1-uv0;
    //     glm::vec2 deltaUV2 = uv2-uv0;

    //     float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
    //     glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
    //     // glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x)*r;

    //     x0.tangent = tangent;
    //     x1.tangent = tangent;
    //     x2.tangent = tangent;
    // }

    // if(shouldLog) log::endsec(log::call);

    // for(unsigned int i = 0; i < vertices.size(); i++)
    //     vertices[i].tangent = glm::normalize(
    //         vertices[i].tangent
    //       - vertices[i].normal * glm::dot(vertices[i].normal, vertices[i].tangent));//glm::normalize(vertices[i].tangent);
}

#endif // UTIL
