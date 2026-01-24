#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <vector>

class PerspectiveCamera {
private:
  glm::mat4 m_View                  = glm::mat4(1.0f);
  glm::mat4 m_Projection            = glm::mat4(0.0f);
  glm::mat4 m_ViewProjection        = glm::mat4(0.0f);
  glm::mat4 m_InverseView           = glm::mat4(0.0f);
  glm::mat4 m_InverseProjection     = glm::mat4(0.0f);
  glm::mat4 m_InverseViewProjection = glm::mat4(0.0f);

  glm::mat4 m_pView                  = glm::mat4(1.0f);
  glm::mat4 m_pProjection            = glm::mat4(0.0f);
  glm::mat4 m_pViewProjection        = glm::mat4(0.0f);
  glm::mat4 m_pInverseView           = glm::mat4(0.0f);
  glm::mat4 m_pInverseProjection     = glm::mat4(0.0f);
  glm::mat4 m_pInverseViewProjection = glm::mat4(0.0f);

  glm::vec3 m_Front = glm::vec3(0.0f);
  glm::vec3 m_Right = glm::vec3(0.0f);
  glm::vec3 m_Up    = glm::vec3(0.0f);
  glm::mat4 m_Roll  = glm::mat4(0.0f);

  bool m_IsDirty = true;

public:
  uint32_t ViewportWidth  = 0;
  uint32_t ViewportHeight = 0;

  glm::vec3 Rotation = glm::vec3(0.0f);
  glm::vec3 Position = glm::vec3(0.0f);

  float FOV       = 45.0f;
  float FarPlane  = 1000.0f;
  float NearPlane = 0.01f;

  void Update();

  void OnResize(uint32_t width, uint32_t height);

  void SetPosition(float x, float y, float z);

  void SetRotation(float pitch, float yaw, float roll);

  void SetProjection(float fov, float nearPlane, float farPlane);

  void Translate(float deltaX, float deltaY, float deltaZ);

  void Rotate(float deltaPitch, float deltaYaw, float deltaRoll);

  const glm::vec3 GetRayDirection(int pixelX, int pixelY);

  const glm::mat4& GetViewProjectionMatrix() const;
  const glm::mat4& GetProjectionMatrix() const;
  const glm::mat4& GetViewMatrix() const;

  const glm::mat4& GetInverseViewMatrix() const;
  const glm::mat4& GetInverseProjectionMatrix() const;
  const glm::mat4& GetInverseViewProjectionMatrix() const;

  const glm::mat4& GetPreviousViewProjectionMatrix() const;
  const glm::mat4& GetPreviousProjectionMatrix() const;
  const glm::mat4& GetPreviousViewMatrix() const;

  const glm::mat4& GetPreviousInverseViewMatrix() const;
  const glm::mat4& GetPreviousInverseProjectionMatrix() const;
  const glm::mat4& GetPreviousInverseViewProjectionMatrix() const;

  bool IsDirty();
};