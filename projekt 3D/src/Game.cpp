#include "Game.h"
#include <osgViewer/GraphicsWindow>
#include <cmath>
#include <algorithm>

Game::Game()
{
    grassTex = loadTexture2D("data/grass.jpg");
    woodTex  = loadTexture2D("data/wood.jpg");

    scene = createBaseScene(grassTex.get(), woodTex.get());

    specs = makeDefaultLevels();
    currentSpec = specs[0];
    level = makeRuntime(currentSpec);
}

void Game::loadLevel(int idx1based)
{
    int idx = std::clamp(idx1based - 1, 0, (int)specs.size()-1);
    currentSpec = specs[(size_t)idx];
    level = makeRuntime(currentSpec);

    rebuildLevelVisual(currentSpec, level, scene.levelRoot.get(), grassTex.get(), woodTex.get());

    camYaw = 0.0f;
    power = 0.0f;
    charging = false;

    resetBall();
    hud.reset();
    updateAimLine();
}

void Game::resetBall()
{
    ball.pos = currentSpec.ballStart;
    ball.vel.set(0,0,0);
    ball.inHole = false;
    scene.ballMT->setMatrix(osg::Matrix::translate(ball.pos));
}

osg::Vec3 Game::computeShotVelocity() const
{
    float speed = cfg::SHOT_SPEED_MAX * cfg::clamp01(power);
    float loft = osg::DegreesToRadians(cfg::clampf(loftDeg, -5.0f, 60.0f));
    float xy = speed * std::cos(loft);
    float z  = speed * std::sin(loft);
    osg::Vec3 dir(std::cos(camYaw), std::sin(camYaw), 0);
    osg::Vec3 v = dir * xy;
    v.z() = z;
    return v;
}

void Game::updateAimLine()
{
    float L = 1.0f + 6.0f * cfg::clamp01(power);
    osg::Vec3 dir(std::cos(camYaw), std::sin(camYaw), 0);

    osg::Vec3 p0 = ball.pos + osg::Vec3(0,0,cfg::AIM_Z_OFFSET);
    osg::Vec3 p1 = p0 + dir * L;

    auto* v = dynamic_cast<osg::Vec3Array*>(scene.aimLineGeom->getVertexArray());
    (*v)[0] = p0;
    (*v)[1] = p1;
    v->dirty();
    scene.aimLineGeom->dirtyBound();
}

void Game::updateCamera()
{
    if (!viewer || freeCam || charging) return;

    osg::Vec3 back(-std::cos(camYaw), -std::sin(camYaw), 0);
    osg::Vec3 eye = ball.pos + back * cfg::CAM_DISTANCE + osg::Vec3(0,0,cfg::CAM_HEIGHT);
    osg::Vec3 center = ball.pos + osg::Vec3(0,0,0.12f);
    viewer->getCamera()->setViewMatrixAsLookAt(eye, center, osg::Vec3(0,0,1));
}

void Game::setCursorGameMode(bool hideCursor)
{
    if (!viewer) return;
    auto* gw = dynamic_cast<osgViewer::GraphicsWindow*>(viewer->getCamera()->getGraphicsContext());
    if (!gw) return;
    gw->setCursor(hideCursor ? osgViewer::GraphicsWindow::NoCursor
                             : osgViewer::GraphicsWindow::LeftArrowCursor);
}
