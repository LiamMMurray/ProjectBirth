#include "SystemManager.h"

void SystemManager::Update(float deltaTime)
{
        auto queue = GetSystemQueue();

        while (!queue.empty())
        {
                ISystem* system = queue.top();
                system->OnPreUpdate(deltaTime);
                system->OnUpdate(deltaTime);
                system->OnPostUpdate(deltaTime);
                queue.pop();
        }
}

void SystemManager::InitializeSystem(FSystemInitProperties* systemProperties, ISystem* isystem)
{
        assert(systemProperties != nullptr && system != nullptr);

        isystem->OnInitialize();

        if (systemProperties->bSuspendOnStart)
        {
                mActiveSystemsQueue.push(isystem);
        }
        else
        {
                mInactiveSystems.push_back(isystem);
        }
}

std::priority_queue<ISystem*, std::vector<ISystem*>, SystemManager::PriorityComparator> SystemManager::GetSystemQueue()
{
        return mActiveSystemsQueue;
}

void SystemManager::Initialize()
{
	
}

void SystemManager::Shutdown()
{
        for (auto it : mSystemsMap)
        {
                delete it.second;
        }
}
