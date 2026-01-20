#pragma once

#include <stdint.h>
#include <array>
#include <bitset>
#include <memory>

namespace ECS
{

constexpr size_t MAX_COMPONENTS = 1024;

using EntityId = size_t;

using EntityTypeId = size_t;

class Registry;

/**
 * Entity represents an object within the ECS.
 * It holds a unique identifier, a name, and a reference to the Registry
 * that manages its components.
 */
class Entity
{
private:
  EntityTypeId m_TId = 0;
  EntityId     m_Id  = 0;

  Registry* m_Registry = nullptr;

  std::array<std::shared_ptr<void>, MAX_COMPONENTS> m_Components;
  std::vector<size_t>                               m_FreeComponentSlots;

  std::bitset<MAX_COMPONENTS> m_Has;
  std::bitset<MAX_COMPONENTS> m_Dirty;
  std::bitset<MAX_COMPONENTS> m_Removal;

private:
  /**
   * A global counter for unique component type.
   */
  static size_t& GetComponentCounter()
  {
    static size_t counter = size_t(-1);
    return counter;
  }

  /**
   * Retrieve a unique, stable ID for a component type.
   */
  template <typename T> size_t GetComponentTypeId()
  {
    static const size_t id = ++GetComponentCounter();
    return id;
  }

public:
  /**
   * Constructs an Entity with a given name, ID, and registry reference.
   *
   * @param name The name of the entity.
   * @param id The unique identifier for the entity.
   * @param registry A pointer to the Registry managing this entity.
   */
  Entity(EntityTypeId tid, EntityId id, Registry* registry) : m_TId(tid), m_Id(id), m_Registry(registry){};

  ~Entity()
  {
    m_TId      = 0;
    m_Id       = 0;
    m_Registry = nullptr;

    for (auto component : m_Components)
      component.reset();
  }

  /**
   * Retrieves the entity's unique identifier.
   *
   * @return The entity's ID.
   */
  EntityId GetId()
  {
    return m_Id;
  }

  /**
   * Adds a component of type C to the entity.
   *
   * @tparam C Component
   * @tparam CArgs argument for component C
   * @param args Constructor arguments for the component.
   * @return A pointer to the newly created component.
   */
  template <typename C, typename... CArgs> C* Add(CArgs&&... args)
  {
    size_t id        = GetComponentTypeId<C>();
    m_Components[id] = std::make_shared<C>(std::forward<CArgs>(args)...);
    m_Has.set(id, true);
    m_Dirty.set(id, true);
    return static_cast<C*>(m_Components[id].get());
  }

  /**
   * Checks if a specific component type exists.
   *
   * @tparam C Component
   * @return True if the entity has the component, false otherwise.
   */
  template <typename C> bool Has()
  {
    return m_Has.test(GetComponentTypeId<C>());
  }

  /**
   * Retrieves a component of typename C.
   *
   * @tparam C Component
   * @return A pointer to the component, or nullptr if not found.
   */
  template <typename C> C* Get()
  {
    size_t id = GetComponentTypeId<C>();
    if (!m_Has.test(id))
      return nullptr;
    try
    {
      return std::static_pointer_cast<C>(m_Components[id]).get();
    }
    catch (const std::exception& e)
    {
    }
    return nullptr;
  }

  /**
   * Retrieves a component of typename C and creates one if it does not exist.
   *
   * @tparam C Component
   * @tparam CArgs Component args
   * @return A pointer to the component.
   */
  template <typename C, typename... CArgs> C* Ensure(CArgs&&... args)
  {
    size_t id = GetComponentTypeId<C>();
    if (!m_Has.test(id))
      return Add<C>(std::forward<CArgs>(args)...);
    try
    {
      return std::static_pointer_cast<C>(m_Components[id]).get();
    }
    catch (const std::exception& e)
    {
    }
    return nullptr;
  }

  /**
   * Retrieves a components.
   *
   * @tparam C Component
   * @return A pointer to the component, or nullptr if not found.
   */
  template <typename... C> std::tuple<C*...> Collect()
  {
    return std::make_tuple(Get<C>()...);
  }

  /**
   * Remove components from the entity if it exists
   *
   * @tparam C Component
   * @tparam Rest Components
   */
  template <typename C, typename... Rest> void Remove()
  {
    size_t id = GetComponentTypeId<C>();
    if (m_Has.test(id))
    {
      m_Components[id].reset();
      m_Has.set(id, false);
      m_Removal.set(id, false);
    }
    if constexpr (sizeof...(Rest) > 0)
      Remove<Rest...>();
  }

  /**
   * Remove all components from the entity
   */
  void Remove()
  {
    m_Has.reset();
    for (auto component : m_Components)
      component.reset();
  }

  /**
   * Mark components for removal
   *
   * @note The component will be removed once ClearChanges() is called
   *
   * @tparam C Component
   * @tparam Rest Components
   */
  template <typename C, typename... Rest> void MarkForRemoval()
  {
    size_t id = GetComponentTypeId<C>();
    if (m_Has.test(id))
    {
      m_Removal.set(id, true);
      m_Dirty.set(id, true);
    }
    if constexpr (sizeof...(Rest) > 0)
      MarkForRemoval<Rest...>();
  }

  /**
   * Check if a component was marked for removal
   *
   * @tparam C Component
   * @return A bool, true for marked, false otherwise
   */
  template <typename C> bool MarkedForRemoval()
  {
    return m_Removal.test(GetComponentTypeId<C>());
  }

  /**
   * Mark the given components as changed
   *
   * @tparam C Component
   */
  template <typename... C> void MarkChanged()
  {
    (m_Dirty.set(GetComponentTypeId<C>(), true), ...);
  }

  /**
   * Check if a component has changed
   *
   * @tparam C Component
   * @return bool True if changed, false otherwise
   */
  template <typename C> bool HasChanged()
  {
    return m_Dirty.test(GetComponentTypeId<C>());
  }

  /**
   * Check if any of the given components have changed
   *
   * @tparam C Component
   * @return bool True if changed, false otherwise
   */
  template <typename... C> bool AnyChanged()
  {
    return (m_Dirty.test(GetComponentTypeId<C>()) || ...);
  }

  /**
   * Get the component if it changed, nullptr if not
   *
   * @tparam C Component
   * @return A pointer to the component
   */
  template <typename C> C* GetChanged()
  {
    size_t id = GetComponentTypeId<C>();
    if (!m_Has.test(id) || !m_Dirty.test(id))
      return nullptr;
    return static_cast<C*>(m_Components[id].get());
  }

  /**
   * Get the component if it changed, nullptr if not
   *
   * @tparam C Component
   * @return A pointer to the component
   */
  template <typename... C> std::tuple<C*...> CollectChanged()
  {
    return std::make_tuple(GetChanged<C>()...);
  }

  /**
   * Clear changes for components
   *
   * @tparam C Component
   * @tparam Rest Components
   */
  template <typename C, typename... Rest> void ClearChanged()
  {
    size_t id = GetComponentTypeId<C>();

    if (m_Removal.test(id))
    {
      if (m_Has.test(id))
        m_Components[id].reset();
      m_Has.set(id, false);
      m_Removal.set(id, false);
    }

    m_Dirty.set(id, false);

    if constexpr (sizeof...(Rest) > 0)
      ClearChanged<Rest...>();
  }

  /**
   * Clear changes for all components
   *
   * @tparam C Component
   * @tparam Rest Components
   */
  void ClearChanged()
  {
    for (size_t id = 0; id < MAX_COMPONENTS; id++)
    {
      if (m_Removal.test(id))
      {
        if (m_Has.test(id))
          m_Components[id].reset();
        m_Has.set(id, false);
        m_Removal.set(id, false);
      }

      m_Dirty.set(id, false);
    }
  }

  /**
   * Equality operator to compare two entities by their IDs.
   *
   * @param other The other entity to compare.
   * @return True if both entities have the same ID, false otherwise.
   */
  bool operator==(const Entity& other) const
  {
    return m_Id == other.m_Id;
  };

  /**
   * Implicit conversion operator to convert Entity to EntityId.
   *
   * @return The entity's ID.
   */
  operator EntityId() const
  {
    return m_Id;
  }
};
} // namespace ECS
