#pragma once
#include "../ECS/ECSTypes.h"
class IEventDelegate;
typedef ECSTypeId<IEventDelegate> EventDelegateTypeId;
class IEventDelegate
{
    public:
        IEventDelegate()
        {}

        virtual ~IEventDelegate()
        {}

        virtual const EventDelegateTypeId GetStaticTypeId() const = 0;
        virtual void                      Invoke(IEvent* e)       = 0;
};