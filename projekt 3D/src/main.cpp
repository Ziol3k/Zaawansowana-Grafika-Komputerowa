#include "Game.h"
#include "Input.h"
#include "Physics.h"
#include <osgViewer/Viewer>
#include <osg/MatrixTransform>

class GameUpdateCallback : public osg::NodeCallback
{
public:
    explicit GameUpdateCallback(Game* g) : _g(g) {}

    void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        if (!_g || !nv || !nv->getFrameStamp()) { traverse(node, nv); return; }

        const double t = nv->getFrameStamp()->getSimulationTime();
        if (_prev < 0.0) _prev = t;
        float dt = static_cast<float>(t - _prev);
        _prev = t;
        dt = cfg::clampf(dt, 0.0f, cfg::DT_MAX);

        if (!_g->freeCam) {
            bool holeTransition = stepBall(_g->ball, _g->level, dt, t);
            if (holeTransition) _g->hud.showHoleIn(t, cfg::HOLE_MSG_SECONDS);

            _g->scene.ballMT->setMatrix(osg::Matrix::translate(_g->ball.pos));
            _g->updateCamera();
        }

        _g->hud.update(t);
        _g->updateAimLine();
        traverse(node, nv);
    }

private:
    Game* _g = nullptr;
    double _prev = -1.0;
};

int main(int argc, char** argv)
{
    osgViewer::Viewer viewer;

    Game game;
    game.viewer = &viewer;

    viewer.setCameraManipulator(nullptr, false);
    viewer.setSceneData(game.scene.root.get());
    viewer.setUpViewInWindow(50, 50, 1280, 720);

    game.hud.create(game.scene.root.get(), 1280, 720);
    game.loadLevel(1);

    game.setCursorGameMode(true);

    viewer.addEventHandler(new InputHandler(&game));
    game.scene.root->setUpdateCallback(new GameUpdateCallback(&game));

    return viewer.run();
}
