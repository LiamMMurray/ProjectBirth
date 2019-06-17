#include "PlayerGroundState.h"
#include <iostream>
#include "..//GEngine.h"
#include "..//GenericComponents/TransformComponent.h"
#include "..//MathLibrary/MathLibrary.h"
#include "../CoreInput/CoreInput.h"
#include "PlayerControllerStateMachine.h"
#include "PlayerMovement.h"
using namespace DirectX;

void PlayerGroundState::Enter()
{
        auto playerTransformComponent =
            GEngine::Get()->GetComponentManager()->GetComponent<TransformComponent>(_playerController->GetControlledEntity());

        _playerController->SetEulerAngles(playerTransformComponent->transform.rotation.ToEulerAngles());
}

void PlayerGroundState::Update(float deltaTime)
{
        XMVECTOR currentVelocity = _playerController->GetCurrentVelocity();
        // Get Delta Time
        float totalTime = (float)GEngine::Get()->GetTotalTime();

        XMFLOAT3 eulerAngles = _playerController->GetEulerAngles();

        float           angularSpeed = XMConvertToRadians(5.0f) * deltaTime;
        constexpr float pitchLimit   = XMConvertToRadians(90.0f);
        constexpr float rollLimit    = 20.0f;

        eulerAngles.x += GCoreInput::GetMouseY() * angularSpeed;
        float yawDelta = eulerAngles.y;
        eulerAngles.y += GCoreInput::GetMouseX() * angularSpeed;
        yawDelta = eulerAngles.y - yawDelta;
        eulerAngles.z += GCoreInput::GetMouseX() * angularSpeed;

        eulerAngles.x = MathLibrary::clamp(eulerAngles.x, -pitchLimit, pitchLimit);

        // Convert to degrees due to precision errors using small radian values
        float rollDegrees = XMConvertToDegrees(eulerAngles.z);
        rollDegrees       = MathLibrary::clamp(rollDegrees, -rollLimit, rollLimit);
        rollDegrees       = MathLibrary::lerp(rollDegrees, 0.0f, MathLibrary::clamp(deltaTime * rollLimit * 0.35f, 0.0f, 1.0f));
        eulerAngles.z     = XMConvertToRadians(rollDegrees);
        _cachedTransformComponent->transform.rotation = FQuaternion::FromEulerAngles(eulerAngles);

        currentVelocity = XMVector3Rotate(currentVelocity, XMQuaternionRotationAxis(VectorConstants::Up, yawDelta));

        // Get the Speed from the gathered input
        XMVECTOR currentInput = _playerController->GetCurrentInput();
        currentInput          = XMVector3Rotate(currentInput, _cachedTransformComponent->transform.rotation.data);
        if (bUseGravity)
        {
                currentInput =
                    XMVector3Normalize(XMVectorSetY(currentInput, 0.0f)) * XMVectorGetX(XMVector3Length(currentInput));
        }
        // Normalize the gathered input to determine the desired direction
        XMVECTOR desiredDir = XMVector3Normalize(currentInput);

        // Determine the max speed the object can move
        float maxSpeed = _playerController->GetCurrentMaxSpeed();

        XMVECTOR desiredVelocity = maxSpeed * desiredDir;
        // Calculate desiredVelocity by multiplying the currSpeed by the direction we want to go
        // Determine if we should speed up or slow down
        float accel = (XMVectorGetX(XMVector3Length(desiredVelocity)) >= XMVectorGetX(XMVector3Length(currentVelocity))) ?
                          _playerController->GetAcceleration() :
                          _playerController->GetDeacceleration();

        // Calculate distance from our current velocity to our desired velocity
        float dist = MathLibrary::CalulateDistance(currentVelocity, desiredVelocity);

        // Calculate change based on the type of acceleration, the change in time, and the calculated distance
        float delta = std::min(accel * deltaTime, dist);

        // Normalize the difference of the desired velocity and the current velocity
        XMVECTOR deltaVec = XMVector3Normalize(desiredVelocity - currentVelocity);
        currentVelocity   = currentVelocity + deltaVec * delta + m_ExtraYSpeed * VectorConstants::Up;
        if (bUseGravity)
        {
                float currY   = XMVectorGetY(_cachedTransformComponent->transform.translation);
                float sign    = MathLibrary::GetSign(currY);
                float gravity = -sign * 10.0f * deltaTime;
                // Calculate current velocity based on itself, the deltaVector, and delta
                currentVelocity += gravity * VectorConstants::Up;
                float velY = XMVectorGetY(currentVelocity);
                if (sign > 0)
                        velY = std::max(velY * deltaTime, -currY) / deltaTime;
                else if (sign < 0)
                        velY = std::min(velY * deltaTime, -currY) / deltaTime;

                currentVelocity = XMVectorSetY(currentVelocity, velY);
        }

        if (m_SpeedboostTimer > 0.0f)
        {
                m_SpeedboostTimer -= deltaTime;
        }
        else
        {
                _playerController->SetCurrentMaxSpeed(MathLibrary::MoveTowards(
                    maxSpeed, _playerController->GetMinMaxSpeed(), m_SpeedboostTransitionSpeed * deltaTime));

                m_ExtraYSpeed = 0.0f;
        }


        // Calculate offset
        XMVECTOR offset = currentVelocity * deltaTime;
        _cachedTransformComponent->transform.translation += offset;


        _playerController->SetCurrentVelocity(currentVelocity);
        _playerController->SetEulerAngles(eulerAngles);
}

void PlayerGroundState::Exit()
{
        _playerController->SetCurrentVelocity(XMVectorZero());
}
