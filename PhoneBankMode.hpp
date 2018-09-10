#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "GL.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "WalkMesh.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

// The 'PhoneBankMode' implements the phone bank game:

struct PhoneBankMode : public Mode {
  PhoneBankMode();
  virtual ~PhoneBankMode();

  //handle_event is called when new mouse or keyboard events are received:
  // (note that this might be many times per frame or never)
  //The function should return 'true' if it handled the event.
  virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

  //update is called at the start of a new frame, after events are handled:
  virtual void update(float elapsed) override;

  //draw is called after update:
  virtual void draw(glm::uvec2 const &drawable_size) override;

  //starts up a 'quit/resume' pause menu:
  void show_pause_menu();

  struct {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
  } controls;

  bool mouse_captured = false;

  Scene scene;
  Scene::Camera *camera = nullptr;

  Scene::Object *first_phone = nullptr;
  Scene::Object *second_phone = nullptr;
  Scene::Object *third_phone = nullptr;
  Scene::Object *fourth_phone = nullptr;

  //when this reaches zero, the 'dot' sample is triggered at the small crate:
//  float dot_countdown = 1.0f;

  //this 'loop' sample is played at the large crate:
  std::shared_ptr<Sound::PlayingSample> loop;
};
