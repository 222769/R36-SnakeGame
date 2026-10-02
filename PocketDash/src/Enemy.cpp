#include "Enemy.h"

namespace pd {

RectF Enemy::hitbox() const { return RectF{pos.x - 10.0f, pos.y - 10.0f, 20.0f, 12.0f}; }

void Enemy::update(float dt, Vec2 /*playerPos*/, float speedScale) {
    // Phase 3 replaces this with per-type behaviour. For now enemies just
    // integrate their velocity so the update path is exercised.
    if (!alive) return;
    timer += dt;
    pos += vel * (dt * speedScale);
}

const char* enemyTypeName(EnemyType type) {
    switch (type) {
    case EnemyType::Slime: return "SLIME";
    case EnemyType::Beetle: return "BEETLE";
    case EnemyType::Mushroom: return "MUSHROOM";
    case EnemyType::Cloud: return "CLOUD";
    case EnemyType::Robot: return "ROBOT";
    case EnemyType::RollingRock: return "ROLLING ROCK";
    case EnemyType::SpinFlower: return "SPIN FLOWER";
    case EnemyType::Count: break;
    }
    return "UNKNOWN";
}

} // namespace pd
