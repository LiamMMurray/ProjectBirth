#include "PlayerMovement.h"
#include "../../ECS/Entity.h"
#include "../CoreInput/CoreInput.h"
#include "../GEngine.h"

#include "../MathLibrary/MathLibrary.h"
#include "../MathLibrary/Quaternion.h"

#include "..//Gameplay/GoalComponent.h"
#include "../CollisionLibary/CollisionLibary.h"
#include "../CollisionLibary/CollisionResult.h"
#include "../GenericComponents/TransformComponent.h"

#include "PlayerCinematicState.h"
#include "PlayerGroundState.h"
#include "PlayerPuzzleState.h"
#include "PlayerStateEvents.h"

#include "../../Rendering/DebugRender/debug_renderer.h"
// v Testing only delete when done v
#include <iostream>
// ^ Testing only delete when done ^
using namespace DirectX;
using namespace Shapes;
using namespace Collision;
using namespace debug_renderer;

void PlayerController::GatherInput()
{

        m_CurrentInput = XMVectorZero();
        if (IsEnabled())
        {
                // requestedDirection = MoveDirections::NO_DIRECTION;
                XMFLOAT4 tempDir = {0.0f, 0.0f, 0.0f, 0.0f};

                /////////////////////////
                // Movement input keys //
                /////////////////////////
                {
                        // Check Forward speed
                        if (GCoreInput::GetKeyState(KeyCode::W) == KeyState::Down)
                        {
                                tempDir.z += 1.0f;
                        }
                        // Backward
                        if (GCoreInput::GetKeyState(KeyCode::S) == KeyState::Down)
                        {
                                tempDir.z -= 1.0f;
                        }
                        // Left
                        if (GCoreInput::GetKeyState(KeyCode::A) == KeyState::Down)
                        {
                                tempDir.x -= 1.0f;
                        }
                        // Right
                        if (GCoreInput::GetKeyState(KeyCode::D) == KeyState::Down)
                        {
                                tempDir.x += 1.0f;
                        }
                }
                // Space bar is used for rhythm input
                if (GCoreInput::GetKeyState(KeyCode::Space) == KeyState::DownFirst)
                {
                        spaceTimeStamp = GEngine::Get()->GetTotalTime();
                }

                m_CurrentInput = XMLoadFloat4(&tempDir);
        }
}

void PlayerController::ProcessInput()
{
        _cachedControlledTransformComponent = m_ControlledEntityHandle.GetComponent<TransformComponent>();
}

void PlayerController::ApplyInput()
{
        m_StateMachine.Update(cacheTime, _cachedControlledTransformComponent);
        FSphere fSpherePlayer;
        fSpherePlayer.center = _cachedControlledTransformComponent->transform.translation;
        fSpherePlayer.radius = 0.25f;

        debug_renderer::AddSphere(fSpherePlayer, 36, XMMatrixIdentity());
}

void PlayerController::DebugPrintSpeedBoostColor(int color)
{
        switch (color)
        {
                case 0:
                        ConsoleWindow::PrintMessage("Boosting on red light", "PlayerMovement");
                        break;
                case 1:
                        ConsoleWindow::PrintMessage("Boosting on blue light", "PlayerMovement");
                        break;
                case 2:
                        ConsoleWindow::PrintMessage("Boosting on green light", "PlayerMovement");
                        break;
                case 3:
                        ConsoleWindow::PrintMessage("Boosting on white light", "PlayerMovement");
                        break;
        }
}

void PlayerController::Shutdown()
{
        m_StateMachine.Shutdown();
}

void PlayerController::Init(EntityHandle h)
{
        IController::Init(h);
        m_CurrentVelocity           = DirectX::XMVectorZero();
        m_CurrentInput              = DirectX::XMVectorZero();
        ComponentHandle     tHandle = m_ControlledEntityHandle.GetComponentHandle<TransformComponent>();
        TransformComponent* tComp   = tHandle.Get<TransformComponent>();

        m_EulerAngles = tComp->transform.rotation.ToEulerAngles();

        // Create any states and set their respective variables here
        m_CinematicState = m_StateMachine.CreateState<PlayerCinematicState>();
        m_GroundState    = m_StateMachine.CreateState<PlayerGroundState>();
        auto puzzleState = m_StateMachine.CreateState<PlayerPuzzleState>();

        m_StateMachine.AddTransition(m_GroundState, m_CinematicState, E_PLAYERSTATE_EVENT::TO_TRANSITION);
        m_StateMachine.AddTransition(m_CinematicState, m_GroundState, E_PLAYERSTATE_EVENT::TO_GROUND);
        m_StateMachine.AddTransition(m_CinematicState, puzzleState, E_PLAYERSTATE_EVENT::TO_PUZZLE);

        m_StateMachine.AddTransition(m_GroundState, puzzleState, E_PLAYERSTATE_EVENT::TO_PUZZLE);
        m_StateMachine.AddTransition(puzzleState, m_GroundState, E_PLAYERSTATE_EVENT::TO_GROUND);
        m_StateMachine.AddTransition(puzzleState, m_CinematicState, E_PLAYERSTATE_EVENT::TO_TRANSITION);

        // Request initial transition
        FTransform target = tComp->transform;
        target.rotation   = XMQuaternionIdentity();

        RequestCinematicTransition(1, &tHandle, &target, E_PLAYERSTATE_EVENT::TO_GROUND, 5.0f);

        // After you create the states, initialize the state machine. First created state is starting state
        m_StateMachine.Init(this);

        // Init sound pool
        for (unsigned int i = 0; i < MAX_SPEEDBOOST_SOUNDS; ++i)
        {
                mSpeedBoostSoundPool[i] = AudioManager::Get()->CreateSFX("whiteSpeedBoost");
        }
}

void PlayerController::SpeedBoost(DirectX::XMVECTOR boostPos, int color, double collisionTimeStamp)
{
        const KeyCode keyCodes[]   = {KeyCode::LeftShift, KeyCode::Q, KeyCode::E, (KeyCode)-1};
        const char*   soundNames[] = {"whiteSpeedBoost", "whiteSpeedBoost", "whiteSpeedBoost", "whiteSpeedBoost"};
		
        // Audio that will play on boost
        if ((int)keyCodes[color] == -1 || GCoreInput::GetKeyState(keyCodes[color]) == KeyState::Down)
        {
                if ((spaceTimeStamp >= collisionTimeStamp && spaceTimeStamp < (collisionTimeStamp + rhythmThreshold)) ||
                    (spaceTimeStamp <= collisionTimeStamp && spaceTimeStamp > (collisionTimeStamp - rhythmThreshold)))
                {
                        bool isPlaying;
                        mSpeedBoostSoundPool[currSpeedBoostIteration]->isSoundPlaying(isPlaying);
                        mSpeedBoostSoundPool[currSpeedBoostIteration]->Play();
                        DebugPrintSpeedBoostColor(color);
                        currentMaxSpeed = std::min(currentMaxSpeed + 0.5f, maxMaxSpeed);
                        XMVECTOR currentInput =
                            XMVector3Rotate(m_CurrentInput, _cachedControlledTransformComponent->transform.rotation.data);
                        if (MathLibrary::VectorDotProduct(currentInput, m_CurrentVelocity) > 0.0f)
                        {
                                m_CurrentVelocity += 2.0f * XMVector3Normalize(m_CurrentVelocity);
                                m_CurrentVelocity = XMVector3ClampLength(m_CurrentVelocity, 0.0f, currentMaxSpeed);
                                m_GroundState->AddSpeedBoost();
                        }
                }
        }
        currSpeedBoostIteration++;
        currSpeedBoostIteration %= MAX_SPEEDBOOST_SOUNDS;
}

void PlayerController::AddCurrentVelocity(DirectX::XMVECTOR val)
{
        m_CurrentVelocity = m_CurrentVelocity + val;
}

void PlayerController::SetUseGravity(bool val)
{
        m_GroundState->bUseGravity = val;
}

bool PlayerController::GetUseGravity()
{
        return m_GroundState->bUseGravity;
}

void PlayerController::SetYExtraSpeed(float val)
{
        m_GroundState->m_ExtraYSpeed = val;
}

void PlayerController::RequestCinematicTransition(int                    count,
                                                  const ComponentHandle* handles,
                                                  const FTransform*      targets,
                                                  int                    targetState,
                                                  float                  duration,
                                                  float                  delay)
{
        TransformComponent* transformComp = m_ControlledEntityHandle.GetComponent<TransformComponent>();

        m_CinematicState->SetTransitionMode(E_TRANSITION_MODE::Simple);
        m_CinematicState->AddTransformTransitions(count, handles, targets);
        m_CinematicState->SetTansitionTargetState(targetState);
        m_CinematicState->SetTansitionDuration(duration);
        m_CinematicState->SetTransitionDelay(delay);

        m_StateMachine.Transition(E_PLAYERSTATE_EVENT::TO_TRANSITION);
}

void PlayerController::RequestCinematicTransitionLookAt(const ComponentHandle  lookAtTarget,
                                                        int                    count,
                                                        const ComponentHandle* handles,
                                                        const FTransform*      targets,
                                                        int                    targetState,
                                                        float                  duration,
                                                        float                  lookAtTransitionDuration,
                                                        float                  delay)
{
        TransformComponent* transformComp = m_ControlledEntityHandle.GetComponent<TransformComponent>();

        m_CinematicState->SetTransitionMode(E_TRANSITION_MODE::LookAt);
        m_CinematicState->AddTransformTransitions(count, handles, targets);
        m_CinematicState->SetLookAtTransitionDuration(lookAtTransitionDuration);
        m_CinematicState->SetLookAtTarget(lookAtTarget);
        m_CinematicState->SetTansitionTargetState(targetState);
        m_CinematicState->SetTansitionDuration(duration);
        m_CinematicState->SetTransitionDelay(delay);

        m_StateMachine.Transition(E_PLAYERSTATE_EVENT::TO_TRANSITION);
}

void PlayerController::RequestPuzzleMode(ComponentHandle          goalHandle,
                                         const DirectX::XMVECTOR& puzzleCenter,
                                         bool                     alignToGoal,
                                         float                    transitionDuration,
                                         float                    lookAtDuration,
                                         int                      otherTransformsCount,
                                         const ComponentHandle*   handles,
                                         const FTransform*        transforms)
{
        SetGoalComponent(goalHandle);

        if (!alignToGoal)
                m_StateMachine.Transition(E_PLAYERSTATE_EVENT::TO_PUZZLE);
        else
        {
                TransformComponent* playerTransformComp = m_ControlledEntityHandle.GetComponent<TransformComponent>();
                GoalComponent*      goalComp            = goalHandle.Get<GoalComponent>();
                EntityHandle        goalCompParent      = goalComp->GetParent();
                ComponentHandle     goalTransformHandle = goalCompParent.GetComponentHandle<TransformComponent>();

                FSphere sphereInitial;
                sphereInitial.center = goalComp->initialTransform.translation;
                sphereInitial.radius = goalComp->initialTransform.GetRadius();

                FSphere sphereTarget;
                sphereTarget.center = goalComp->goalTransform.translation;
                sphereTarget.radius = goalComp->goalTransform.GetRadius();

                XMVECTOR eyePos = playerTransformComp->transform.translation;

                float desiredAngularDiameter = MathLibrary::CalculateAngularDiameter(eyePos, sphereTarget);
                float desiredInitialDistance =
                    MathLibrary::CalculateDistanceFromAngularDiameter(desiredAngularDiameter, sphereInitial);

                XMVECTOR dir = XMVector3Normalize(puzzleCenter - eyePos);

                FTransform targetGoalTransform  = goalComp->initialTransform;
                targetGoalTransform.translation = eyePos + dir * desiredInitialDistance;

                m_CinematicState->AddTransformTransitions(1, &goalTransformHandle, &targetGoalTransform);
                RequestCinematicTransitionLookAt(goalTransformHandle,
                                                 otherTransformsCount,
                                                 handles,
                                                 transforms,
                                                 E_PLAYERSTATE_EVENT::TO_PUZZLE,
                                                 transitionDuration,
                                                 lookAtDuration);
        }
}
