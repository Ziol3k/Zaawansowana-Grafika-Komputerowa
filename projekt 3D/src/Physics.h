#pragma once
#include "Level.h"
#include <osg/Vec2>
#include <osg/Vec3>

struct BallState {
    osg::Vec3 pos = osg::Vec3(0,0,cfg::BALL_R);
    osg::Vec3 vel = osg::Vec3(0,0,0);
    bool inHole = false;
};

bool isStopped(const BallState& b);

// Zwraca true jeśli w tej klatce nastąpiło "now in hole" 
bool stepBall(BallState& b, const LevelRuntime& lvl, float dt, double now);
