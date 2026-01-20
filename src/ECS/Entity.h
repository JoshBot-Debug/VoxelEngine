#pragma once

#include "Registry.h"
#include <string>

using EntityID = int;

/**
 * Entity represents an object within the ECS.
 * It holds a unique identifier, a name, and a reference to the Registry
 * that manages its components.
 */
class Entity {
private:
  EntityID m_Id;        ///< Unique identifier for the entity.
  std::string m_Name;   ///< Name of the entity.
  Registry *m_Registry; ///< Pointer to the Registry managing this entity.

public:
  /**
   * Constructs an Entity with a given name, ID, and registry reference.
   *
   * @param name The name of the entity.
   * @param id The unique identifier for the entity.
   * @param registry A pointer to the Registry managing this entity.
   */
  Entity(const std::string &name, int id, Registry *registry)
      : m_Id(id), m_Name(name), m_Registry(registry){};

  /**
   * Retrieves the entity's unique identifier.
   *
   * @return The entity's ID.
   */
  EntityID GetId() { return m_Id; }

  /**
   * Adds a component of type T to this entity.
   *
   * @param args Constructor arguments for the component.
   * @return A pointer to the newly created component.
   */
  template <typename T, typename... Args> T *Add(Args &&...args) {
    return m_Registry->Add<T>(m_Id, std::forward<Args>(args)...);
  }

  /**
   * Checks if this entity has a component of type T.
   *
   * @return True if the entity has the component, false otherwise.
   */
  template <typename T> bool Has() { return m_Registry->Has<T>(m_Id); }

  /**
   * Collects components of specified types from this entity.
   *
   * @return A tuple containing pointers to the collected components.
   */
  template <typename... T> std::tuple<T *...> Collect() {
    return m_Registry->Collect<T...>(m_Id);
  }

  /**
   * Retrieves a component of type T from this entity.
   *
   * @return A pointer to the component, or nullptr if not found.
   */
  template <typename T> T *Get() { return m_Registry->Get<T>(m_Id); }

  /**
   * Compares the name of this entity with a given name.
   *
   * @param name The name to compare against.
   * @return True if the names match, false otherwise.
   */
  bool Is(const std::string &name) { return m_Name == name; }

  /**
   * Frees components of specified types from this entity
   * or all components for the entity if none are specified.
   *
   * This method also deletes the entity itself.
   */
  template <typename... T> void Free() {
    (m_Registry->free<T>(m_Id), ...);
    delete this;
  }

  /**
   * Equality operator to compare two entities by their IDs.
   *
   * @param other The other entity to compare.
   * @return True if both entities have the same ID, false otherwise.
   */
  bool operator==(const Entity &other) const { return m_Id == other.m_Id; };

  /**
   * Implicit conversion operator to convert Entity to EntityID.
   *
   * @return The entity's ID.
   */
  operator EntityID() const { return m_Id; }
};