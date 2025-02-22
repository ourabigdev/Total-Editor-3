#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "raylib.h"

#include <filesystem>

// Loads an .obj file into a Raylib model. Unlike the default model loader,
// this one loads in indices as well.
Model LoadOBJModelButBetter(const std::filesystem::path& path);

#endif