#pragma once
#include <DirectXMath.h>
#include <vector>
#include "Shapes.h"
#include "../../ECS/ECSTypes.h"

namespace Collision
{

        enum ECollisionType
        {
                ENoCollision,
                EOveralap,
                ECollide
        };

        struct FContactPoint
        {
                DirectX::XMVECTOR position;
                DirectX::XMVECTOR normal;
        };
        struct FOverlapResult
        {
                //ECollisionType             collisionType;
                std::vector<FContactPoint> collideSurfaces;
               // DirectX::XMVECTOR          finalPosition;
                bool hasOverlap;
        };

        struct FAdvancedCollisionResult
        {
                ECollisionType             collisionType;
                std::vector<FContactPoint> collideSurfaces;
                DirectX::XMVECTOR          finalPosition;
                DirectX::XMVECTOR          finalDirection;
        };

		struct FCollisionQueryResult
        {
                std::vector<ComponentHandle> spheres;
                std::vector<ComponentHandle> AABBs;
                std::vector<ComponentHandle> capsules;
        };

}; // namespace Collision