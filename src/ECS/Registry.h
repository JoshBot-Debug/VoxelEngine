#pragma once

#include <array>
#include <bitset>
#include <memory>
#include <stdint.h>
#include <vector>

#include "Entity.h"
#include <iostream>

namespace ECS {

/**
 * Registry is a container for managing entities and their associated
 * components. It supports adding, retrieving, and deleting components of
 * various types associated with each entity.
 */
class Registry {

private:
  /// @brief Vector of Entities by Entity type id
  std::vector<std::vector<Entity*>> m_EntitiesByETID;

  /// @brief Vector of Entity ids by Entity type id
  std::vector<std::vector<EntityId>> m_FreeEntitySlotsByETID;

  /**
   * Registers a new entity type and returns it's id
   */
  template <typename T>
  EntityTypeId RegisterEntityType() {
    const EntityTypeId id = m_EntitiesByETID.size();
    if (id >= m_EntitiesByETID.size()) {
      m_EntitiesByETID.resize(id + 1);
      m_FreeEntitySlotsByETID.resize(id + 1);
    }
    return id;
  }

  /**
   * Retrieve a unique, stable type ID.
   */
  template <typename T>
  EntityTypeId GetEntityTypeId() {
    static const EntityTypeId id = RegisterEntityType<T>();
    return id;
  }

  /**
   * Retrieve a unique, stable ID.
   *
   * @return A pair containing the entity id and a bool determining if the eid
   * was reused (true) or new (false)
   */
  template <typename T>
  std::pair<EntityId, bool> GetEntityId() {
    auto tid = GetEntityTypeId<T>();

    if (!m_FreeEntitySlotsByETID[tid].empty()) {
      EntityId id = m_FreeEntitySlotsByETID[tid].back();
      m_FreeEntitySlotsByETID[tid].pop_back();
      return {id, true};
    }

    EntityId id = m_EntitiesByETID[tid].size();
    return {id, false};
  }

public:
  Registry() = default;

  ~Registry();

  /**
   * Creates a new entity with a given name.
   *
   * @tparam E Entity
   * @return A pointer to the newly created entity.
   */
  template <typename E>
  Entity* CreateEntity() {
    EntityTypeId tid  = GetEntityTypeId<E>();
    auto [id, reused] = GetEntityId<E>();

    if (reused)
      m_EntitiesByETID[tid][id] = new Entity{tid, id + 1, this};
    else
      m_EntitiesByETID[tid].emplace_back(new Entity{tid, id + 1, this});

    return m_EntitiesByETID[tid][id];
  }

  /**
   * Get an entity by id
   *
   * @tparam E Entity
   * @return A point to the entity or nullptr if it was destroyed
   */
  template <typename E>
  Entity* GetEntity(EntityId id) {
    if (id == 0)
      return nullptr;
    return m_EntitiesByETID[GetEntityTypeId<E>()][id - 1];
  }

  /**
   * Get all entities by type
   *
   * @tparam E Entity
   * @return A vector of entity pointers, may contain nullptrs
   */
  template <typename E>
  std::vector<Entity*> GetEntities() {
    return m_EntitiesByETID[GetEntityTypeId<E>()];
  }

  /**
   * Destroys an entity
   *
   * @tparam E Entity
   */
  template <typename E>
  void DestroyEntity(EntityId id) {
    EntityTypeId tid = GetEntityTypeId<E>();
    delete m_EntitiesByETID[tid][id - 1];
    m_EntitiesByETID[tid][id - 1] = nullptr;
    m_FreeEntitySlotsByETID[tid].push_back(id - 1);
  }

  /**
   * Adds a component typename C to the specified entity.
   *
   * @tparam E Entity
   * @tparam C Component
   * @param id The entity ID to which the component will be added.
   * @param args Constructor arguments for the component.
   * @return A pointer to the newly created component.
   */
  template <typename E, typename C, typename... CArgs>
  C* Add(EntityId id, CArgs&&... args) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return nullptr;
    return entity->Add<C>(std::forward<CArgs>(args)...);
  }

  /**
   * Checks if any entity has a component C.
   *
   * @tparam C Component
   * @return True if the entity has the component, false otherwise.
   */
  template <typename C>
  bool Has() {
    for (auto& entities : m_EntitiesByETID)
      for (Entity* entity : entities)
        if (entity && entity->Has<C>())
          return true;

    return false;
  }

  /**
   * Checks if any entity E has a component C.
   *
   * @tparam E Entity
   * @tparam C Component
   * @return True if the entity has the component, false otherwise.
   */
  template <typename E, typename C>
  bool Has() {
    for (Entity* entity : GetEntities<E>())
      if (entity && entity->Has<C>())
        return true;
    return false;
  }

  /**
   * Checks if an entity E has a component C.
   *
   * @tparam E Entity
   * @tparam C Component
   * @param id The entity id.
   * @return True if the entity has the component, false otherwise.
   */
  template <typename E, typename C>
  bool Has(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return false;
    return entity->Has<C>();
  }

  /**
   * Retrieves a component C from entity E.
   *
   * @tparam E Entity
   * @tparam C Component
   * @param id The entity id
   * @return A pointer to the component, or nullptr if not found.
   */
  template <typename E, typename C>
  C* Get(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return nullptr;
    return entity->Get<C>();
  }

  /**
   * Retrieves a component C from entity E & creates it if it does not exist.
   *
   * @tparam E Entity
   * @tparam C Component
   * @tparam CArgs Component args
   * @param id The entity id
   * @return A pointer to the component.
   */
  template <typename E, typename C, typename... CArgs>
  C* Ensure(EntityId id, CArgs&&... args) {
    Entity* entity = GetEntity<E>(id);
    return entity->Ensure<C>(std::forward<CArgs>(args)...);
  }

  /**
   * Retrieves components C from all entities.
   *
   * @tparam E Entity
   * @tparam C Component
   * @return A vector of pointers to the component & entity.
   */
  template <typename E, typename C>
  std::vector<std::pair<Entity*, C*>> Get() {
    std::vector<std::pair<Entity*, C*>> components;
    for (auto& entities : m_EntitiesByETID)
      for (Entity* entity : entities)
        if (entity && entity->Has<C>())
          components.emplace_back(entity, entity->Get<C>());
    return components;
  }

  /**
   * Removes all components of a specified type for the entity specified if they
   * exist for that entity.
   *
   * @tparam C Component
   * @tparam Rest Components
   * @param id The entity id
   */
  template <typename E, typename C, typename... Rest>
  void Remove(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return;
    entity->Remove<C, Rest...>();
  }

  /**
   * Removes all components from an entity type E if they exist
   *
   * @tparam E Entity
   * @tparam Rest Components
   * @tparam C Component
   */
  template <typename E, typename C, typename... Rest>
  void Remove() {
    for (Entity* entity : GetEntities<E>())
      if (entity)
        entity->Remove<C, Rest...>();
  }

  /**
   * Removes all components from all entites
   */
  void Remove() {
    for (auto& entities : m_EntitiesByETID)
      for (Entity* entity : entities)
        if (entity)
          entity->Remove();
  }

  /**
   * Mark components for removal for an entity
   *
   * @note The component will be removed once ClearChanges() is called
   *
   * @tparam E Entity
   * @tparam C Component
   * @tparam Rest Components
   *
   * @param id The entity id
   */
  template <typename E, typename... C>
  void MarkForRemoval(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return;
    entity->MarkForRemoval<C...>();
  }

  /**
   * Mark components for removal for an entity type
   *
   * @note The component will be removed once ClearChanges() is called
   *
   * @tparam E Entity
   * @tparam C Component
   * @tparam Rest Components
   */
  template <typename E, typename... C>
  void MarkForRemoval() {
    for (Entity* entity : GetEntities<E>())
      if (entity)
        entity->MarkForRemoval<C...>();
  }

  /**
   * Mark components for removal for an entity type
   *
   * @tparam E Entity
   * @tparam C Component
   * @param id Then entity id
   * @tparam Rest Components
   */
  template <typename E, typename C>
  bool MarkedForRemoval(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return false;
    return entity->MarkedForRemoval<C>();
  }

  /**
   * Mark the given components C as changed for an entity E
   *
   * @tparam E Entity
   * @tparam C Component
   * @param id The entity id
   */
  template <typename E, typename... C>
  void MarkChanged(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return;
    entity->MarkChanged<C...>();
  }

  /**
   * Mark the given components C as changed for all entities of type E
   *
   * @tparam E Entity
   * @tparam C Component
   */
  template <typename E, typename... C>
  void MarkChanged() {
    for (Entity* entity : GetEntities<E>())
      if (entity)
        entity->MarkChanged<C...>();
  }

  /**
   * Get a component if it changed for all entities
   *
   * @tparam C Component
   * @return A vector of pointers to the component & entity
   */
  template <typename C>
  std::vector<std::pair<Entity*, C*>> GetChanged() {
    std::vector<std::pair<Entity*, C*>> components;
    for (auto& entities : m_EntitiesByETID)
      for (Entity* entity : entities)
        if (entity)
          if (C* changed = entity->GetChanged<C>())
            components.emplace_back(entity, changed);
    return components;
  }

  /**
   * Get a component if it changed for entities of type E
   *
   * @tparam E Entity
   * @tparam C Component
   * @return A vector of pointers to the component & entity
   */
  template <typename E, typename C>
  std::vector<std::pair<Entity*, C*>> GetChanged() {
    std::vector<std::pair<Entity*, C*>> components;
    for (Entity* entity : GetEntities<E>())
      if (entity)
        if (C* changed = entity->GetChanged<C>())
          components.emplace_back(entity, changed);
    return components;
  }

  /**
   * Get the component if it changed, nullptr if not
   *
   * @tparam E Entity
   * @tparam C Component
   *
   * @param id The entity id
   * @return A pointer to the component or nullptr if nothing changed
   */
  template <typename E, typename C>
  C* GetChanged(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return nullptr;
    return entity->GetChanged<C>();
  }

  /**
   * Get the component if it changed, nullptr if not
   *
   * @tparam E Entity
   * @tparam C Component
   *
   * @param id The entity id
   * @return A pointer to the component or nullptr if nothing changed
   */
  template <typename E, typename... C>
  std::tuple<C*...> CollectChanged(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return std::make_tuple();
    return entity->CollectChanged<C...>();
  }

  /**
   * Get the component if it changed, nullptr if not for all entities of type E
   *
   * @tparam E Entity
   * @tparam C Component
   * @return A vector of pairs with pointers to the component or nullptr if
   * nothing changed & entity
   */
  template <typename E, typename... C>
  std::tuple<C*...> CollectChanged() {
    std::vector<std::pair<Entity*, std::tuple<C*...>>> components;
    for (Entity* entity : GetEntities<E>())
      if (entity)
        components.emplace_back(entity, entity->GetChanged<C...>());
    return components;
  }

  /**
   * Check if a component has changed for an entity
   *
   * @tparam E Entity
   * @tparam C Component
   *
   * @param id The entity id
   * @return bool True if changed, false otherwise
   */
  template <typename E, typename C>
  bool HasChanged() {
    for (Entity* entity : GetEntities<E>())
      if (entity && entity->HasChanged<C>())
        return true;
    return false;
  }

  /**
   * Check if a component has changed for an entity
   *
   * @tparam E Entity
   * @tparam C Component
   *
   * @param id The entity id
   * @return bool True if changed, false otherwise
   */
  template <typename E, typename C>
  bool HasChanged(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return false;
    return entity->HasChanged<C>();
  }

  /**
   * Check if any component has changed for an entity
   *
   * @tparam E Entity
   * @tparam C Component
   *
   * @return bool True if changed, false otherwise
   */
  template <typename E, typename... C>
  bool AnyChanged() {
    for (Entity* entity : GetEntities<E>())
      if (entity && entity->AnyChanged<C...>())
        return true;
    return false;
  }

  /**
   * Check if any component has changed for an entity
   *
   * @tparam E Entity
   * @tparam C Component
   *
   * @param id The entity id
   * @return bool True if changed, false otherwise
   */
  template <typename E, typename... C>
  bool AnyChanged(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return false;
    return entity->AnyChanged<C...>();
  }

  /**
   * Clear changes in an entity
   *
   * @tparam E Entity
   * @tparam C Component
   *
   * @return bool True if changed, false otherwise
   */
  template <typename E, typename... C>
  void ClearChanged() {
    for (Entity* entity : GetEntities<E>())
      if (entity)
        entity->ClearChanged<C...>();
  }

  /**
   * Clear changes in an entity
   *
   * @tparam E Entity
   * @tparam C Component
   *
   * @param id The entity id
   * @return bool True if changed, false otherwise
   */
  template <typename E, typename... C>
  void ClearChanged(EntityId id) {
    Entity* entity = GetEntity<E>(id);
    if (!entity)
      return;
    entity->ClearChanged<C...>();
  }
};
} // namespace ECS
