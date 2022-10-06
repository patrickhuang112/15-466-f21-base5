#include "Game.hpp"

#include <ctime>
#include <cmath>

namespace Game {

Scene::Transform *Game::get_free_rocket() {
    assert(!rocket_models.empty());
    Scene::Transform *res = rocket_models[rocket_models.size() - 1];
    rocket_models.pop_back();
    return res;
}
Scene::Transform *Game::get_free_target() {
    assert(!target_models.empty());
    Scene::Transform *res = target_models[target_models.size() - 1];
    target_models.pop_back();
    return res;
}
void Game::release_target(Scene::Transform *target) {
    target->position = DEFAULT_MODEL_POSITION;
    target_models.emplace_back(target);
}
void Game::release_rocket(Scene::Transform *rocket) {
    rocket->position = DEFAULT_MODEL_POSITION;
    rocket_models.emplace_back(rocket); 
}


// xyroll 0 means straight forwad (0,1) (x,y)
// pi == (0,-1) 
// POSITIVE = left/counterclockwise of (0,1)
// NEGATIVE = right/clockwise of (0,1)
// THIS TOOK SO LONG TO GET RIGHT THE MATH OAPIFJSAPOIJF
void Game::shoot(const glm::vec3& pos, float pitch, float xyroll) {
    float halfpi = PI / 2.f;
    float zfrac = -glm::cos(pitch);
    float y = glm::cos(xyroll);
    float x = -glm::sin(xyroll);
    float xyfrac = glm::sqrt(1.f - (zfrac * zfrac));
    glm::vec3 xynorm = glm::normalize(glm::vec3(x,y,0));
    glm::vec3 xyactual = xynorm * xyfrac;
    glm::vec3 velo(xyactual.x, xyactual.y, zfrac);
    velo = velo * ROCKET_SPEED;
    auto rocket = std::make_shared<Rocket>(ROCKET_RADIUS, pos, velo, get_free_rocket()); 
    rockets.emplace_back(rocket); 
    player_shoot_sample = Sound::play(player_shoot_audio);
}

void Game::move_projectiles(float elapsed) {
    for (auto target : targets) {
        target->move(elapsed); 
    }
    for (auto rocket : rockets) {
        rocket->move(elapsed); 
    }
}

bool Projectile::colliding(std::shared_ptr<Projectile> a, std::shared_ptr<Projectile> b) {
    return glm::distance(a->pos, b->pos) <= a->radius + b->radius;
}

bool Game::check_collisions() {
    bool collision = false;
    for (auto rocket : rockets) {
        auto target_it = targets.begin();
        for (auto target : targets) {
            if (!target->remove && Projectile::colliding(rocket, target)) {
                float add = (TARGET_LIFETIME - target->elapsed);
                if (in_bonus) {
                    add += add;
                }
                score += add;
                target->remove = true;
                rocket->remove = true;
                collision = true;
                break;
            }
        }
    }

    std::vector<std::shared_ptr<Rocket>> new_rockets;
    std::vector<std::shared_ptr<Target>> new_targets;
    for (auto rocket : rockets) {
        if (!rocket->remove)  {
            new_rockets.emplace_back(rocket); 
        }
        else {
            release_rocket(rocket->model);
        }
    } 

    for (auto target : targets) {
        if (!target->remove)  {
            new_targets.emplace_back(target); 
        }
        else {
            release_target(target->model); 
        }
    } 

    rockets = new_rockets; 
    targets = new_targets;

    if (collision) {
        hit_sample = Sound::play(hit_audio);
    }
    return collision;
}

// Rockets go left to right?
void Game::launch_new_target() {
    float y = 0.f;
    int xi = static_cast<int>(dis(e) * std::time(0) * dis(e)) % 1000;
    float x = xi / 1000.f - 0.5f;
    float z = static_cast<float>(glm::sqrt(1.f - x * x));

    glm::vec3 velo(x, y, z);
    velo = glm::normalize(velo) * TARGET_SPEED;
    auto target = std::make_shared<Target>(TARGET_RADIUS, TARGET_LAUNCH_POSITION, velo, get_free_target()); 
    targets.emplace_back(target);  

    target_shoot_sample = Sound::play(target_shoot_audio); 
}

void Game::move_bonus_position() {
    int xi = static_cast<int>(dis(e) * std::time(0) * dis(e)) % (xmax - xmin);
    bonus->position = glm::vec3(float(xi) - 10.f, 0.f, 0.f); 
    bonus_sample = Sound::play(bonus_audio); 
}

void Game::play_bonus_timer() {
    if (bonus_timer_sample == nullptr) {
        bonus_timer_sample = Sound::play(bonus_timer_audio);  
    } 
}

void Game::remove_long_lived_projectiles() {
    std::vector<std::shared_ptr<Rocket>> new_rockets;
    std::vector<std::shared_ptr<Target>> new_targets;
    for (auto rocket : rockets) {
        if (rocket->elapsed > ROCKET_LIFETIME)  {
            release_rocket(rocket->model);
        }
        else {
            new_rockets.emplace_back(rocket); 
        }
    } 

    for (auto target : targets) {
        if (target->elapsed > TARGET_LIFETIME)  {
            release_target(target->model);
        }
        else {
            new_targets.emplace_back(target);
        }
    } 

    rockets = new_rockets; 
    targets = new_targets; 
}

void Game::remove_finished_sounds() {
    if (bonus_sample != nullptr && bonus_sample->stopped) {
        bonus_sample = nullptr; 
    }
    if (bonus_timer_sample != nullptr && bonus_timer_sample->stopped) {
        bonus_timer_sample = nullptr; 
    }
    if (player_shoot_sample != nullptr && player_shoot_sample->stopped) {
        player_shoot_sample = nullptr; 
    }
    if (target_shoot_sample != nullptr && target_shoot_sample->stopped) {
        target_shoot_sample = nullptr; 
    } 
    if (hit_sample != nullptr && hit_sample->stopped) {
        hit_sample = nullptr; 
    }
}

void Game::is_in_bonus(const glm::vec3& pos) {
    bool before = in_bonus;
    in_bonus = glm::distance(glm::vec3(pos.x, pos.y, 0.f), bonus->position) < BONUS_RADIUS ? true : false; 
    if (in_bonus && !before) {
        Sound::play(entered_bonus_audio);  
    }
}

}