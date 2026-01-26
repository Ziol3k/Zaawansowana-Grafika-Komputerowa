#pragma once
#include <osg/Camera>
#include <osgText/Text>

struct Hud {
    osg::ref_ptr<osg::Camera> cam;
    osg::ref_ptr<osgText::Text> strokesText;
    osg::ref_ptr<osgText::Text> holeText;

    int strokes = 0;
    bool holeMsgVisible = false;
    double holeMsgUntil = 0.0;

    void create(osg::Group* root, int w, int h);
    void reset();
    void incStroke();
    void showHoleIn(double now, double seconds);
    void update(double now);
};
