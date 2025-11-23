#pragma once

// enum と struct をここに移動

enum class LedClass {
  Off = 0,
  Red,
  Green,
  Blue,
  Orange,
  Yellow,
  Cyan,
  Magenta,
  White,
};

struct LedState {
  bool  on;
  LedClass color;
  float normBrightness; // 0.0〜1.0 (背景差し引き後の明るさ)
  float r, g, b;
};
