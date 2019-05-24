#pragma once
#include <assert.h>
#include <stdint.h>
#include <type_traits>
#include <unordered_map>
#include "Component.h"
#include "ComponentManager.h"
#include "ECSTypes.h"
#include "Entity.h"
#include "HandleManager.h"
// TODO implement container type and possibly custom allocator in the future
class EntityManager
{
        HandleManager<IEntity> m_HandleManager;
        ComponentManager*      m_ComponentManager;

    public:
        void     Initialize(ComponentManager* componentManager);
        IEntity* GetEntity(EntityHandle handle);
        template <typename T>
        EntityHandle CreateEntity();
        EntityHandle GetEntityHandle(IEntity* pEntity);

        // NON IMPLEMENTED FUNCTIONS //
        //////////////////////////////
        //////////////////////////////

        template <typename T>
        void DestroyEntity(EntityHandle entityHandle);
        template <typename T>
        void ActivateEntity(EntityHandle entityHandle);
        template <typename T>
        void DeactivateEntity(EntityHandle entityHandle);
        template <typename T>
        size_t GetContainerSize();
        template <typename entityType>
        size_t GetContainerActiveCount();
        template <typename T>
        Component<T>* GetActiveComponents();
        ~EntityManager();

        //////////////////////////////
        //////////////////////////////
};

template <typename T>
inline EntityHandle EntityManager::CreateEntity()
{
        static_assert(std::is_base_of<Entity<T>, T>::value, "CreateEntity can only accept CRTP classes derived from Entity<T>");
        T* _entity                  = new T();
        _entity->m_EntityManager    = this;
        _entity->m_ComponentManager = m_ComponentManager;
        auto id                     = m_HandleManager.GetHandle(_entity);

        return id;
}

template <typename T>
inline void EntityManager::DestroyEntity(EntityHandle entityHandle)
{}

template <typename T>
inline void EntityManager::ActivateEntity(EntityHandle entityHandle)
{}

template <typename T>
inline void EntityManager::DeactivateEntity(EntityHandle entityHandle)
{}

template <typename T>
inline size_t EntityManager::GetContainerSize()
{
        return size_t();
}

template <typename entityType>
inline size_t EntityManager::GetContainerActiveCount()
{
        return size_t();
}

template <typename T>
inline Component<T>* EntityManager::GetActiveComponents()
{
        return NULL;
}


// FOREIGN  TEMPLATE CLASS FUNCTION DEFINITIONS THAT REQUIRE ENTITY MANAGER TO BE DEFINED

template <typename T>
inline void IEntity::AddComponent()
{
        m_ComponentManager->AddComponent<T>(m_EntityManager->GetEntityHandle((IEntity*)this));
}