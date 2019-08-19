#pragma once

#include "../../ECS/HandleManager.h"
#include "../../ECS/SystemManager.h"
#include "../MathLibrary/MathLibrary.h"
#include "GoalComponent.h"
#include "..//ResourceManager/IResource.h"

class TransformComponent;
class ResourceManager;

class OrbitSystem : public ISystem
{
    private:
        HandleManager* m_HandleManager;

		ResourceManager* m_ResourceManager;

        const char* materialNames[3] = {"GlowMatPlanet01", "GlowMatPlanet03", "GlowMatPlanet02"};
        int         m_Stage          = 0;
        void        CreateGoal(int color, DirectX::XMVECTOR position);

        float             orbitOffset = 1300.0f;
        FQuaternion       sunRotation;
        DirectX::XMVECTOR orbitCenter;

        struct FActiveGoalInfo
        {
                EntityHandle activeGoalGround;
                EntityHandle activeGoalOrbit;
                int          activeColor;
                bool         hasActiveGoal = false;
        };

        static constexpr float goalDistances[3] = {10.0f, 10.0f, 10.0f};

        std::vector<ComponentHandle> sunAlignedTransforms;


        void UpdateSunAlignedObjects();

    protected:
        // Inherited via ISystem
        virtual void OnPreUpdate(float deltaTime) override;
        virtual void OnUpdate(float deltaTime) override;
        virtual void OnPostUpdate(float deltaTime) override;
        virtual void OnInitialize() override;
        virtual void OnShutdown() override;
        virtual void OnResume() override;
        virtual void OnSuspend() override;

        int collectEventTimestamps[3] = {-1, -1, -1};

    public:
        std::vector<ComponentHandle> sunAlignedTransformsSpawning;
        unsigned int    goalsCollected   = 0;
        bool            collectedMask[3] = {};
        FActiveGoalInfo activeGoal;
        void                         DestroyPlanet(GoalComponent* toDestroy);
        ComponentHandle              sunHandle, ring1Handle, ring2Handle, ring3Handle;
};