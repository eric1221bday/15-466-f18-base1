#include "PhoneBankMode.hpp"

#include "Load.hpp"
#include "MenuMode.hpp"
#include "MeshBuffer.hpp"
#include "Sound.hpp"
#include "compile_program.hpp"  //helper to compile opengl shader programs
#include "data_path.hpp"        //helper to get paths relative to executable
#include "draw_text.hpp"        //helper to... um.. draw text
#include "gl_errors.hpp"        //helper for dumpping OpenGL error messages
#include "read_chunk.hpp"  //helper for reading a vector of structures from a file
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <map>
#include <random>

Load<MeshBuffer> phone_bank_meshes(LoadTagDefault, []() {
  return new MeshBuffer(data_path("phone-bank.pnc"));
});

Load<GLuint> phone_bank_meshes_for_vertex_color_program(LoadTagDefault, []() {
  return new GLuint(
      phone_bank_meshes->make_vao_for_program(vertex_color_program->program));
});

Load<Sound::Sample> sample_dot(LoadTagDefault, []() {
  return new Sound::Sample(data_path("dot.wav"));
});
Load<Sound::Sample> sample_loop(LoadTagDefault, []() {
  return new Sound::Sample(data_path("loop.wav"));
});
Load<Sound::Sample> phone_ring(LoadTagDefault, []() {
  return new Sound::Sample(data_path("telephone-ring-01a.wav"));
});
Load<Sound::Sample> dial_tone(LoadTagDefault, []() {
  return new Sound::Sample(data_path("rotary-phone-2-nr0.wav"));
});

Load<WalkMesh> walk_mesh(LoadTagDefault, []() {
  return new WalkMesh(data_path("phone-bank-walk.blob"));
});

PhoneBankMode::PhoneBankMode()
    : generator(std::time(nullptr)),
      distribution_phones(0, 3),
      distribution_mission(0, 1),
      distribution_answers(0, 4) {
  auto attach_object = [this](Scene::Transform *transform,
                              std::string const &name) {
    Scene::Object *object = scene.new_object(transform);
    object->program = vertex_color_program->program;
    object->program_mvp_mat4 = vertex_color_program->object_to_clip_mat4;
    object->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
    object->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;
    object->vao = *phone_bank_meshes_for_vertex_color_program;
    MeshBuffer::Mesh const &mesh = phone_bank_meshes->lookup(name);
    object->start = mesh.start;
    object->count = mesh.count;
    return object;
  };

  //----------------
  // set up scene:
  std::ifstream file(data_path("phone-bank.scene"), std::ios::binary);

  {
    struct MeshRef {
      int32_t ref;
      uint32_t mesh_name_begin, mesh_name_end;
    };

    struct TransformEntry {
      int32_t parent_ref;
      int32_t ref;
      uint32_t obj_name_begin, obj_name_end;
      glm::vec3 position;
      glm::vec4 rotation;
      glm::vec3 scale;
    };

    std::vector<char> strings;
    std::vector<MeshRef> meshes;
    std::map<int32_t, std::string> mesh_ref;
    std::vector<TransformEntry> transforms;
    read_chunk(file, "str0", &strings);
    read_chunk(file, "xfh0", &transforms);
    read_chunk(file, "msh0", &meshes);

    for (const auto &elem : meshes) {
      mesh_ref[elem.ref] = std::string(&strings[0] + elem.mesh_name_begin,
                                       &strings[0] + elem.mesh_name_end);
    }

    for (const auto &entry : transforms) {
      Scene::Transform *transform = scene.new_transform();
      transform->position = entry.position;
      transform->rotation = glm::quat(entry.rotation.w, entry.rotation.x,
                                      entry.rotation.y, entry.rotation.z);
      transform->scale = entry.scale;
      std::string obj_name(&strings[0] + entry.obj_name_begin,
                           &strings[0] + entry.obj_name_end);

      if (mesh_ref.find(entry.ref) != mesh_ref.end()) {
        auto object = attach_object(transform, mesh_ref.at(entry.ref));
        if (obj_name == "Phone.001") {
          first_phone = object;
          phone_hitbox[first_phone] =
              Box3(transform->position - glm::vec3(1.0f, 1.0f, 1.0f),
                   transform->position + glm::vec3(1.0f, 1.0f, 1.0f));
        } else if (obj_name == "Phone.002") {
          second_phone = object;
          phone_hitbox[second_phone] =
              Box3(transform->position - glm::vec3(1.0f, 1.0f, 1.0f),
                   transform->position + glm::vec3(1.0f, 1.0f, 1.0f));
        } else if (obj_name == "Phone.003") {
          third_phone = object;
          phone_hitbox[third_phone] =
              Box3(transform->position - glm::vec3(1.0f, 1.0f, 1.0f),
                   transform->position + glm::vec3(1.0f, 1.0f, 1.0f));
        } else if (obj_name == "Phone.004") {
          fourth_phone = object;
          phone_hitbox[fourth_phone] =
              Box3(transform->position - glm::vec3(1.0f, 1.0f, 1.0f),
                   transform->position + glm::vec3(1.0f, 1.0f, 1.0f));
        }
      }
    }
  }

  ringing_phone = choose_phone();

  walk_point = walk_mesh->start(glm::vec3(0.0f, -3.0f, 2.5f));
  player_up = walk_mesh->world_normal(walk_point);
  player_at = walk_mesh->world_point(walk_point) + player_up * 1.7f;
  player_right = glm::vec3(1.0f, 0.0f, 0.0f);

  std::cout << glm::to_string(player_up) << std::endl;
  std::cout << glm::to_string(player_right) << std::endl;
  std::cout << glm::to_string(glm::cross(player_up, player_right)) << std::endl;

  {  // Camera looking at the origin:
    Scene::Transform *transform = scene.new_transform();
    transform->position = player_at;
    // Cameras look along -z, so rotate view to look at origin:
    elev_offset = std::atan2f(
        std::sqrtf(player_up.x * player_up.x + player_up.y * player_up.y),
        player_up.z);
    transform->rotation = glm::angleAxis(elev_offset + elevation, player_right);
    camera = scene.new_camera(transform);
  }

  // start the 'loop' sample playing at the first phone:
  loop = sample_loop->play(player_at, 0.2f, Sound::Loop);
  ringing =
      phone_ring->play(ringing_phone->transform->position, 2.0f, Sound::Loop);
}

PhoneBankMode::~PhoneBankMode() {
  if (loop) loop->stop();
  if (ringing) ringing->stop();
}

bool PhoneBankMode::handle_event(SDL_Event const &evt,
                                 glm::uvec2 const &window_size) {
  // ignore any keys that are the result of automatic key repeat:
  if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
    return false;
  }
  // handle tracking the state of WSAD for movement control:
  if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
    if (evt.key.keysym.scancode == SDL_SCANCODE_W) {
      controls.forward = (evt.type == SDL_KEYDOWN);
      return true;
    } else if (evt.key.keysym.scancode == SDL_SCANCODE_S) {
      controls.backward = (evt.type == SDL_KEYDOWN);
      return true;
    } else if (evt.key.keysym.scancode == SDL_SCANCODE_A) {
      controls.left = (evt.type == SDL_KEYDOWN);
      return true;
    } else if (evt.key.keysym.scancode == SDL_SCANCODE_D) {
      controls.right = (evt.type == SDL_KEYDOWN);
      return true;
    }
  }

  // handle activating phone
  if (evt.type == SDL_KEYDOWN &&
      evt.key.keysym.scancode == SDL_SCANCODE_SPACE && selectable_phone) {
    if (selectable_phone == ringing_phone) {
      uint32_t resolve_option = distribution_mission(generator);

      if (resolve_option) {
        mode = mission_mode::TASK;
        answer_index = distribution_answers(generator);
        talk_phone = choose_phone();
        ringing_phone = nullptr;
        ringing->set_volume(0.0f);

      } else {
        merits++;
        ringing_phone = choose_phone();
        ringing->set_position(ringing_phone->transform->position);
        mode = mission_mode::RINGING;
      }
      instruction_countdown = instruction_duration;

    } else {
      show_phone_menu();
    }
  }

  // handle tracking the mouse for rotation control:
  if (!mouse_captured) {
    if (evt.type == SDL_KEYDOWN &&
        evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
      show_pause_menu();
      return true;
    }
    if (evt.type == SDL_MOUSEBUTTONDOWN) {
      SDL_SetRelativeMouseMode(SDL_TRUE);
      mouse_captured = true;
      return true;
    }
  } else if (mouse_captured) {
    if (evt.type == SDL_KEYDOWN &&
        evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
      SDL_SetRelativeMouseMode(SDL_FALSE);
      mouse_captured = false;
      return true;
    }
    if (evt.type == SDL_MOUSEMOTION) {
      // Note: float(window_size.y) * camera->fovy is a pixels-to-radians
      // conversion factor
      float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
      float pitch = evt.motion.yrel / float(window_size.y) * camera->fovy;
      azimuth -= yaw;
      elevation -= pitch;
      camera->transform->rotation =
          glm::normalize(glm::angleAxis(azimuth, player_up) *
                         glm::angleAxis(elev_offset + elevation, player_right));
      handle_phone();
      return true;
    }
  }
  return false;
}

void PhoneBankMode::update(float elapsed) {
  if (merits >= 10) {
    show_win_menu();
  }

  if (strikes >= 3) {
    show_lose_menu();
  }

  glm::mat3 directions = glm::mat3_cast(camera->transform->rotation);
  float amt = 5.0f * elapsed;

  if (controls.right) walk_mesh->walk(walk_point, amt * directions[0]);
  if (controls.left) walk_mesh->walk(walk_point, -amt * directions[0]);
  if (controls.backward) walk_mesh->walk(walk_point, amt * directions[2]);
  if (controls.forward) walk_mesh->walk(walk_point, -amt * directions[2]);

  player_up = walk_mesh->world_normal(walk_point);
  player_at = walk_mesh->world_point(walk_point) + player_up * 1.7f;

  elev_offset = std::atan2f(
      std::sqrtf(player_up.x * player_up.x + player_up.y * player_up.y),
      player_up.z);
  camera->transform->position = player_at;
  camera->transform->rotation =
      glm::normalize(glm::angleAxis(azimuth, player_up) *
                     glm::angleAxis(elev_offset + elevation, player_right));

  handle_phone();

  {  // set sound positions:
    glm::mat4 cam_to_world = camera->transform->make_local_to_world();
    Sound::listener.set_position(cam_to_world[3]);
    // camera looks down -z, so right is +x:
    Sound::listener.set_right(glm::normalize(cam_to_world[0]));

    if (loop) {
      //      glm::mat4 first_phone_to_world =
      //          first_phone->transform->make_local_to_world();
      loop->set_position(player_at);
    }
  }

  if (instruction_countdown > 0.0f) {
    instruction_countdown -= elapsed;
  } else {
    instruction_countdown = 0.0f;
  }
}

void PhoneBankMode::draw(glm::uvec2 const &drawable_size) {
  // set up basic OpenGL state:
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // set up light position + color:
  glUseProgram(vertex_color_program->program);
  glUniform3fv(vertex_color_program->sun_color_vec3, 1,
               glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
  glUniform3fv(vertex_color_program->sun_direction_vec3, 1,
               glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
  glUniform3fv(vertex_color_program->sky_color_vec3, 1,
               glm::value_ptr(glm::vec3(0.4f, 0.4f, 0.45f)));
  glUniform3fv(vertex_color_program->sky_direction_vec3, 1,
               glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
  glUseProgram(0);

  // fix aspect ratio of camera
  camera->aspect = drawable_size.x / float(drawable_size.y);

  scene.draw(camera);

  if (Mode::current.get() == this) {
    glDisable(GL_DEPTH_TEST);
    std::string message;
    if (mouse_captured) {
      message = "ESCAPE TO UNGRAB MOUSE * WASD MOVE";
    } else {
      message = "CLICK TO GRAB MOUSE * ESCAPE QUIT";
    }
    float height = 0.06f;
    float width = text_width(message, height);
    draw_text(message, glm::vec2(-0.5f * width, -0.99f), height,
              glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
    draw_text(message, glm::vec2(-0.5f * width, -1.0f), height,
              glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // phone activation instructions
    if (selectable_phone) {
      std::string phone_name;
      glm::vec4 phone_color;

      if (selectable_phone == first_phone) {
        phone_name = "FIRST PHONE";
        phone_color = glm::vec4(0.0f, 0.0f, 0.5f, 1.0f);
      } else if (selectable_phone == second_phone) {
        phone_name = "SECOND PHONE";
        phone_color = glm::vec4(0.0f, 0.5f, 0.0f, 1.0f);
      } else if (selectable_phone == third_phone) {
        phone_name = "THIRD PHONE";
        phone_color = glm::vec4(0.5f, 0.0f, 0.0f, 1.0f);
      } else {
        phone_name = "FOURTH PHONE";
        phone_color = glm::vec4(0.5f, 0.5f, 0.0f, 1.0f);
      }

      float width = text_width(phone_name, height);
      draw_text(phone_name, glm::vec2(-0.5f * width, -0.4f), height,
                phone_color);
      std::string activation_instruction("PRESS SPACE TO ACTIVATE");
      float instruction_height = 0.04f;
      width = text_width(activation_instruction, instruction_height);
      draw_text(activation_instruction, glm::vec2(-0.5f * width, -0.47f),
                instruction_height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    switch (mode) {
      case mission_mode::RINGING:
        width = text_width(resolution_display, height);
        draw_text(resolution_display, glm::vec2(-0.5f * width, -0.3f), height,
                  glm::vec4(1.0f, 1.0f, 1.0f,
                            instruction_countdown / instruction_duration));
        break;
      case mission_mode::TASK:
        std::string phone_name;
        std::stringstream task_ss;
        if (talk_phone == first_phone) {
          phone_name = "FIRST PHONE";
        } else if (talk_phone == second_phone) {
          phone_name = "SECOND PHONE";
        } else if (talk_phone == third_phone) {
          phone_name = "THIRD PHONE";
        } else {
          phone_name = "FOURTH PHONE";
        }
        task_ss << "CALL THE " << phone_name << " AND SAY "
                << answers[answer_index];
        std::string task_str = task_ss.str();
        width = text_width(task_str, height);
        draw_text(task_str, glm::vec2(-0.5f * width, -0.3f), height,
                  glm::vec4(1.0f, 1.0f, 1.0f,
                            instruction_countdown / instruction_duration));
        break;
    }

    // handle drawing merits
    std::stringstream merit_ss;
    for (uint32_t i = 0; i < merits; i++) {
      merit_ss << " * ";
    }
    std::string merit_str = merit_ss.str();
    width = text_width(merit_str, height);
    draw_text(merit_str, glm::vec2(-0.5f * width, -0.90f), height,
              glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

    // handle drawing strikes
    std::stringstream strike_ss;
    for (uint32_t i = 0; i < strikes; i++) {
      strike_ss << "X";
    }
    std::string strike_str = strike_ss.str();
    height = 0.1f;
    draw_text(strike_str, glm::vec2(0.95f, 0.85f), height,
              glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    glUseProgram(0);
  }

  GL_ERRORS();
}

Scene::Object *PhoneBankMode::choose_phone() {
  uint32_t number = distribution_phones(generator);

  if (number == 0) {
    return first_phone;
  } else if (number == 1) {
    return second_phone;
  } else if (number == 2) {
    return third_phone;
  } else {
    return fourth_phone;
  }
}

void PhoneBankMode::handle_phone() {
  glm::mat3 directions = glm::mat3_cast(camera->transform->rotation);
  auto player_forward = directions[2];
  selectable_phone = nullptr;

  for (auto const &entry : phone_hitbox) {
    float tmin, tmax;
    if (entry.second.intersect(Ray(player_at, -player_forward), tmin, tmax) &&
        glm::distance(player_at, entry.first->transform->position) < 3.0f &&
        tmin > 0.0f) {
      selectable_phone = entry.first;
    }
  }
}

void PhoneBankMode::show_phone_menu() {
  std::shared_ptr<MenuMode> menu = std::make_shared<MenuMode>();

  std::shared_ptr<Mode> game = shared_from_this();
  menu->background = game;

  for (auto const word : answers) {
    menu->choices.emplace_back(word, [word, game, this]() {
      switch (mode) {
        case mission_mode::RINGING:
          strikes++;
          dial_tone->play(selectable_phone->transform->position, 3.0f);
          break;
        case mission_mode::TASK:
          std::cout << "selected: " << word
                    << ", answer is: " << answers[answer_index] << std::endl;
          if (selectable_phone == talk_phone && word == answers[answer_index]) {
            uint32_t resolve_option = distribution_mission(generator);

            if (resolve_option) {
              mode = mission_mode::TASK;
              answer_index = distribution_answers(generator);
              talk_phone = choose_phone();
              ringing_phone = nullptr;
              ringing->set_volume(0.0f);
              instruction_countdown = instruction_duration;

            } else {
              merits++;
              ringing_phone = choose_phone();
              ringing->set_position(ringing_phone->transform->position);
              ringing->set_volume(2.0f);
              mode = mission_mode::RINGING;
            }
          } else {
            strikes++;
            dial_tone->play(selectable_phone->transform->position, 3.0f);
          }

          break;
      }
      Mode::set_current(game);
    });
  }
  menu->choices.emplace_back("CANCEL", [game]() { Mode::set_current(game); });

  menu->selected = 1;

  Mode::set_current(menu);
}

void PhoneBankMode::show_win_menu() {
  std::shared_ptr<MenuMode> menu = std::make_shared<MenuMode>();

  std::shared_ptr<Mode> game = shared_from_this();
  menu->background = game;

  menu->choices.emplace_back("CONGRATULATIONS");
  menu->choices.emplace_back("QUIT", []() { Mode::set_current(nullptr); });

  menu->selected = 1;

  Mode::set_current(menu);
}

void PhoneBankMode::show_lose_menu() {
  std::shared_ptr<MenuMode> menu = std::make_shared<MenuMode>();

  std::shared_ptr<Mode> game = shared_from_this();
  menu->background = game;

  menu->choices.emplace_back("YOU LOSE");
  menu->choices.emplace_back("QUIT", []() { Mode::set_current(nullptr); });

  menu->selected = 1;

  Mode::set_current(menu);
}

void PhoneBankMode::show_pause_menu() {
  std::shared_ptr<MenuMode> menu = std::make_shared<MenuMode>();

  std::shared_ptr<Mode> game = shared_from_this();
  menu->background = game;

  menu->choices.emplace_back("PAUSED");
  menu->choices.emplace_back("RESUME", [game]() { Mode::set_current(game); });
  menu->choices.emplace_back("QUIT", []() { Mode::set_current(nullptr); });

  menu->selected = 1;

  Mode::set_current(menu);
}
