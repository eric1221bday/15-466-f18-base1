#pragma once

#include "Mode.hpp"

#include "GL.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "WalkMesh.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"

#include <ctime>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

// The 'PhoneBankMode' implements the phone bank game:

struct PhoneBankMode : public Mode {
  PhoneBankMode();
  virtual ~PhoneBankMode();

  // handle_event is called when new mouse or keyboard events are received:
  // (note that this might be many times per frame or never)
  // The function should return 'true' if it handled the event.
  virtual bool handle_event(SDL_Event const &evt,
                            glm::uvec2 const &window_size) override;

  // update is called at the start of a new frame, after events are handled:
  virtual void update(float elapsed) override;

  // draw is called after update:
  virtual void draw(glm::uvec2 const &drawable_size) override;

  // starts up a 'quit/resume' pause menu:
  void show_pause_menu();

  // handles phone selection and menus;
  void handle_phone();

  void show_phone_menu();

  void show_win_menu();

  void show_lose_menu();

  Scene::Object *choose_phone();

  struct {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
  } controls;

  // for intersection test purposes
  // from
  // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
  struct Ray {
    glm::vec3 orig, dir;  // ray orig and dir
    glm::vec3 invdir;
    int32_t sign[3];

    Ray(const glm::vec3 &orig, const glm::vec3 &dir) : orig(orig), dir(dir) {
      invdir = 1.0f / dir;
      sign[0] = (invdir.x < 0);
      sign[1] = (invdir.y < 0);
      sign[2] = (invdir.z < 0);
    }
  };

  // for intersection test purposes
  // from
  // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
  struct Box3 {
    glm::vec3 bounds[2];

    Box3(const glm::vec3 &vmin, const glm::vec3 &vmax) {
      bounds[0] = vmin;
      bounds[1] = vmax;
    }

    Box3() = default;

    bool intersect(const Ray &r, float &tmin, float &tmax) const {
      float tymin, tymax, tzmin, tzmax;

      tmin = (bounds[r.sign[0]].x - r.orig.x) * r.invdir.x;
      tmax = (bounds[1 - r.sign[0]].x - r.orig.x) * r.invdir.x;
      tymin = (bounds[r.sign[1]].y - r.orig.y) * r.invdir.y;
      tymax = (bounds[1 - r.sign[1]].y - r.orig.y) * r.invdir.y;

      if ((tmin > tymax) || (tymin > tmax)) return false;
      if (tymin > tmin) tmin = tymin;
      if (tymax < tmax) tmax = tymax;

      tzmin = (bounds[r.sign[2]].z - r.orig.z) * r.invdir.z;
      tzmax = (bounds[1 - r.sign[2]].z - r.orig.z) * r.invdir.z;

      if ((tmin > tzmax) || (tzmin > tmax)) return false;
      if (tzmin > tmin) tmin = tzmin;
      if (tzmax < tmax) tmax = tzmax;

      return true;
    }
  };

  enum class mission_mode {
    RINGING,
    TASK,
  };

  bool mouse_captured = false;

  Scene scene;
  Scene::Camera *camera = nullptr;

  Scene::Object *first_phone = nullptr;
  Scene::Object *second_phone = nullptr;
  Scene::Object *third_phone = nullptr;
  Scene::Object *fourth_phone = nullptr;

  Scene::Object *selectable_phone = nullptr;
  Scene::Object *ringing_phone = nullptr;
  Scene::Object *talk_phone = nullptr;

  std::unordered_map<Scene::Object *, Box3> phone_hitbox;

  WalkMesh::WalkPoint walk_point;
  glm::vec3 player_at, player_up, player_right;
  float azimuth = 0.0f, elevation = float(M_PI_2);
  float elev_offset;

  uint32_t merits = 0, strikes = 0;

  mission_mode mode = mission_mode::RINGING;

  // when this reaches zero, the 'dot' sample is triggered at the small crate:
  //  float dot_countdown = 1.0f;
  float instruction_countdown = 0.0f;
  float instruction_duration = 3.0f;

  std::default_random_engine generator;
  std::uniform_int_distribution<uint32_t> distribution_phones,
      distribution_mission, distribution_answers;

  uint32_t answer_index;

  // this 'loop' sample is played at the large crate:
  std::shared_ptr<Sound::PlayingSample> loop;
  std::shared_ptr<Sound::PlayingSample> ringing;

  std::vector<std::string> answers = {
      {"HERON", "DOG", "FISH", "MONKEY", "BIRD"}};
  std::string resolution_display = "JUST CHECKING IN";
};
