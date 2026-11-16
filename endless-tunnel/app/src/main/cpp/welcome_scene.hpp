// Copyright (C) Google Inc.
// SPDX-License-Identifier: Apache-2.0
#ifndef endlesstunnel_welcome_scene_hpp
#define endlesstunnel_welcome_scene_hpp

#include "engine.hpp"
#include "our_shader.hpp"
#include "tex_quad.hpp"
#include "ui_scene.hpp"
#include "util.hpp"

/* The "welcome scene" (main menu) */
class WelcomeScene : public UiScene {
 protected:
  // IDs for our buttons:
  int mPlayButtonId;
  int mStoryButtonId;
  int mAboutButtonId;

  virtual void RenderBackground();
  virtual void OnButtonClicked(int id);

  void UpdateWidgetStates();

 public:
  WelcomeScene();
  ~WelcomeScene();

  virtual void OnCreateWidgets();
  virtual void OnStartGraphics();
  virtual void OnKillGraphics();

  virtual void DoFrame();
};

#endif
