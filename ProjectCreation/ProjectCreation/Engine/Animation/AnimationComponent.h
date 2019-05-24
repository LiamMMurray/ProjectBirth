#pragma once

#include "../../ECS/ECS.h"

#include "../../Engine/ResourceManager/AnimationClip.h"

class AnimationComponent : public Component<AnimationComponent>
{
        friend class AnimationSystem;

        double                      m_Time = 0.0f;
        std::vector<ResourceHandle> m_Clips;
        std::vector<float>          m_Weights;

    public:
        void SetWeights(int count, float* weights);
        
};
