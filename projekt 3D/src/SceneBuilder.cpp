#include "SceneBuilder.h"
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/LineWidth>
#include <osg/Program>
#include <osg/Shader>
#include <osg/Uniform>
#include <osgDB/ReadFile>
#include <algorithm>
#include <osg/AnimationPath>

osg::ref_ptr<osg::Texture2D> loadTexture2D(const std::string& path)
{
    osg::ref_ptr<osg::Image> img = osgDB::readImageFile(path);
    osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D;
    if (img.valid()) tex->setImage(img.get());
    tex->setDataVariance(osg::Object::STATIC);
    tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR_MIPMAP_LINEAR);
    tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
    tex->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::REPEAT);
    tex->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::REPEAT);
    return tex;
}

static void applyTexture(osg::StateSet* ss, osg::Texture2D* tex)
{
    if (!ss || !tex) return;
    ss->setTextureAttributeAndModes(0, tex, osg::StateAttribute::ON);
}

static osg::ref_ptr<osg::Geometry> makeAimLineGeometry()
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;

    osg::ref_ptr<osg::Vec3Array> v = new osg::Vec3Array;
    v->push_back(osg::Vec3(0,0,0));
    v->push_back(osg::Vec3(1,0,0));
    geom->setVertexArray(v.get());
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 2));

    osg::ref_ptr<osg::Vec4Array> c = new osg::Vec4Array;
    c->push_back(osg::Vec4(1,1,1,1));
    geom->setColorArray(c.get(), osg::Array::BIND_OVERALL);

    osg::StateSet* ss = geom->getOrCreateStateSet();
    ss->setAttributeAndModes(new osg::LineWidth(3.0f), osg::StateAttribute::ON);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    ss->setRenderBinDetails(11, "RenderBin");

    geom->setDataVariance(osg::Object::DYNAMIC);
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);
    return geom;
}

static osg::ref_ptr<osg::Geode> makeTexturedBox(
    const osg::Vec3& center, float sx, float sy, float sz,
    osg::Texture2D* tex, const osg::Vec4& fallbackColor)
{
    osg::ref_ptr<osg::Box> box = new osg::Box(center, sx, sy, sz);
    osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(box.get());
    sd->setColor(fallbackColor);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(sd.get());

    applyTexture(geode->getOrCreateStateSet(), tex);
    return geode;
}

static osg::ref_ptr<osg::Geode> makeSolidGroundSlab(float sx, float sy, float topZ, float thickness, const osg::Vec4& color)
{
    osg::ref_ptr<osg::Box> box = new osg::Box(osg::Vec3(0,0, topZ - thickness*0.5f), sx, sy, thickness);
    osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(box.get());
    sd->setColor(color);

    osg::ref_ptr<osg::Geode> g = new osg::Geode;
    g->addDrawable(sd.get());
    return g;
}

static osg::ref_ptr<osg::Geode> makeRoundHole(const osg::Vec3& holePos, float holeRadius, float groundTopZ)
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;

    osg::Vec3 center = holePos + osg::Vec3(0, 0, groundTopZ - cfg::HOLE_DEPTH * 0.5f);
    osg::ref_ptr<osg::Cylinder> wall = new osg::Cylinder(center, holeRadius * 0.95f, cfg::HOLE_DEPTH);

    osg::ref_ptr<osg::ShapeDrawable> wallSD = new osg::ShapeDrawable(wall.get());
    wallSD->setColor(osg::Vec4(0,0,0,1));
    geode->addDrawable(wallSD.get());

    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setRenderBinDetails(12, "RenderBin");
    ss->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
    //ss->setMode(GL_LIGHTING, osg::StateAttribute::ON);
    return geode;
}

static osg::ref_ptr<osg::Geode> makeGrassPlaneWithHoleShader(
    const osg::Vec3& centerXY, float sx, float sy, float z,
    osg::Texture2D* grassTex,
    const osg::Vec3& holePos, float holeRadius,
    float tilesPerMeter)
{
    const float u = sx * tilesPerMeter;
    const float v = sy * tilesPerMeter;

    osg::ref_ptr<osg::Geometry> geom = osg::createTexturedQuadGeometry(
        osg::Vec3(centerXY.x() - sx*0.5f, centerXY.y() - sy*0.5f, z),
        osg::Vec3(sx, 0, 0),
        osg::Vec3(0, sy, 0),
        u, v
    );

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(geom.get());

    osg::StateSet* ss = geode->getOrCreateStateSet();
    applyTexture(ss, grassTex);

    static const char* vs = R"glsl(
        varying vec2 vWorldXY;
        varying vec2 vUV;
        void main()
        {
            vWorldXY = gl_Vertex.xy;
            vUV = gl_MultiTexCoord0.st;
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
        }
    )glsl";

    static const char* fs = R"glsl(
        uniform sampler2D uTex;
        uniform vec2 uHolePosXY;
        uniform float uHoleRadius;
        varying vec2 vWorldXY;
        varying vec2 vUV;
        void main()
        {
            float d = distance(vWorldXY, uHolePosXY);
            if (d < uHoleRadius) discard;
            gl_FragColor = texture2D(uTex, vUV);
        }
    )glsl";

    osg::ref_ptr<osg::Program> program = new osg::Program;
    program->addShader(new osg::Shader(osg::Shader::VERTEX, vs));
    program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fs));
    ss->setAttributeAndModes(program.get(), osg::StateAttribute::ON);

    ss->addUniform(new osg::Uniform("uTex", 0));
    ss->addUniform(new osg::Uniform("uHolePosXY", osg::Vec2(holePos.x(), holePos.y())));
    ss->addUniform(new osg::Uniform("uHoleRadius", holeRadius));
    return geode;
}

SceneNodes createBaseScene(osg::Texture2D* /*grassTex*/, osg::Texture2D* /*woodTex*/)
{
    SceneNodes n;
    n.root = new osg::Group;
    n.levelRoot = new osg::Group;
    n.root->addChild(n.levelRoot.get());

    // piłka
    n.ballMT = new osg::MatrixTransform;
    {
        osg::ref_ptr<osg::Sphere> sphere = new osg::Sphere(osg::Vec3(0,0,0), cfg::BALL_R);
        osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(sphere.get());
        sd->setColor(osg::Vec4(1,1,1,1));
        osg::ref_ptr<osg::Geode> g = new osg::Geode;
        g->addDrawable(sd.get());
        n.ballMT->addChild(g.get());
        n.root->addChild(n.ballMT.get());
    }

    // aim
    n.aimLineGeom = makeAimLineGeometry();
    {
        osg::ref_ptr<osg::Geode> g = new osg::Geode;
        g->addDrawable(n.aimLineGeom.get());
        n.root->addChild(g.get());
    }

    return n;
}

static osg::ref_ptr<osg::AnimationPath> createRotorAnimationPath(float angularVelRadPerSec)
{
    osg::ref_ptr<osg::AnimationPath> ap = new osg::AnimationPath;
    ap->setLoopMode(osg::AnimationPath::LOOP);

    const int steps = 16;
    const float w = std::max(angularVelRadPerSec, 1e-3f);
    const float period = 2.0f * osg::PI / w;

    for (int i = 0; i <= steps; ++i) {
        float tt = (period * i) / steps;
        float a  = (2.0f * osg::PI * i) / steps;
        osg::Quat q(a, osg::Vec3(0,0,1));
        ap->insert(tt, osg::AnimationPath::ControlPoint(osg::Vec3(0,0,0), q));
    }
    return ap;
}

static void buildWindmillVisual(const LevelSpec& spec, osg::Group* levelRoot, osg::Texture2D* woodTex)
{
    if (!spec.hasWindmill) return;

    osg::ref_ptr<osg::MatrixTransform> rotor = new osg::MatrixTransform;
    rotor->setMatrix(osg::Matrix::translate(spec.windmillCenter + osg::Vec3(0,0,spec.bladeZ)));

    const float bladeLen = spec.bladeHalfLen * 2.0f;
    const float bladeWid = spec.bladeHalfWid * 2.0f;
    const float bladeTh  = 0.18f;

    osg::ref_ptr<osg::Group> blades = new osg::Group;
    blades->addChild(makeTexturedBox(osg::Vec3(+spec.bladeHalfLen,0,0), bladeLen, bladeWid, bladeTh, woodTex, osg::Vec4(0.6f,0.4f,0.25f,1)).get());
    blades->addChild(makeTexturedBox(osg::Vec3(-spec.bladeHalfLen,0,0), bladeLen, bladeWid, bladeTh, woodTex, osg::Vec4(0.6f,0.4f,0.25f,1)).get());
    blades->addChild(makeTexturedBox(osg::Vec3(0,+spec.bladeHalfLen,0), bladeWid, bladeLen, bladeTh, woodTex, osg::Vec4(0.6f,0.4f,0.25f,1)).get());
    blades->addChild(makeTexturedBox(osg::Vec3(0,-spec.bladeHalfLen,0), bladeWid, bladeLen, bladeTh, woodTex, osg::Vec4(0.6f,0.4f,0.25f,1)).get());

    rotor->addChild(blades.get());

    osg::ref_ptr<osg::AnimationPath> ap = createRotorAnimationPath(spec.windmillAngularVel);
    rotor->setUpdateCallback(new osg::AnimationPathCallback(ap.get(), 0.0, 1.0));
    levelRoot->addChild(rotor.get());
}

static void buildWallsVisual(const LevelRuntime& rt, osg::Group* levelRoot, osg::Texture2D* woodTex)
{
    auto addWall = [&](const osg::Vec3& c, float sx, float sy, float sz)
    {
        levelRoot->addChild(makeTexturedBox(c, sx, sy, sz, woodTex,
                                            osg::Vec4(0.55f,0.35f,0.2f,1.0f)).get());
    };

    const float sx = (rt.maxXY.x() - rt.minXY.x()) + 5.0f;
    const float sy = (rt.maxXY.y() - rt.minXY.y()) + 2.0f;

    addWall(osg::Vec3(0, rt.minXY.y()-0.2f, 0.3f), sx, 0.4f, 0.6f);
    addWall(osg::Vec3(0, rt.maxXY.y()+0.2f, 0.3f), sx, 0.4f, 0.6f);
    addWall(osg::Vec3(rt.minXY.x()-0.2f, 0, 0.3f), 0.4f, sy, 0.6f);
    addWall(osg::Vec3(rt.maxXY.x()+0.2f, 0, 0.3f), 0.4f, sy, 0.6f);
}

void rebuildLevelVisual(const LevelSpec& spec, const LevelRuntime& rt,
                        osg::Group* levelRoot,
                        osg::Texture2D* grassTex, osg::Texture2D* woodTex)
{
    levelRoot->removeChildren(0, levelRoot->getNumChildren());

    const float topZ = rt.groundTopZ;

    levelRoot->addChild(
        makeSolidGroundSlab(spec.grassSX, spec.grassSY, topZ, cfg::GRASS_THICKNESS,
                            osg::Vec4(0.06f,0.06f,0.06f,1.0f)).get());

    levelRoot->addChild(
        makeGrassPlaneWithHoleShader(osg::Vec3(0,0,0),
                                     spec.grassSX, spec.grassSY, topZ + 0.001f,
                                     grassTex,
                                     rt.holePos, rt.holeRadius,
                                     spec.tilesPerMeter).get());

    buildWallsVisual(rt, levelRoot, woodTex);
    buildWindmillVisual(spec, levelRoot, woodTex);
    levelRoot->addChild(makeRoundHole(rt.holePos, rt.holeRadius, topZ).get());
}
