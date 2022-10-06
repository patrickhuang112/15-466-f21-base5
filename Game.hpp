#pragma once


#include "Scene.hpp"
#include "Sound.hpp"
#include "data_path.hpp"

#include <glm/glm.hpp>
#include <stdint.h>
#include <list>
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <random>

namespace Game {
constexpr float ROCKET_LIFETIME = 3.f;
constexpr float TARGET_LIFETIME = 8.f;
constexpr float PI = 3.1415926f;
constexpr float ROCKET_RADIUS = 0.7f;
constexpr float TARGET_RADIUS = 1.f;
constexpr float RELOAD_SPEED = 1.f;
constexpr float ROCKET_SPEED = 30.f;
constexpr float TARGET_SPEED = 20.f;
constexpr float GAME_LENGTH = 60.f;
constexpr float BONUS_RADIUS = 1.f;
constexpr glm::vec3 TARGET_LAUNCH_POSITION = glm::vec3(0.f, 10.f, 2.f);
constexpr glm::vec3 DEFAULT_MODEL_POSITION = glm::vec3(0.f, 0.f, -100.f);
const glm::u8vec4 TEXT_COLOR = glm::u8vec4(0xf4, 0x04, 0x2c, 0x00);
constexpr uint32_t MOVE_BONUS_TIME = 15;
constexpr uint32_t LAUNCH_TARGET_TIME = 3;
constexpr uint32_t CHECK_PROJECTILES_TIMER = 5;
constexpr uint32_t NUM_PATHS = 2;
constexpr const char * BONUS_MOVED_AUDIO = "sounds/moved.opus";
constexpr const char * BONUS_5SEC_AUDIO = "sounds/moving.opus";
constexpr const char * PLAYER_SHOOT_AUDIO = "sounds/shoot.opus";
constexpr const char * TARGET_SHOOT_AUDIO = "sounds/target.opus";
constexpr const char * ENTERED_BONUS_AUDIO = "sounds/in_bonus.opus";
constexpr const char * HIT_AUDIO = "sounds/hit.opus";
constexpr float GRAVITY = 9.8f;

constexpr float TARGET_CLIP_DIST = 500.f;

struct Projectile {
    Projectile (float r, const glm::vec3& pos, const glm::vec3& velo, Scene::Transform *m) {
        radius = r;
        this->pos = pos;
        this->velo = velo;
        elapsed = 0.f; 
        model = m;
        remove = false;
        model->position = pos;
    }
    bool remove;
    float radius; 
    float elapsed;
    glm::vec3 pos;
    glm::vec3 velo; 
    Scene::Transform *model;
    
    virtual void move(float elapsed) = 0;
    static bool colliding(std::shared_ptr<Projectile> a, std::shared_ptr<Projectile> b);
};

struct Target : Projectile {
    Target (float r, const glm::vec3& pos, const glm::vec3& velo, Scene::Transform *m) : Projectile (r, pos, velo, m) {};

    void move(float elapsed) {
        pos.x += velo.x * elapsed;
        pos.y += velo.y * elapsed;
        pos.z += velo.z * elapsed;
        velo.z -= GRAVITY * elapsed;
        this->elapsed += elapsed;
        if (this->model != nullptr) {
            this->model->position = pos; 
        }
    } 
};

struct Rocket : Projectile {
    Rocket (float r, const glm::vec3& pos, const glm::vec3& velo, Scene::Transform *m) : Projectile (r, pos, velo, m) {};

    void move(float elapsed) {
        pos.x += velo.x * elapsed;
        pos.y += velo.y * elapsed;
        pos.z += velo.z * elapsed;
        this->elapsed += elapsed;
        if (this->model != nullptr) {
            this->model->position = pos; 
        }
     
    } 
};

struct Game {
    Game(const Scene& scene) : 
             bonus_audio(data_path(BONUS_MOVED_AUDIO)),
             bonus_timer_audio(data_path(BONUS_5SEC_AUDIO)),
             entered_bonus_audio(data_path(ENTERED_BONUS_AUDIO)),
             player_shoot_audio(data_path(PLAYER_SHOOT_AUDIO)),
             target_shoot_audio(data_path(TARGET_SHOOT_AUDIO)),
             hit_audio(data_path(HIT_AUDIO)) {
        
        // Hardcoded from model being used as platform
        this->xmin = -10;
        this->xmax = 10;

        bonus_sample = nullptr;
        bonus_timer_sample = nullptr;
        player_shoot_sample = nullptr;
        target_shoot_sample = nullptr;
        hit_sample = nullptr; 


        dis = std::uniform_real_distribution(0.f, 1.f);

        for (auto &transform : scene.transforms) {
            Scene::Transform *model = const_cast<Scene::Transform *>(&transform);
            if (transform.name.substr(0, 7) == "Target.") {
                model->position = DEFAULT_MODEL_POSITION;
                target_models.emplace_back(model); 
            }
            else if (transform.name.substr(0, 7) == "Rocket.") {
                model->position = DEFAULT_MODEL_POSITION;
                rocket_models.emplace_back(model);
            } 
            else if (transform.name.substr(0, 7) == "Shooter") {
                model->position = TARGET_LAUNCH_POSITION;
            }
            else if (transform.name.substr(0, 7) == "Bonus") {
                bonus = model;
            } 
        }  
        move_bonus_position();
         
        rockets.clear();
        targets.clear();
        game_over = false;
        in_bonus = false;
        score = 0;
    }


    void shoot(const glm::vec3& pos, float pitch, float xyroll);
    bool check_collisions();
    void move_projectiles(float elapsed);
    void launch_new_target();
    void move_bonus_position();
    void play_bonus_timer();
    void remove_long_lived_projectiles();
    void remove_finished_sounds();
    void is_in_bonus(const glm::vec3& pos);
    bool game_over; 
    bool in_bonus;
    float score;
    
    ~Game() = default;

    private:
        Scene::Transform *get_free_rocket();
        Scene::Transform *get_free_target();
        void release_rocket(Scene::Transform *rocket);
        void release_target(Scene::Transform *target);

        std::default_random_engine e;
        std::uniform_real_distribution<float> dis; // rage 0 - 1
        Scene::Transform *bonus;
        int32_t xmin;
        int32_t xmax;
        Sound::Sample bonus_audio; 
        Sound::Sample bonus_timer_audio; 
        Sound::Sample entered_bonus_audio;
        Sound::Sample player_shoot_audio; 
        Sound::Sample target_shoot_audio; 
        Sound::Sample hit_audio; 
        std::shared_ptr<Sound::PlayingSample> bonus_sample;
        std::shared_ptr<Sound::PlayingSample> bonus_timer_sample;
        std::shared_ptr<Sound::PlayingSample> player_shoot_sample;
        std::shared_ptr<Sound::PlayingSample> target_shoot_sample;
        std::shared_ptr<Sound::PlayingSample> hit_sample; 
        std::vector<std::shared_ptr<Target>> targets;
        std::vector<std::shared_ptr<Rocket>> rockets;
        std::unordered_map<char, Sound::Sample> path_audio;
        std::vector<Scene::Transform *> rocket_models;
        std::vector<Scene::Transform *> target_models;
};

}
