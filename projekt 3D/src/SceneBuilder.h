#pragma once
#include "Level.h"
#include <osg/Group>
#include <osg/Texture2D>
#include <osg/MatrixTransform>
#include <osg/Geometry>


struct SceneNodes {
    osg::ref_ptr<osg::Group> root;
    osg::ref_ptr<osg::Group> levelRoot;
    osg::ref_ptr<osg::MatrixTransform> ballMT;
    osg::ref_ptr<osg::Geometry> aimLineGeom;
    osg::ref_ptr<osg::MatrixTransform> rotorMT;
};

osg::ref_ptr<osg::Texture2D> loadTexture2D(const std::string& path);
SceneNodes createBaseScene(osg::Texture2D* grassTex, osg::Texture2D* woodTex);
void rebuildLevelVisual(const LevelSpec& spec, const LevelRuntime& rt,
                        osg::Group* levelRoot,
                        osg::Texture2D* grassTex, osg::Texture2D* woodTex);
