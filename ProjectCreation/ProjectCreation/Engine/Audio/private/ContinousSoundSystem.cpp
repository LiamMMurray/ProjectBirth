#include "ContinousSoundSystem.h"
#include <LightOrbColors.h>
#include <TransformComponent.h>
#include <MathLibrary.h>
#include <ControllerSystem.h>
#include <GEngine.h>
#include "3DSoundComponent.h"
#include "AudioManager.h"
#include "SoundComponent.h"

void SpatialSoundSystem::OnPreUpdate(float deltaTime)
{}

void SpatialSoundSystem::OnUpdate(float deltaTime)
{
        ControllerSystem*   controllerSystem  = GEngine::Get()->GetSystemManager()->GetSystem<ControllerSystem>();
        IController*        currentController = controllerSystem->GetCurrentController();
        TransformComponent* playerTransformComponent =
            currentController->GetControlledEntity().GetComponent<TransformComponent>();

        for (auto& soundComp : m_HandleManager->GetActiveComponents<SoundComponent>())
        {
                bool isPlaying;
                soundComp.m_gSound->isSoundPlaying(isPlaying);

                if (isPlaying == false)
                {
                        soundComp.GetParent().Free();
                };
        }

        for (auto& soundComp : m_HandleManager->GetActiveComponents<SoundComponent3D>())
        {
                EntityHandle        parent             = soundComp.GetParent();
                TransformComponent* transformComponent = parent.GetComponent<TransformComponent>();

                int16_t index     = soundComp.m_SoundPoolIndex;
                int     type      = soundComp.m_Settings.m_SoundType;
                int     variation = soundComp.m_Settings.m_SoundVaration;

                bool isPlaying;
                m_SoundPools[type][variation][index].gwSound->isSoundPlaying(isPlaying);

                float distance = MathLibrary::CalulateDistance(playerTransformComponent->transform.translation,
                                                               transformComponent->transform.translation);

                float alpha =
                    1.0f - MathLibrary::saturate((distance - soundComp.m_Settings.m_Radius) / soundComp.m_Settings.m_Falloff);

                m_SoundPools[type][variation][index].gwSound->SetVolume(alpha * soundComp.m_Settings.m_Volume * m_masterVolume);
                if (isPlaying == false)
                {
                        if (soundComp.m_Settings.flags.test(SoundComponent3D::E_FLAGS::Looping) == true)
                        {
                                if (soundComp.m_Settings.flags.test(SoundComponent3D::E_FLAGS::ShouldPlay) == true)
                                        m_SoundPools[type][variation][index].gwSound->Play();
                        }
                        else
                        {

                                if (soundComp.m_Settings.flags.test(SoundComponent3D::E_FLAGS::DestroyOnEnd) == true)
                                        parent.Free();
                        }
                }
        };
}
void SpatialSoundSystem::OnPostUpdate(float deltaTime)
{}

void SpatialSoundSystem::OnInitialize()
{
        using namespace Pools;

        m_HandleManager = GEngine::Get()->GetHandleManager();


        for (int color = 0; color < 3; ++color)
        {
                std::stringstream baseColorStream;
                baseColorStream << E_LIGHT_ORBS::Literal[color];

                for (int type = 0; type < E_SOUND_TYPE::TYPE_COUNT; ++type)
                {
                        std::stringstream baseTypeStream;
                        baseTypeStream << baseColorStream.str() << "_" << E_SOUND_TYPE::Literal[type];


                        m_SoundPools[SOUND_COLOR_TYPE(color, type)].resize(E_SOUND_TYPE::variations[type]);
                        for (unsigned int variation = 0; variation < E_SOUND_TYPE::variations[type]; ++variation)
                        {
                                std::stringstream baseVariationStream;
                                baseVariationStream << baseTypeStream.str() << "_" << variation;

                                m_SoundPools[SOUND_COLOR_TYPE(color, type)][variation].Init(E_SOUND_POOL_SIZES::arr[type]);

                                for (unsigned int poolIndex = 0; poolIndex < E_SOUND_POOL_SIZES::arr[type]; ++poolIndex)
                                {
                                        m_SoundPools[SOUND_COLOR_TYPE(color, type)][variation][poolIndex].gwSound =
                                            AudioManager::Get()->CreateSFX(baseVariationStream.str().c_str());
                                }
                        }
                }
        }

        ControllerSystem*   controllerSystem  = GEngine::Get()->GetSystemManager()->GetSystem<ControllerSystem>();
        IController*        currentController = controllerSystem->GetCurrentController();
        TransformComponent* playerTransformComponent =
            currentController->GetControlledEntity().GetComponent<TransformComponent>();

        // SoundComponent3D::FSettings settings;
        // settings.flags.set(SoundComponent3D::E_FLAGS::Looping, true);
        // settings.flags.set(SoundComponent3D::E_FLAGS::ShouldPlay, true);
        // PlaySoundAtLocation(playerTransformComponent->transform.translation, settings);
}

void SpatialSoundSystem::OnShutdown()
{}

void SpatialSoundSystem::OnResume()
{}

void SpatialSoundSystem::OnSuspend()
{}

EntityHandle SpatialSoundSystem::PlaySoundAtLocation(const DirectX::XMVECTOR& pos, SoundComponent3D::FSettings& settings)
{
        EntityHandle entity = m_HandleManager->CreateEntity();

        ComponentHandle     transCompHandle = entity.AddComponent<TransformComponent>();
        TransformComponent* transComp       = transCompHandle.Get<TransformComponent>();
        transComp->transform.translation    = pos;

        ComponentHandle   soundCompHandle = entity.AddComponent<SoundComponent3D>();
        SoundComponent3D* soundComponent  = soundCompHandle.Get<SoundComponent3D>();
        soundComponent->m_Settings        = settings;
        soundComponent->m_SoundPoolIndex  = m_SoundPools[settings.m_SoundType][settings.m_SoundVaration].alloc();

        if (soundComponent->m_SoundPoolIndex != -1)
        {
                soundComponent->m_gwSound =
                    m_SoundPools[settings.m_SoundType][settings.m_SoundVaration][soundComponent->m_SoundPoolIndex].gwSound;
                soundComponent->m_gwSound->SetVolume(m_masterVolume);
                soundComponent->m_gwSound->Play();
        }
        else
        {
                entity.Free();
        }

        return entity;
}

EntityHandle SpatialSoundSystem::PlaySoundWithVolume(float volume, const char* soundName)
{
        EntityHandle entity = m_HandleManager->CreateEntity();

        ComponentHandle soundCompHandle = entity.AddComponent<SoundComponent>();
        SoundComponent* soundComponent  = soundCompHandle.Get<SoundComponent>();
        soundComponent->m_Volume        = volume * m_masterVolume;
        soundComponent->m_gSound        = AudioManager::Get()->CreateSFX(soundName);

        soundComponent->m_gSound->SetVolume(soundComponent->m_Volume);
        soundComponent->m_gSound->Play();

        return entity;
}

void SpatialSoundSystem::FreeSound(int type, int variation, int16_t index)
{
        m_SoundPools[type][variation][index].gwSound->StopSound();
        m_SoundPools[type][variation].Free(index);
}

void SpatialSoundSystem::SetMasterVolume(float volume)
{
        m_masterVolume = volume;
        for (auto& soundComp : m_HandleManager->GetActiveComponents<SoundComponent>())
        {
                soundComp.m_gSound->SetVolume(soundComp.m_Volume * m_masterVolume);
        }
}
