#pragma once

#include <string>

#include <vector>

namespace FileIO
{
        struct FTextureDesc
        {
                //ETextureType textureType;
                file_path_t  filePath;

                /*inline bool operator==(const TextureDesc& other)
                {
                        return textureType == other.textureType;
                }*/
        };

        struct FMaterialData
        {
                std::string name;
                // EMaterialType            surfaceType;
                // FSurfaceProperties       surfaceProperties;
                std::vector<FTextureDesc> textureDescs;
        };
} // namespace FileIO