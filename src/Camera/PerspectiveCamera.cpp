#include "PerspectiveCamera.h"

#include <execution>
#include <glm/gtc/matrix_transform.hpp>

float ToDegree(float degree) {
  return glm::mod(glm::abs(degree), 360.0f) * (degree < 0 ? -1 : 1);
}

void PerspectiveCamera::Update() {

  glm::mat4 pView                  = m_View;
  glm::mat4 pProjection            = m_Projection;
  glm::mat4 pViewProjection        = m_ViewProjection;
  glm::mat4 pInverseView           = m_InverseView;
  glm::mat4 pInverseProjection     = m_InverseProjection;
  glm::mat4 pInverseViewProjection = m_InverseViewProjection;

  m_Front.x = sin(glm::radians(Rotation.y)) * cos(glm::radians(Rotation.x));
  m_Front.y = sin(glm::radians(Rotation.x));
  m_Front.z = -cos(glm::radians(Rotation.y)) * cos(glm::radians(Rotation.x));

  m_Front = glm::normalize(m_Front);

  m_Right = glm::normalize(glm::cross(m_Front, glm::vec3(0.0f, 1.0f, 0.0f)));
  m_Up    = glm::normalize(glm::cross(m_Right, m_Front));

  m_Roll = glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.z), m_Front);

  m_Right = glm::normalize(glm::vec3(m_Roll * glm::vec4(m_Right, 0.0f)));
  m_Up    = glm::normalize(glm::vec3(m_Roll * glm::vec4(m_Up, 0.0f)));

  m_View = glm::lookAt(Position, Position + m_Front, m_Up);

  m_InverseView = glm::inverse(m_View);

  m_Projection = glm::perspective(
      glm::radians(FOV),
      glm::max((float)ViewportWidth / ViewportHeight, 1.0f),
      FarPlane,
      NearPlane);

  // Flip y for vulkan
  m_Projection[1][1] *= -1;

  m_InverseProjection = glm::inverse(m_Projection);

  m_ViewProjection = m_Projection * m_View;

  glm::mat4 inverseViewProjection = glm::inverse(m_ViewProjection);

  m_IsDirty = inverseViewProjection != m_InverseViewProjection;

  if (m_IsDirty) {
    m_pView                  = pView;
    m_pProjection            = pProjection;
    m_pViewProjection        = pViewProjection;
    m_pInverseView           = pInverseView;
    m_pInverseProjection     = pInverseProjection;
    m_pInverseViewProjection = pInverseViewProjection;

    m_InverseViewProjection = inverseViewProjection;
  }
}

void PerspectiveCamera::OnResize(uint32_t width, uint32_t height) {
  ViewportWidth  = width;
  ViewportHeight = height;
}

void PerspectiveCamera::SetPosition(float x, float y, float z) {
  Position.x = x;
  Position.y = y;
  Position.z = z;
}

void PerspectiveCamera::SetRotation(float pitch, float yaw, float roll) {
  Rotation.x = ToDegree(pitch);
  Rotation.y = ToDegree(yaw);
  Rotation.z = ToDegree(roll);
}

void PerspectiveCamera::Translate(float deltaX, float deltaY, float deltaZ) {
  Position += deltaX * m_Right;
  Position += deltaY * m_Up;
  Position += deltaZ * m_Front;
}

void PerspectiveCamera::Rotate(float deltaPitch, float deltaYaw, float deltaRoll) {
  Rotation.x = ToDegree(Rotation.x + deltaPitch);
  Rotation.y = ToDegree(Rotation.y + deltaYaw);
  Rotation.z = ToDegree(Rotation.z + deltaRoll);
}

const glm::mat4& PerspectiveCamera::GetRoll() {
  return m_Roll;
}

const glm::vec3& PerspectiveCamera::GetForward() {
  return m_Front;
}

const glm::vec3& PerspectiveCamera::GetUp() {
  return m_Up;
}

const glm::vec3& PerspectiveCamera::GetRight() {
  return m_Right;
}

void PerspectiveCamera::SetProjection(float fov, float nearPlane, float farPlane) {
  FOV       = fov;
  NearPlane = nearPlane;
  FarPlane  = farPlane;
}

const glm::mat4& PerspectiveCamera::GetViewProjectionMatrix() const {
  return m_ViewProjection;
}

const glm::mat4& PerspectiveCamera::GetInverseViewMatrix() const {
  return m_InverseView;
}

const glm::mat4& PerspectiveCamera::GetInverseProjectionMatrix() const {
  return m_InverseProjection;
}

const glm::mat4& PerspectiveCamera::GetInverseViewProjectionMatrix() const {
  return m_InverseViewProjection;
}

const glm::mat4& PerspectiveCamera::GetProjectionMatrix() const {
  return m_Projection;
}

const glm::mat4& PerspectiveCamera::GetViewMatrix() const { return m_View; }

const glm::mat4& PerspectiveCamera::GetPreviousViewProjectionMatrix() const {
  return m_pViewProjection;
};

const glm::mat4& PerspectiveCamera::GetPreviousProjectionMatrix() const {
  return m_pProjection;
};

const glm::mat4& PerspectiveCamera::GetPreviousViewMatrix() const {
  return m_pView;
};

const glm::mat4& PerspectiveCamera::GetPreviousInverseViewMatrix() const {
  return m_pInverseView;
};

const glm::mat4& PerspectiveCamera::GetPreviousInverseProjectionMatrix() const {
  return m_pInverseProjection;
};

const glm::mat4&
PerspectiveCamera::GetPreviousInverseViewProjectionMatrix() const {
  return m_pInverseViewProjection;
};

const glm::vec3 PerspectiveCamera::GetRayDirection(int pixelX, int pixelY) {
  // 1. Convert pixel to Normalized Device Coordinates [-1, 1]
  float ndcX = (2.0f * pixelX) / ViewportWidth - 1.0f;
  float ndcY = (2.0f * pixelY) / ViewportHeight - 1.0f;

  // 2. Clip space position
  glm::vec4 clipCoords(ndcX, ndcY, -1.0f, 1.0f); // -1 for near plane

  // 3. Unproject to world space
  glm::vec4 worldCoords = m_InverseViewProjection * clipCoords;
  worldCoords /= worldCoords.w;

  // Don't normalize to speed up perf. (since we're on the cpu)
  // 4. Ray direction = from camera position to world point
  return glm::vec3(worldCoords) - Position;
}

bool PerspectiveCamera::IsDirty() { return m_IsDirty; }