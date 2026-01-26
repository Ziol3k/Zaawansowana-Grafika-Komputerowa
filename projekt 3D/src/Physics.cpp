#include "Physics.h"
#include <cmath>
#include <algorithm>

static inline float groundHeightAt(float, float) { return 0.0f; }

bool isStopped(const BallState& b)
{
    return b.vel.length() < cfg::STOP_EPS;
}

static float distancePointOBB2D(const osg::Vec2& p,
                                const osg::Vec2& center,
                                const osg::Vec2& axisX,
                                const osg::Vec2& axisY,
                                float halfX, float halfY)
{
    osg::Vec2 d = p - center;
    float lx = d.x()*axisX.x() + d.y()*axisX.y();
    float ly = d.x()*axisY.x() + d.y()*axisY.y();

    float dx = std::max(std::fabs(lx) - halfX, 0.0f);
    float dy = std::max(std::fabs(ly) - halfY, 0.0f);

    return std::sqrt(dx*dx + dy*dy);
}

static bool rotorHitsBallXY(const LevelRuntime& lvl, const BallState& b, double now, bool nearGround)
{
    if (!lvl.hasWindmill) return false;
    if (!nearGround) return false;

    osg::Vec2 P(b.pos.x(), b.pos.y());
    osg::Vec2 C(lvl.windmillCenter.x(), lvl.windmillCenter.y());

    float a = static_cast<float>(lvl.windmillAngularVel * now);
    float ca = std::cos(a), sa = std::sin(a);

    osg::Vec2 ax(ca, sa);
    osg::Vec2 ay(-sa, ca);

    float minD = 1e9f;

    auto testBlade = [&](const osg::Vec2& localCenter, float hx, float hy)
    {
        osg::Vec2 wc = C + ax * localCenter.x() + ay * localCenter.y();
        float d = distancePointOBB2D(P, wc, ax, ay, hx, hy);
        minD = std::min(minD, d);
    };

    const float L = lvl.bladeHalfLen;
    const float W = lvl.bladeHalfWid;

    // 4 łopatki (krzyż)
    testBlade(osg::Vec2(+L, 0), L, W);
    testBlade(osg::Vec2(-L, 0), L, W);
    testBlade(osg::Vec2(0, +L), W, L);
    testBlade(osg::Vec2(0, -L), W, L);

    return minD < cfg::BALL_R;
}

bool stepBall(BallState& b, const LevelRuntime& lvl, float dt, double now)
{
    bool holeTransition = false;
    if (b.inHole) return false;

    const float g = 9.81f;
    b.vel.z() -= g * dt;
    b.pos += b.vel * dt;

    float ground = groundHeightAt(b.pos.x(), b.pos.y());
    const float groundZ = ground + cfg::BALL_R;
    const bool nearGround = (b.pos.z() <= groundZ + 0.03f);

    if (b.pos.z() < groundZ) {
        b.pos.z() = groundZ;
        if (b.vel.z() < -1.0f) b.vel.z() = -b.vel.z() * 0.30f;
        else b.vel.z() = 0.0f;
    }
    if (nearGround && b.vel.z() < 0.0f && std::fabs(b.vel.z()) < 2.0f) b.vel.z() = 0.0f;

    // Bandy
    if (b.pos.x() < lvl.minXY.x() + cfg::BALL_R) { b.pos.x() = lvl.minXY.x() + cfg::BALL_R; b.vel.x() = -b.vel.x(); }
    if (b.pos.x() > lvl.maxXY.x() - cfg::BALL_R) { b.pos.x() = lvl.maxXY.x() - cfg::BALL_R; b.vel.x() = -b.vel.x(); }
    if (b.pos.y() < lvl.minXY.y() + cfg::BALL_R) { b.pos.y() = lvl.minXY.y() + cfg::BALL_R; b.vel.y() = -b.vel.y(); }
    if (b.pos.y() > lvl.maxXY.y() - cfg::BALL_R) { b.pos.y() = lvl.maxXY.y() - cfg::BALL_R; b.vel.y() = -b.vel.y(); }

    // Wiatrak: kolizja z łopatkami
    if (rotorHitsBallXY(lvl, b, now, nearGround))
    {
        osg::Vec3 d3 = b.pos - lvl.windmillCenter;
        float dist = std::max(d3.length(), 1e-4f);
        osg::Vec3 dir3 = d3 / dist;
        b.vel += dir3 * lvl.windmillKick;
        b.vel.z() += 0.6f;
    }

    // Tarcie XY
    const float f = std::pow(cfg::FRICTION_PER_S, dt);
    b.vel.x() *= f;
    b.vel.y() *= f;
    if (nearGround) {
        if (std::fabs(b.vel.x()) < cfg::STOP_EPS) b.vel.x() = 0.0f;
        if (std::fabs(b.vel.y()) < cfg::STOP_EPS) b.vel.y() = 0.0f;
    }

    // Dołek
    {
        osg::Vec2 dxy(b.pos.x() - lvl.holePos.x(), b.pos.y() - lvl.holePos.y());
        float dist = dxy.length();
        float speedXY = std::sqrt(b.vel.x()*b.vel.x() + b.vel.y()*b.vel.y());

        if (dist < lvl.holeRadius && nearGround && speedXY < cfg::HOLE_SPEED_MAX) {
            b.inHole = true;
            holeTransition = true;
            b.vel.set(0,0,0);

            b.pos.x() = lvl.holePos.x();
            b.pos.y() = lvl.holePos.y();
            b.pos.z() = ground + cfg::BALL_R - cfg::HOLE_DEPTH;
        }
    }

    return holeTransition;
}
