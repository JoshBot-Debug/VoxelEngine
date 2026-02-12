#pragma once

namespace vxen {
enum Binding {

  T_DEPTH                 = 101,
  T_NORMAL                = 102,
  T_MATERIAL              = 103,
  T_MOTION_VECTOR         = 104,
  T_DIRECT_LIGHT          = 105,
  T_SHADOW                = 106,
  T_AMBIENT_OCCLUSION     = 107,
  T_SSAO_NOISE            = 108,
  T_SHADING               = 109,
  T_SSAO_SAMPLE_COUNT     = 110,
  T_BILATERAL_BLUR_INPUT  = 111,
  T_BILATERAL_BLUR_OUTPUT = 112,
  T_REFLECTION            = 113,
  T_SKYBOX                = 114,

  S_SVO           = 50,
  S_MATERIALS     = 51,
  S_MATERIALS_LUT = 52,
  S_LIGHTS        = 53,

  U_CAMERA   = 0,
  U_METADATA = 1,

};
} // namespace vxen
