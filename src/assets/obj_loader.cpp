/**
 * Copyright (c) 2022-present Alexander Lunsford
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#include "obj_loader.hpp"

#include "raymath.h"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <stdint.h>

#include "../text_util.hpp"
#include "../c_helpers.hpp"
#include "../tile.hpp"

// Returns the model with an error message.
static Model LoadOBJModelFromStream(std::istream& stream)
{
    struct Vertex 
    {
        Vector3 pos;
        Vector2 uv;
        Vector3 norm;
    };

    struct OBJMesh 
    {
        std::vector<Vertex> verts;
        std::vector<uint16_t> indices;

        // This takes a triplet string from the .obj face and points it to a vertex in `verts`.
        std::map<std::string, size_t> mapper;
    };

    std::array<OBJMesh, TEXTURES_PER_TILE> objMeshes;

    // These track the vertex data as laid out in the .obj file
    // (Since attributes are reused between vertices, they don't map directly onto the mesh's attributes)
    std::vector<Vector3> objPositions;
    objPositions.reserve(128);
    std::vector<Vector2> objUVs;
    objUVs.reserve(128);
    std::vector<Vector3> objNormals;
    objNormals.reserve(128);

    // Parse the .obj file
    std::string line;
    std::string objectName;

    size_t meshIndex = 0;

    while (std::getline(stream, line), !line.empty())
    {
        // Get the items between spaces
        std::vector<std::string> tokens = SplitString(line, " ");
        if (tokens.empty()) continue;

        if (tokens[0].compare("v") == 0) // Vertex positions
        {
            objPositions.push_back(
                Vector3 {
                    std::stof(tokens[1]),
                    std::stof(tokens[2]),
                    std::stof(tokens[3]),
                });
        }
        else if (tokens[0].compare("vt") == 0) // Vertex texture coordinates
        {
            objUVs.push_back(
                Vector2 {
                    std::stof(tokens[1]),
                    1.0f - std::stof(tokens[2]),
                });
        }
        else if (tokens[0].compare("vn") == 0) // Vertex normals
        {
            objNormals.push_back(
                Vector3 {
                    std::stof(tokens[1]),
                    std::stof(tokens[2]),
                    std::stof(tokens[3]),
                });
        }
        else if (tokens[0].compare("f") == 0) // Faces
        {
            if (tokens.size() > 4)
            {
                throw "shape should be triangulated";
            }
            
            for (size_t i = 1; i < tokens.size(); ++i)
            {
                std::vector<std::string> indices = SplitString(tokens[i], "/");
                if (indices.size() != 3 || indices[0].empty() || indices[1].empty() || indices[2].empty())
                {
                    throw "face does not have the right number of indices";
                }
                
                int posIdx = std::stoi(indices[0]) - 1,
                    uvIdx = std::stoi(indices[1]) - 1,
                    normIdx = std::stoi(indices[2]) - 1;
                if (posIdx < 0 || uvIdx < 0 || normIdx < 0)
                {
                    throw "negative indices";
                }

                // Add indices to mesh
                std::map<std::string, size_t>& mapper = objMeshes[meshIndex].mapper;
                if (mapper.find(tokens[i]) != mapper.end())
                {
                    objMeshes[meshIndex].indices.push_back(mapper[tokens[i]]);
                }
                else
                {
                    size_t numVerts = objMeshes[meshIndex].verts.size();
                    mapper[tokens[i]] = numVerts;
                    objMeshes[meshIndex].indices.push_back(numVerts);
                    objMeshes[meshIndex].verts.push_back(
                        Vertex { 
                            objPositions[posIdx], 
                            objUVs[uvIdx], 
                            objNormals[normIdx]
                        });
                }
            }
        }
        else if (tokens[0].compare("o") == 0) // Objects
        {
            if (objectName.empty())
            {
                objectName = tokens[1];
            }
            else
            {
                break; // Only parse the first object in the file.
            }
        }
        else if (tokens[0].compare("usemtl") == 0) // Material switch
        {
            if (tokens.size() > 1 && StringToLower(tokens[1]) == "secondary")
            {
                meshIndex = 1;
            }
            else
            {
                meshIndex = 0;
            }
        }
    }

    // Initialize the model with the meshes
    Model model = {}; 
    model.bindPose = nullptr;
    model.boneCount = 0;
    model.bones = nullptr;
    model.materialCount = objMeshes.size();
    model.materials = SAFE_MALLOC(Material, model.materialCount);
    model.meshCount = model.materialCount;
    model.meshes = SAFE_MALLOC(Mesh, model.meshCount);
    model.meshMaterial = SAFE_MALLOC(int, model.meshCount);
    model.transform = MatrixIdentity();

    for (size_t m = 0; m < objMeshes.size(); ++m)
    {
        model.materials[m] = LoadMaterialDefault();
        model.meshMaterial[m] = m;
    
        const std::vector<Vertex>& meshVerts = objMeshes[m].verts;
        const std::vector<uint16_t>& meshInds = objMeshes[m].indices;

        // Encode the dynamic vertex arrays into a Raylib mesh
        Mesh mesh = {};
        mesh.vertices = SAFE_MALLOC(float, meshVerts.size() * 3);
        mesh.vertexCount = meshVerts.size();
        mesh.texcoords = SAFE_MALLOC(float, meshVerts.size() * 2);
        mesh.texcoords2 = mesh.animNormals = mesh.animVertices = mesh.boneWeights = mesh.tangents = nullptr;
        mesh.boneIds = mesh.colors = nullptr;
        mesh.normals = SAFE_MALLOC(float, meshVerts.size() * 3);
        mesh.indices = SAFE_MALLOC(uint16_t, meshInds.size());
        mesh.triangleCount = meshInds.size() / 3;
        mesh.vaoId = 0, mesh.vboId = 0;
    
        for (size_t v = 0, p = 0, n = 0, u = 0; v < meshVerts.size(); ++v)
        {
            mesh.vertices[p++] = meshVerts[v].pos.x;
            mesh.vertices[p++] = meshVerts[v].pos.y;
            mesh.vertices[p++] = meshVerts[v].pos.z;
            mesh.normals[n++] = meshVerts[v].norm.x;
            mesh.normals[n++] = meshVerts[v].norm.y;
            mesh.normals[n++] = meshVerts[v].norm.z;
            mesh.texcoords[u++] = meshVerts[v].uv.x;
            mesh.texcoords[u++] = meshVerts[v].uv.y;
        }
        for (size_t i = 0; i < meshInds.size(); ++i)
        {
            mesh.indices[i] = meshInds[i];
        }
        UploadMesh(&mesh, false);
        
        model.meshes[m] = mesh;
    }

    return model;
}

Model LoadOBJModelButBetter(const std::filesystem::path& path)
{
    std::ifstream objFile(path);

    if (!objFile.is_open())
    {
        std::cerr << "Failed to load file " << path << std::endl;
        return {};
    }

    try
    {
        return LoadOBJModelFromStream(objFile);
    }
    catch(const char* message)
    {
        std::cerr << "Error loading obj file from " << path << ": " << message << std::endl;
        return {};
    }
}

Model LoadOBJModelFromString(const std::string stringContents)
{
    std::stringstream stream(stringContents);

    try 
    {
        return LoadOBJModelFromStream(stream);
    }
    catch (const char* message)
    {
        std::cerr << "Error loading OBJ model from string: " << message << std::endl;
        return {};
    }
}