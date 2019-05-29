#include "SpeedBoostSystem.h"
#include "../Controller/ControllerManager.h"
#include "../Entities/EntityFactory.h"
#include "../GEngine.h"

#include "..//GenericComponents/TransformComponent.h"

#include <cmath>
#include <map>
#include <random>
#include <string>
#include "..//CoreInput/CoreInput.h"
#include "..//MathLibrary/MathLibrary.h"
#include "../Controller/PlayerMovement.h"

using namespace DirectX;

std::random_device                    r;
std::default_random_engine            e1(r());
std::uniform_real_distribution<float> uniform_dist(-8.0f, 8.0f);

void SpeedBoostSystem::RandomMoveBoost(TransformComponent* boostTC, TransformComponent* playerTC)
{
        float x = uniform_dist(e1);
        float y = uniform_dist(e1);
        float z = uniform_dist(e1);

        boostTC->transform.translation = playerTC->transform.translation + XMVectorSet(x, 0.0f, z, 1.0f);
}

void SpeedBoostSystem::OnPreUpdate(float deltaTime)
{}

void SpeedBoostSystem::OnUpdate(float deltaTime)
{

        TransformComponent* playerTransform = m_ComponentManager->GetComponent<TransformComponent>(
            ControllerManager::Get()->m_Controllers[ControllerManager::E_CONTROLLERS::PLAYER]->GetControlledEntity());

        for (int i = 0; i < m_MaxSpeedBoosts; ++i)
        {
                TransformComponent* tc = m_ComponentManager->GetComponent<TransformComponent>(m_BoostTransformHandles[i]);
                float               distanceSq =
                    MathLibrary::CalulateDistanceSq(playerTransform->transform.translation, tc->transform.translation);

                if (distanceSq > m_MaxBoostDistance * m_MaxBoostDistance)
                {
                        RandomMoveBoost(tc, playerTransform);
                }

                if (distanceSq < m_BoostRadius * 2.0f)
                {
                        RandomMoveBoost(tc, playerTransform);

                        static_cast<PlayerController*>(
                            ControllerManager::Get()->m_Controllers[ControllerManager::E_CONTROLLERS::PLAYER])
                            ->SpeedBoost(XMVectorZero());
                }
        }
}

void SpeedBoostSystem::OnPostUpdate(float deltaTime)
{}

void SpeedBoostSystem::OnInitialize()
{
        m_EntityManager    = GEngine::Get()->GetEntityManager();
        m_ComponentManager = GEngine::Get()->GetComponentManager();

        TransformComponent* playerTransform = m_ComponentManager->GetComponent<TransformComponent>(
            ControllerManager::Get()->m_Controllers[ControllerManager::E_CONTROLLERS::PLAYER]->GetControlledEntity());

        for (int i = 0; i < m_MaxSpeedBoosts; ++i)
        {
                EntityFactory::CreateStaticMeshEntity("Sphere01", "GlowMatRing", &m_BoostTransformHandles[i]);

                TransformComponent* tc = m_ComponentManager->GetComponent<TransformComponent>(m_BoostTransformHandles[i]);
                float               distanceSq =
                    MathLibrary::CalulateDistanceSq(playerTransform->transform.translation, tc->transform.translation);
                tc->transform.SetScale(m_BoostRadius * 2.0f);

                RandomMoveBoost(tc, playerTransform);
        }
}

void SpeedBoostSystem::OnShutdown()
{}

void SpeedBoostSystem::OnResume()
{}

void SpeedBoostSystem::OnSuspend()
{}