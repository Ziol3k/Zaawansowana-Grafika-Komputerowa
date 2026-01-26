#include "Hud.h"
#include "Level.h"
#include <osg/Geode>

static osg::ref_ptr<osg::Camera> createHUDCamera(int w, int h)
{
    osg::ref_ptr<osg::Camera> cam = new osg::Camera;
    cam->setProjectionMatrix(osg::Matrix::ortho2D(0, w, 0, h));
    cam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    cam->setViewMatrix(osg::Matrix::identity());
    cam->setClearMask(GL_DEPTH_BUFFER_BIT);
    cam->setRenderOrder(osg::Camera::POST_RENDER);
    cam->setAllowEventFocus(false);
    cam->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    return cam;
}

void Hud::create(osg::Group* root, int w, int h)
{
    cam = createHUDCamera(w, h);

    osg::ref_ptr<osg::Geode> g = new osg::Geode;

    strokesText = new osgText::Text;
    strokesText->setCharacterSize(22.0f);
    strokesText->setPosition(osg::Vec3(20.0f, h - 40.0f, 0.0f));
    strokesText->setText("Strokes: 0");
    g->addDrawable(strokesText.get());

    holeText = new osgText::Text;
    holeText->setCharacterSize(44.0f);
    holeText->setPosition(osg::Vec3(w * 0.40f, h * 0.50f, 0.0f));
    holeText->setText("");
    g->addDrawable(holeText.get());

    cam->addChild(g.get());
    root->addChild(cam.get());
}

void Hud::reset()
{
    strokes = 0;
    holeMsgVisible = false;
    holeMsgUntil = 0.0;
    if (strokesText.valid()) strokesText->setText("Strokes: 0");
    if (holeText.valid()) holeText->setText("");
}

void Hud::incStroke()
{
    ++strokes;
    if (strokesText.valid()) strokesText->setText("Strokes: " + std::to_string(strokes));
}

void Hud::showHoleIn(double now, double seconds)
{
    holeMsgVisible = true;
    holeMsgUntil = now + seconds;
    if (holeText.valid()) holeText->setText("HOLE IN " + std::to_string(strokes));
}

void Hud::update(double now)
{
    if (!holeMsgVisible) return;
    if (now >= holeMsgUntil) {
        holeMsgVisible = false;
        if (holeText.valid()) holeText->setText("");
    }
}
