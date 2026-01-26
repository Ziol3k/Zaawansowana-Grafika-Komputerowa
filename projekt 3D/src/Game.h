#pragma once
#include "Level.h"
#include "Physics.h"
#include "Hud.h"
#include "SceneBuilder.h"

#include <osgGA/TrackballManipulator>
#include <osgViewer/Viewer>

struct Game
{
    osgViewer::Viewer* viewer = nullptr;

    osg::ref_ptr<osg::Texture2D> grassTex;
    osg::ref_ptr<osg::Texture2D> woodTex;

    SceneNodes scene;
    Hud hud;

    std::array<LevelSpec, 2> specs;
    LevelSpec currentSpec;
    LevelRuntime level;
    BallState ball;

    bool freeCam = false;
    bool charging = false;

    osg::ref_ptr<osgGA::TrackballManipulator> tb = new osgGA::TrackballManipulator;

    float camYaw = 0.0f;
    float loftDeg = 10.0f;
    float power = 0.0f;

    Game();

    void loadLevel(int idx1based);
    void resetBall();
    void updateAimLine();
    void updateCamera();
    osg::Vec3 computeShotVelocity() const;

    void setCursorGameMode(bool hideCursor);
};
