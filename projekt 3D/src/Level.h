#pragma once
#include <osg/Vec2>
#include <osg/Vec3>
#include <array>

namespace cfg {
inline constexpr float BALL_R = 0.20f;
inline constexpr float STOP_EPS = 0.05f;
inline constexpr float HOLE_DEPTH = 0.55f;
inline constexpr float HOLE_SPEED_MAX = 2.0f;
inline constexpr float FRICTION_PER_S = 0.60f;
inline constexpr float DT_MAX = 0.05f;

inline constexpr float AIM_Z_OFFSET = 0.02f;
inline constexpr float POWER_GAIN = 0.0045f;
inline constexpr float SHOT_SPEED_MAX = 15.0f;

inline constexpr float CAM_DISTANCE = 10.0f;
inline constexpr float CAM_HEIGHT = 3.0f;

inline constexpr float GRASS_THICKNESS = 0.14f;
inline constexpr float HOLE_MSG_SECONDS = 2.0f;

inline float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
}

struct LevelSpec {
    osg::Vec2 minXY, maxXY;

    osg::Vec3 holePos;
    float holeRadius = 0.35f;

    osg::Vec3 ballStart;

    float grassSX = 25.0f;
    float grassSY = 5.0f;
    float tilesPerMeter = 1.2f;

    bool hasWindmill = false;
    osg::Vec3 windmillCenter = osg::Vec3(0,0,0);
    float windmillKick = 10.0f;
    float windmillAngularVel = 2.0f; // rad/s

    float bladeHalfLen = 0.90f; 
    float bladeHalfWid = 0.09f; 
    float bladeZ = 1.0f;  
};

struct LevelRuntime {
    osg::Vec2 minXY, maxXY;

    osg::Vec3 holePos;
    float holeRadius = 0.35f;

    bool hasWindmill = false;
    osg::Vec3 windmillCenter = osg::Vec3(0,0,0);
    float windmillKick = 14.0f;
    float windmillAngularVel = 2.5f;

    float bladeHalfLen = 0.90f;
    float bladeHalfWid = 0.09f;
    float bladeZ = 1.0f;

    float groundTopZ = 0.0f;
};

LevelRuntime makeRuntime(const LevelSpec& spec);
std::array<LevelSpec, 2> makeDefaultLevels();
