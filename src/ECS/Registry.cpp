#include "Registry.h"
#include "Entity.h"

Registry::~Registry() {
  for (auto entity : m_Entities)
    delete entity;
}

Entity *Registry::CreateEntity(const char *name) {
  ++m_EID;
  Entity *entity = new Entity(name, m_EID, this);
  m_Entities.push_back(entity);
  return entity;
}