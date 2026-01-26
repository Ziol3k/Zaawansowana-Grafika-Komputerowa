#include "Level.h"

LevelRuntime makeRuntime(const LevelSpec& s)
{
    LevelRuntime r;
    r.minXY = s.minXY;
    r.maxXY = s.maxXY;
    r.holePos = s.holePos;
    r.holeRadius = s.holeRadius;

    r.hasWindmill = s.hasWindmill;
    r.windmillCenter = s.windmillCenter;
    r.windmillKick = s.windmillKick;
    r.windmillAngularVel = s.windmillAngularVel;
    r.bladeHalfLen = s.bladeHalfLen;
    r.bladeHalfWid = s.bladeHalfWid;
    r.bladeZ = s.bladeZ;
    r.groundTopZ = 0.0f;
    return r;
}

std::array<LevelSpec, 2> makeDefaultLevels()
{
    std::array<LevelSpec, 2> specs;

    specs[0] = LevelSpec{
        osg::Vec2(-10.0f, -1.5f), osg::Vec2(10.0f, 1.5f),
        osg::Vec3(9.5f, 0, 0), 0.35f,
        osg::Vec3(-9.0f, 0, cfg::BALL_R),
        25.0f, 5.0f, 1.2f,
        false
    };

    specs[1] = LevelSpec{
        osg::Vec2(-12.0f, -2.5f), osg::Vec2(12.0f, 2.5f),
        osg::Vec3(10.5f, 0, 0), 0.35f,
        osg::Vec3(-10.0f, 0, cfg::BALL_R),
        30.0f, 7.0f, 1.2f,
        true, osg::Vec3(0,0,0), 14.0f, 2.5f,
        0.90f, 0.09f, 1.0f
    };

    return specs;
}
