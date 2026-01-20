#pragma once

#include <any>
#include <unordered_map>
#include <vector>

// This is called all so it's easy to undersand when looking at the
// intellisense for methods, basically it's the initial value which is zero.
// all ids start from 1
const int ALL = 0;

class Entity;

using EntityID = int;

/**
 * Registry is a container for managing entities and their associated
 * components. It supports adding, retrieving, and deleting components of
 * various types associated with each entity.
 */
class Registry {
private:
  EntityID m_EID = ALL;             ///< The next available entity ID.
  std::vector<Entity *> m_Entities; ///< List of all entities in the registry.
  std::unordered_map<EntityID, std::vector<std::any>>
      m_Storage; ///< Storage of components indexed by entity ID.

public:
  Registry() = default;
  ~Registry();

  /**
   * Creates a new entity with a given name.
   *
   * @param name The name of the entity.
   * @return A pointer to the newly created entity.
   */
  Entity *CreateEntity(const char *name);

  /**
   * Adds a component of type T to the specified entity.
   *
   * @param entity The entity ID to which the component will be added.
   * @param args Constructor arguments for the component.
   * @return A pointer to the newly created component.
   */
  template <typename T, typename... Args>
  T *Add(EntityID entity, Args &&...args) {
    T *component = new T(std::forward<Args>(args)...);
    m_Storage[entity].push_back((std::any)component);
    return component;
  }

  /**
   * Checks if an entity has a specific component type.
   *
   * @param entity The entity ID to check.
   * @return True if the entity has the component, false otherwise.
   */
  template <typename T> bool Has(EntityID entity) {
    return (m_Storage.find(entity) != m_Storage.end());
  }

  /**
   * Collects components of specified types from a single entity.
   *
   * @param entity The entity ID from which to collect components.
   * @return A tuple containing pointers to the components.
   */
  template <typename... T> std::tuple<T *...> Collect(EntityID entity) {
    return std::make_tuple(Get<T>(entity)...);
  }

  /**
   * Retrieves a component of type T from a specified entity.
   *
   * @param entity The entity ID from which to retrieve the component.
   * @return A pointer to the component, or nullptr if not found.
   */
  template <typename T> T *Get(EntityID entity) {
    for (auto &component : m_Storage[entity])
      try {
        return std::any_cast<T *>(component);
      } catch (const std::bad_any_cast &e) {
      }
    return nullptr;
  }

  /**
   * Collects all components of specified types across all entities.
   *
   * @return A tuple containing vectors of pointers to the components.
   */
  template <typename... T> std::tuple<std::vector<T *>...> Collect() {
    return std::make_tuple(Get<T>()...);
  }

  /**
   * Retrieves all components of a specified type across all entities.
   *
   * @return A vector of pointers to the components.
   */
  template <typename T> std::vector<T *> Get() {
    std::vector<T *> result;

    for (const auto &[eid, components] : m_Storage)
      for (auto &component : components)
        try {
          result.push_back(std::any_cast<T *>(component));
        } catch (const std::bad_any_cast &e) {
        }

    return result;
  }

  /**
   * Retrieves all entities in the registry.
   *
   * @return A vector of pointers to all entities.
   */
  std::vector<Entity *> Entities() { return m_Entities; }

  /**
   * Frees a specific component type from the specified entity.
   *
   * @param entity The entity ID from which to free the component.
   */
  template <typename T> void Free(EntityID entity) {
    if (m_Storage.find(entity) == m_Storage.end())
      return;

    for (const auto &component : m_Storage[entity]) {
      try {
        delete std::any_cast<T *>(component);
      } catch (const std::bad_any_cast &e) {
        // The cast failed, this is no the object we want, skip.
      }
    }
  }

  /**
   * Frees all components of a specified type across all entities.
   */
  template <typename... T> void free() {
    for (const auto [eid, components] : m_Storage)
      (Free<T>(eid), ...);
  }
};