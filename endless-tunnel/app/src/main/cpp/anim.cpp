// Copyright (C) Google Inc.
// SPDX-License-Identifier: Apache-2.0
#include "anim.hpp"

#include "util.hpp"

void RenderBackgroundAnimation(ShapeRenderer* r) {
  float aspect = SceneManager::GetInstance()->GetScreenAspect();
  static const int BG_RECTS = 50;
  static const float RECT_W = 0.3f;
  static const float RECT_H = 0.1f;
  static float rectX[BG_RECTS];
  static float rectY[BG_RECTS];
  static bool rectsInitted = false;
  int i;

  if (!rectsInitted) {
    for (i = 0; i < BG_RECTS; i++) {
      rectX[i] = aspect * (Random(100) / 100.0f);
      rectY[i] = Random(100) / 100.0f;
    }
    rectsInitted = true;
  }

  glClear(GL_COLOR_BUFFER_BIT);

  for (i = 0; i < BG_RECTS; i++) {
    float c = 0.1f + 0.1f * (i % 4);
    r->SetColor(c, c, c);
    r->RenderRect(rectX[i], rectY[i], RECT_W, RECT_H);

    rectX[i] -= (0.01f + 0.01f * (i % 4));
    if (rectX[i] < -RECT_W * 0.5f) {
      rectX[i] = aspect + RECT_W * 0.5f;
      rectY[i] = Random(100) / 100.0f;
    }
  }
}
