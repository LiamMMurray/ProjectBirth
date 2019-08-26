#include "OrbitSystem.h"
#include <limits>
#include "../../Rendering/Components/StaticMeshComponent.h"
#include "..//..//Rendering/Components/DirectionalLightComponent.h"
#include "..//CoreInput/CoreInput.h"
#include "..//Entities/EntityFactory.h"
#include "..//GEngine.h"
#include "../Controller/ControllerSystem.h"
#include "../GenericComponents/TransformComponent.h"
#include "../Levels/TutorialLevel.h"
#include "../Particle Systems/EmitterComponent.h"
#include "../ResourceManager/Material.h"
#include "SpeedBoostSystem.h"


using namespace DirectX;

void OrbitSystem::CreateGoal(int color, DirectX::XMVECTOR position)
{
        color = min(2, color);

        if (activeGoal.hasActiveGoal)
        {
                activeGoal.activeGoalGround.Free();
                activeGoal.activeGoalOrbit.Free();
        }

        /*** REFACTORING CODE START ***/
        ComponentHandle transHandle, transHandle2;

        auto entityH1   = EntityFactory::CreateStaticMeshEntity("Sphere01", materialNames[color], &transHandle);
        auto goalHandle = entityH1.AddComponent<GoalComponent>();
        auto goalComp   = goalHandle.Get<GoalComponent>();
        auto transComp  = transHandle.Get<TransformComponent>();

        auto entityH2 = EntityFactory::CreateStaticMeshEntity("Sphere01", materialNames[color], &transHandle2, nullptr, false);
        auto transComp2           = transHandle2.Get<TransformComponent>();
        goalComp->color           = color;
        goalComp->collisionHandle = transHandle2;
        goalComp->goalTransform.SetScale(50.0f);
        goalComp->initialTransform.SetScale(1.0f);
        goalComp->initialTransform.translation = position;
        goalComp->goalState                    = E_GOAL_STATE::Spawning;
        transComp->transform                   = goalComp->initialTransform;
        transComp->transform.SetScale(0.0f);

        float time = float(GEngine::Get()->GetTotalTime() / (1.0f + color) + color * 3.7792f);
        float x    = sin(time);
        float y    = cos(time);

        XMVECTOR offset1 = XMVectorSet(x, 0, y, 0.0f);

        goalComp->goalTransform.translation = orbitCenter + offset1 * 150.f * (color + 1.0f);
        transComp2->transform               = goalComp->goalTransform;

        activeGoal.hasActiveGoal    = true;
        activeGoal.activeGoalGround = entityH1;
        activeGoal.activeGoalOrbit  = entityH2;
        activeGoal.activeColor      = color;

        // check if player is in range
        // particle fly up
        XMFLOAT4 goalColor;
        XMStoreFloat4(&goalColor, 4.0f * DirectX::PackedVector::XMLoadColor(&E_LIGHT_ORBS::ORB_COLORS[color]));
        goalColor.w                        = 0.4f;
        goalHandle                         = entityH1.AddComponent<EmitterComponent>();
        EmitterComponent* emitterComponent = goalHandle.Get<EmitterComponent>();
        emitterComponent->ParticleFloatUp(
            XMFLOAT3(-0.3f, -0.3f, -0.3f), XMFLOAT3(0.3f, 0.3f, 0.3f), goalColor, goalColor, XMFLOAT4(3.0f, 1.0f, 0.1f, 0.1f));
        emitterComponent->EmitterData.index         = 2;
        emitterComponent->EmitterData.particleScale = XMFLOAT2(0.2f, 0.2f);
        emitterComponent->maxCount                  = 0;
        emitterComponent->spawnRate                 = 0.0f;
        emitterComponent->EmitterData.textureIndex  = 3;
        /*** REFACTORING CODE END ***/
}

void OrbitSystem::CreateTutorialGoal(int color, DirectX::XMVECTOR position)
{
        color                  = min(3, color);
        tutorialPlanets[color] = true;
        if (activeGoal.hasActiveGoal)
        {
                activeGoal.activeGoalGround.Free();
                activeGoal.activeGoalOrbit.Free();
        }

        /*** REFACTORING CODE START ***/
        ComponentHandle transHandle, transHandle2;


        if (color == E_LIGHT_ORBS::WHITE_LIGHTS)
        {
                auto entityH1   = EntityFactory::CreateStaticMeshEntity("Sphere01", materialNames[color], &transHandle);
                auto goalHandle = entityH1.AddComponent<GoalComponent>();
                auto goalComp   = goalHandle.Get<GoalComponent>();
                auto transComp  = transHandle.Get<TransformComponent>();

                auto entityH2 =
                    EntityFactory::CreateStaticMeshEntity("Sphere01", materialNames[color], &transHandle2, nullptr, false);
                auto transComp2           = transHandle2.Get<TransformComponent>();
                goalComp->color           = color;
                goalComp->collisionHandle = transHandle2;
                goalComp->goalTransform.SetScale(50.0f);
                goalComp->initialTransform.SetScale(1.0f);
                goalComp->initialTransform.translation = position;
                goalComp->goalState                    = E_GOAL_STATE::Spawning;
                transComp->transform                   = goalComp->initialTransform;
                transComp->transform.SetScale(0.0f);

                float time = float(GEngine::Get()->GetTotalTime() / (1.0f + color) + color * 3.7792f);
                float x    = sin(time);
                float y    = cos(time);

                XMVECTOR offset1 = XMVectorSet(x, 0, y, 0.0f);

                goalComp->goalTransform.translation = orbitCenter + offset1 * 150.f * (color + 1.0f);
                transComp2->transform               = goalComp->goalTransform;

                activeGoal.hasActiveGoal    = true;
                activeGoal.activeGoalGround = entityH1;
                activeGoal.activeGoalOrbit  = entityH2;
                activeGoal.activeColor      = color;

                // check if player is in range
                // particle fly up
                XMFLOAT4 goalColor;
                XMStoreFloat4(&goalColor, 4.0f * DirectX::PackedVector::XMLoadColor(&E_LIGHT_ORBS::ORB_COLORS[color]));
                goalColor.w                        = 0.4f;
                goalHandle                         = entityH1.AddComponent<EmitterComponent>();
                EmitterComponent* emitterComponent = goalHandle.Get<EmitterComponent>();
                emitterComponent->ParticleFloatUp(XMFLOAT3(-0.3f, -0.3f, -0.3f),
                                                  XMFLOAT3(0.3f, 0.3f, 0.3f),
                                                  goalColor,
                                                  goalColor,
                                                  XMFLOAT4(3.0f, 1.0f, 0.1f, 0.1f));
                emitterComponent->EmitterData.index         = 2;
                emitterComponent->EmitterData.particleScale = XMFLOAT2(0.2f, 0.2f);
                emitterComponent->maxCount                  = 0;
                emitterComponent->spawnRate                 = 0.0f;
                emitterComponent->EmitterData.textureIndex  = 3;
                /*** REFACTORING CODE END ***/
        }

        else
        {
                auto entityH1   = EntityFactory::CreateStaticMeshEntity("Ring01", materialNames[color], &transHandle);
                auto goalHandle = entityH1.AddComponent<GoalComponent>();
                auto goalComp   = goalHandle.Get<GoalComponent>();
                auto transComp  = transHandle.Get<TransformComponent>();

                auto entityH2 =
                    EntityFactory::CreateStaticMeshEntity("Ring01", materialNames[color], &transHandle2, nullptr, false);
                auto transComp2           = transHandle2.Get<TransformComponent>();
                goalComp->color           = color;
                goalComp->collisionHandle = transHandle2;
                goalComp->goalTransform.SetScale(50.0f);
                goalComp->initialTransform.SetScale(1.0f);
                goalComp->initialTransform.translation = position;
                goalComp->goalState                    = E_GOAL_STATE::Spawning;
                transComp->transform                   = goalComp->initialTransform;
                transComp->transform.SetScale(0.0f);

                float time = float(GEngine::Get()->GetTotalTime() / (1.0f + color) + color * 3.7792f);
                float x    = sin(time);
                float y    = cos(time);

                XMVECTOR offset1 = XMVectorSet(x, 0, y, 0.0f);

                goalComp->goalTransform.translation = orbitCenter + offset1 * 150.f * (color + 1.0f);
                transComp2->transform               = goalComp->goalTransform;

                activeGoal.hasActiveGoal    = true;
                activeGoal.activeGoalGround = entityH1;
                activeGoal.activeGoalOrbit  = entityH2;
                activeGoal.activeColor      = color;

                // check if player is in range
                // particle fly up
                XMFLOAT4 goalColor;
                XMStoreFloat4(&goalColor, 4.0f * DirectX::PackedVector::XMLoadColor(&E_LIGHT_ORBS::ORB_COLORS[color]));
                goalColor.w                        = 0.4f;
                goalHandle                         = entityH1.AddComponent<EmitterComponent>();
                EmitterComponent* emitterComponent = goalHandle.Get<EmitterComponent>();
                emitterComponent->ParticleFloatUp(XMFLOAT3(-0.3f, -0.3f, -0.3f),
                                                  XMFLOAT3(0.3f, 0.3f, 0.3f),
                                                  goalColor,
                                                  goalColor,
                                                  XMFLOAT4(3.0f, 1.0f, 0.1f, 0.1f));
                emitterComponent->EmitterData.index         = 2;
                emitterComponent->EmitterData.particleScale = XMFLOAT2(0.2f, 0.2f);
                emitterComponent->maxCount                  = 0;
                emitterComponent->spawnRate                 = 0.0f;
                emitterComponent->EmitterData.textureIndex  = 3;
                /*** REFACTORING CODE END ***/}
}

void OrbitSystem::UpdateSunAlignedObjects(float delta)
{
        for (auto& h : sunAlignedTransforms)
        {
                TransformComponent* tc    = h.Get<TransformComponent>();
                tc->transform.translation = orbitCenter;
                tc->transform.rotation    = sunRotation;

                float currentRadius = tc->transform.GetRadius();
                if (fabsf(currentRadius - 150.0f) < 0.1f)
                {
                        tc->transform.SetScale(150.0f);
                }
                else
                {
                        tc->transform.SetScale(MathLibrary::MoveTowards(currentRadius, 150.0f, delta * 20.0f));
                }
        }
}

void OrbitSystem::OnPreUpdate(float deltaTime)
{

        ComponentHandle playerTransformHandle =
            m_PlayerController->GetControlledEntity().GetComponentHandle<TransformComponent>();
        TransformComponent* playerTransform = playerTransformHandle.Get<TransformComponent>();

        sunRotation = GEngine::Get()->m_SunHandle.GetComponent<DirectionalLightComponent>()->m_LightRotation;

        orbitCenter = playerTransform->transform.translation - sunRotation.GetForward() * orbitOffset;

        UpdateSunAlignedObjects(deltaTime);
}

void OrbitSystem::OnUpdate(float deltaTime)
{

        ComponentHandle playerTransformHandle =
            m_PlayerController->GetControlledEntity().GetComponentHandle<TransformComponent>();
        TransformComponent* playerTransform = playerTransformHandle.Get<TransformComponent>();

        double totalTime = GEngine::Get()->GetTotalTime();

        TransformComponent* closestGoalTransform = nullptr;

        for (auto& goalComp : m_HandleManager->GetActiveComponents<GoalComponent>())
        {
                EntityHandle        goalParent      = goalComp.GetParent();
                ComponentHandle     goalHandle      = goalComp.GetHandle();
                ComponentHandle     transHandle     = goalParent.GetComponentHandle<TransformComponent>();
                TransformComponent* transComp       = transHandle.Get<TransformComponent>();
                TransformComponent* transCompPuzzle = goalComp.collisionHandle.Get<TransformComponent>();


                float time = float(totalTime / (1.0f + goalComp.color) + goalComp.color * 3.7792f);
                float x    = sin(time);
                float y    = cos(time);

                XMVECTOR offset1 = XMVectorSet(x, y, 0, 0.0f);


                offset1 = XMVector3Rotate(offset1, sunRotation.data);

                goalComp.goalTransform.translation = orbitCenter + offset1 * 150.f * (goalComp.color + 1.0f);
                transCompPuzzle->transform         = goalComp.goalTransform;

                if (goalComp.goalState == E_GOAL_STATE::Spawning)
                {
                        float scale       = transComp->transform.GetRadius();
                        float targetScale = goalComp.initialTransform.GetRadius();
                        float newScale    = MathLibrary::MoveTowards(scale, targetScale, deltaTime * 1.0f);
                        transComp->transform.SetScale(newScale);

                        if (scale >= targetScale)
                                goalComp.goalState = E_GOAL_STATE::Idle;

                        return;
                }

                float distanceSq =
                    MathLibrary::CalulateDistanceSq(playerTransform->transform.translation, transComp->transform.translation);

                auto emitterComponent = goalParent.GetComponent<EmitterComponent>();
                if (goalComp.goalState == E_GOAL_STATE::Idle && distanceSq < 80.0f)
                {

                        emitterComponent->spawnRate = 50.0f;
                        emitterComponent->maxCount  = 2048;
                }
                XMVECTOR deltaDistance = transCompPuzzle->transform.translation - transComp->transform.translation;
                XMVECTOR offset        = 1.0f * XMVector3Normalize(deltaDistance);
                XMStoreFloat3(&emitterComponent->EmitterData.minInitialVelocity, XMVectorZero());
                XMStoreFloat3(&emitterComponent->EmitterData.maxInitialVelocity, +offset);
                XMStoreFloat3(&emitterComponent->EmitterData.acceleration, deltaDistance / 100.0f);

                if (goalComp.goalState == E_GOAL_STATE::Idle && distanceSq < 3.5f)
                {
                        goalComp.goalState  = E_GOAL_STATE::InitialTransition;
                        transComp->wrapping = false;
                        m_PlayerController->RequestPuzzleMode(goalHandle, orbitCenter, true, 4.0f);
                        SYSTEM_MANAGER->GetSystem<SpeedBoostSystem>()->m_ColorsCollected[activeGoal.activeColor] = true;
                }

                if (goalComp.goalState == E_GOAL_STATE::Done)
                {
                        goalComp.targetAlpha = 1.0f;

                        float dist  = MathLibrary::CalulateDistance(goalComp.initialTransform.translation,
                                                                   goalComp.goalTransform.translation);
                        float speed = MathLibrary::lerp(
                            goalComp.transitionInitialSpeed, goalComp.transitionFinalSpeed, min(1.0f, goalComp.currAlpha));
                        goalComp.currAlpha =
                            MathLibrary::MoveTowards(goalComp.currAlpha, goalComp.targetAlpha, speed * deltaTime * 1.0f / dist);

                        transComp->transform =
                            FTransform::Lerp(goalComp.initialTransform, goalComp.goalTransform, min(1.0f, goalComp.currAlpha));
                }
        }

        goalsCollected = std::min<unsigned int>(goalsCollected, 3);

        XMVECTOR nextGoalPos = GET_SYSTEM(SpeedBoostSystem)->m_CurrentPathEnd;

        ControllerSystem* controllerSystem = SYSTEM_MANAGER->GetSystem<ControllerSystem>();

        if (GCoreInput::GetKeyState(KeyCode::Four) == KeyState::Down)
        {
                CreateTutorialGoal(0, nextGoalPos);
        }
        if (GCoreInput::GetKeyState(KeyCode::Five) == KeyState::Down)
        {
                CreateTutorialGoal(1, nextGoalPos);
        }
        if (GCoreInput::GetKeyState(KeyCode::Six) == KeyState::Down)
        {
                CreateTutorialGoal(2, nextGoalPos);
        }
        if (GCoreInput::GetKeyState(KeyCode::Seven) == KeyState::Down)
        {
                CreateTutorialGoal(3, nextGoalPos);
        }

        if ((GEngine::Get()->GetLevelStateManager()->GetCurrentLevelState()->GetLevelType() == E_Level_States::TUTORIAL_LEVEL))
        {
                for (int i = 0; i < 4; ++i)
                {
                        if (m_PendingGoalCounts[i] > 0)
                        {
                                TimedFunction timedFunction;
                                timedFunction.m_delay    = 2.0f;
                                timedFunction.m_function = []() { TutorialLevel::Get()->RequestNextPhase(); };
                                m_timedFunctions.push_back(timedFunction);

                                m_PendingGoalCounts[i]--;
                        }
                }
        }

        else
        {

                for (int i = 0; i < 3; ++i)
                {
                        if (m_PendingGoalCounts[i] > 0)
                        {
                                if (collectedMask[i] == false)
                                {
                                        if (activeGoal.hasActiveGoal == false || activeGoal.activeColor != i)
                                        { // play sfx when spawned
                                                CreateGoal(i, nextGoalPos);
                                        }
                                }
                                m_PendingGoalCounts[i]--;
                        }
                }
        }

        for (auto i = 0; i < m_timedFunctions.size(); i++)
        {
                m_timedFunctions[i].m_delay -= deltaTime;
				if (m_timedFunctions[i].m_delay <= 0) {
                        m_timedFunctions[i].m_function();
                        m_timedFunctions.erase(m_timedFunctions.begin() + i);
                        --i;
				}
        }
}

void OrbitSystem::OnPostUpdate(float deltaTime)
{}

void OrbitSystem::OnInitialize()
{
        m_HandleManager   = GEngine::Get()->GetHandleManager();
        m_ResourceManager = GEngine::Get()->GetResourceManager();

        m_PlayerController = (PlayerController*)SYSTEM_MANAGER->GetSystem<ControllerSystem>()
                                 ->m_Controllers[ControllerSystem::E_CONTROLLERS::PLAYER];


        for (int i = 0; i < 3; ++i)
        {
                auto mat = m_ResourceManager->GetResource<Material>(m_ResourceManager->LoadMaterial(materialNames[i]));

                XMStoreFloat3(&mat->m_SurfaceProperties.emissiveColor,
                              1.5f * DirectX::PackedVector::XMLoadColor(&E_LIGHT_ORBS::RING_COLORS[i]));
        }

        std::string ring1MaterialName = "Ring01Mat", ring2MaterialName = "Ring02Mat", ring3MaterialName = "Ring03Mat";

        auto baseRingMaterial = m_ResourceManager->LoadMaterial("GlowMatRing");

        // Red Ring Material
        {
                auto ring1MaterialHandle =
                    m_ResourceManager->CopyResource<Material>(baseRingMaterial, ring1MaterialName.c_str());

                auto ring1Material = m_ResourceManager->GetResource<Material>(ring1MaterialHandle);

                XMStoreFloat3(&ring1Material->m_SurfaceProperties.emissiveColor,
                              1.5f * DirectX::PackedVector::XMLoadColor(&E_LIGHT_ORBS::RING_COLORS[0]));
        }

        // Green Ring Material
        {
                auto ring2MaterialHandle =
                    m_ResourceManager->CopyResource<Material>(baseRingMaterial, ring2MaterialName.c_str());

                auto ring2Material = m_ResourceManager->GetResource<Material>(ring2MaterialHandle);

                XMStoreFloat3(&ring2Material->m_SurfaceProperties.emissiveColor,
                              1.5f * DirectX::PackedVector::XMLoadColor(&E_LIGHT_ORBS::RING_COLORS[1]));
        }

        // Blue Ring Material
        {
                auto ring3MaterialHandle =
                    m_ResourceManager->CopyResource<Material>(baseRingMaterial, ring3MaterialName.c_str());

                auto ring3Material = m_ResourceManager->GetResource<Material>(ring3MaterialHandle);

                XMStoreFloat3(&ring3Material->m_SurfaceProperties.emissiveColor,
                              1.5f * DirectX::PackedVector::XMLoadColor(&E_LIGHT_ORBS::RING_COLORS[2]));
        }
}

void OrbitSystem::OnShutdown()
{}

void OrbitSystem::OnResume()
{}

void OrbitSystem::OnSuspend()
{}

void OrbitSystem::CreateSun()
{
        ComponentHandle transHandle;
        EntityFactory::CreateStaticMeshEntity("Sphere01", "GlowMatSun", &transHandle, nullptr, false);
        auto sunTransform = transHandle.Get<TransformComponent>();
        sunTransform->transform.SetScale(0.0f);
        sunAlignedTransforms.push_back(transHandle);
}

void OrbitSystem::CreateRing(int color)
{
        ComponentHandle transHandle;
        EntityFactory::CreateStaticMeshEntity(ringMeshNames[color], ringMaterialNames[color], &transHandle, nullptr, false);
        auto transformComp = transHandle.Get<TransformComponent>();
        transformComp->transform.SetScale(0.0f);
        sunAlignedTransforms.push_back(transHandle);
}

void OrbitSystem::InstantCreateOrbitSystem()
{
        goalsCollected = 0;

        InstantRemoveOrbitSystem();

        ComponentHandle playerTransformHandle =
            m_PlayerController->GetControlledEntity().GetComponentHandle<TransformComponent>();
        TransformComponent* playerTransform = playerTransformHandle.Get<TransformComponent>();

        sunRotation = GEngine::Get()->m_SunHandle.GetComponent<DirectionalLightComponent>()->m_LightRotation;

        orbitCenter = playerTransform->transform.translation - sunRotation.GetForward() * orbitOffset;

        CreateSun();

        for (int i = 0; i < 3; ++i)
        {
                CreateRing(i);
        }

        UpdateSunAlignedObjects(GEngine::Get()->GetDeltaTime());
}

void OrbitSystem::InstantRemoveOrbitSystem()
{
        InstantRemoveFromOrbit();

        for (int i = 0; i < sunAlignedTransforms.size(); ++i)
        {
                sunAlignedTransforms.at(i).Get<TransformComponent>()->GetParent().Free();
        }

        sunAlignedTransforms.clear();
}

void OrbitSystem::InstantInOrbit(int color)
{
        color = min(2, color);

        if (activeGoal.hasActiveGoal)
        {
                activeGoal.activeGoalGround.Free();
                activeGoal.activeGoalOrbit.Free();
        }

        /*** REFACTORING CODE START ***/
        ComponentHandle transHandle, transHandle2;

        auto entityH1   = EntityFactory::CreateStaticMeshEntity("Sphere01", materialNames[color], &transHandle);
        auto goalHandle = entityH1.AddComponent<GoalComponent>();
        auto goalComp   = goalHandle.Get<GoalComponent>();
        auto transComp  = transHandle.Get<TransformComponent>();

        auto entityH2 = EntityFactory::CreateStaticMeshEntity("Sphere01", materialNames[color], &transHandle2, nullptr, false);
        auto transComp2           = transHandle2.Get<TransformComponent>();
        goalComp->color           = color;
        goalComp->collisionHandle = transHandle2;
        goalComp->goalTransform.SetScale(50.0f);
        goalComp->initialTransform.SetScale(1.0f);
        goalComp->initialTransform.translation = transComp2->transform.translation;
        goalComp->goalState                    = E_GOAL_STATE::Done;
        transComp->transform                   = goalComp->initialTransform;
        transComp->transform.SetScale(0.0f);

        float time = float(GEngine::Get()->GetTotalTime() / (1.0f + color) + color * 3.7792f);
        float x    = sin(time);
        float y    = cos(time);

        XMVECTOR offset1 = XMVectorSet(x, 0, y, 0.0f);

        goalComp->goalTransform.translation = orbitCenter + offset1 * 150.f * (color + 1.0f);
        transComp2->transform               = goalComp->goalTransform;

        activeGoal.hasActiveGoal    = false;
        activeGoal.activeGoalGround = entityH1;
        activeGoal.activeGoalOrbit  = entityH2;
        activeGoal.activeColor      = color;

        // check if player is in range
        // particle fly up
        XMFLOAT4 goalColor;
        XMStoreFloat4(&goalColor, 4.0f * DirectX::PackedVector::XMLoadColor(&E_LIGHT_ORBS::ORB_COLORS[color]));
        goalColor.w                        = 0.4f;
        goalHandle                         = entityH1.AddComponent<EmitterComponent>();
        EmitterComponent* emitterComponent = goalHandle.Get<EmitterComponent>();
        emitterComponent->ParticleFloatUp(
            XMFLOAT3(-0.3f, -0.3f, -0.3f), XMFLOAT3(0.3f, 0.3f, 0.3f), goalColor, goalColor, XMFLOAT4(3.0f, 1.0f, 0.1f, 0.1f));
        emitterComponent->EmitterData.index         = 2;
        emitterComponent->EmitterData.particleScale = XMFLOAT2(0.2f, 0.2f);
        emitterComponent->maxCount                  = 0;
        emitterComponent->spawnRate                 = 0.0f;
        emitterComponent->EmitterData.textureIndex  = 3;

        goalsCollected++;
        SYSTEM_MANAGER->GetSystem<SpeedBoostSystem>()->m_ColorsCollected[color] = true;
        /*** REFACTORING CODE END ***/
}

void OrbitSystem::InstantRemoveFromOrbit()
{
        goalsCollected = 0;

        for (auto& iter : m_HandleManager->GetComponents<GoalComponent>())
        {
                SYSTEM_MANAGER->GetSystem<SpeedBoostSystem>()->m_ColorsCollected[iter.color] = false;
                DestroyPlanet(&iter);
        }
}

void OrbitSystem::DestroyPlanet(GoalComponent* toDestroy)
{
        toDestroy->collisionHandle.Get<TransformComponent>()->GetParent().Free();
        toDestroy->GetParent().Free();
        activeGoal.hasActiveGoal = false;
}
