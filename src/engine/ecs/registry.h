#pragma once
#include "entity.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <typeindex>

namespace engine::ecs
{
    class Registry
    {
    private:
        Entity _next_entity = 1;
        std::unordered_map<std::type_index, std::unordered_map<Entity, std::shared_ptr<void>>> _components;

    public:
        Entity create()
        {
            return _next_entity++;
        }

        void destroy(Entity entity)
        {
            for (auto& [type, storage] : _components)
            {
                storage.erase(entity);
            }
        }

        template<typename T, typename... Args>
        T& add(Entity entity, Args&&... args)
        {
            auto component = std::make_shared<T>(std::forward<Args>(args)...);
            _components[typeid(T)][entity] = component;
            return *component;
        }

        template<typename T>
        T* get(Entity entity)
        {
            auto type_it = _components.find(typeid(T));
            if (type_it == _components.end())
                return nullptr;

            auto entity_it = type_it->second.find(entity);
            if (entity_it == type_it->second.end())
                return nullptr;

            return static_cast<T*>(entity_it->second.get());
        }

        template<typename T>
        bool has(Entity entity)
        {
            return get<T>(entity) != nullptr;
        }

        template<typename T>
        std::vector<Entity> view()
        {
            std::vector<Entity> result;
            auto it = _components.find(typeid(T));
            if (it != _components.end())
            {
                for (const auto& [entity, _] : it->second)
                {
                    result.push_back(entity);
                }
            }
            return result;
        }
    };
}
