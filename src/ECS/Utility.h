#pragma once

#include "Entity.h"

namespace ECS {

/**
 * @brief Mutates a component value and marks it as changed in the ECS registry.
 *
 * This utility function is typically used to update a component field (or any
 * tracked value) while automatically notifying the ECS registry that the
 * component has been modified.
 *
 * It is especially useful for smooth transitions (e.g., animation,
 * interpolation, or tweening), where you want to know when a value has reached
 * its target.
 *
 * @tparam C The component type associated with the change (used to mark it as
 * changed).
 * @tparam T The value type being mutated (must support != and assignment).
 * @param registry Pointer to the ECS registry that tracks component changes.
 * @param current Reference to the current value to potentially mutate.
 * @param next The target value to update to.
 * @return true if the mutation is complete (i.e., current == next after
 * update), false if mutation is still in progress (i.e., current was different
 * from next).
 */
template <typename C, typename T>
inline bool Mutate(ECS::Entity* entity, T& current, const T& next) {
  bool mutated = current != next;
  if (mutated) {
    current = next;
    entity->MarkChanged<C>();
  }
  return !mutated;
}
} // namespace ECS
