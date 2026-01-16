#include <osg/Group>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/MatrixTransform>
#include <osg/Geometry>
#include <osg/LineWidth>
#include <osg/Texture2D>
#include <osg/Light>
#include <osg/LightSource>
#include <osgDB/ReadFile>
#include <osgGA/GUIEventHandler>
#include <osgGA/TrackballManipulator>
#include <osgViewer/Viewer>
#include <osg/AnimationPath>

#include <algorithm>
#include <cmath>
#include <string>

// =====================
// Stałe gry
// =====================
static constexpr float BALL_R = 0.20f;

// Linia ma być "na piłce", nie wysoko nad nią:
static constexpr float AIM_Z_OFFSET = 0.02f;

// Kiedy uznajemy, że piłka stoi (można oddać kolejny strzał):
static constexpr float STOP_EPS = 0.05f;

// Czułość mocy: przeciąganie myszą w pionie (w dół = mocniej):
static constexpr float POWER_GAIN = 0.0045f;

// Max prędkość startowa:
static constexpr float SHOT_SPEED_MAX = 14.0f;

// Kamera trybu gry: niżej (mniejszy kąt)
static constexpr float GAME_CAM_DISTANCE = 7.0f;
static constexpr float GAME_CAM_HEIGHT   = 2.2f;

// ============================================================
// Pomocnicze: tekstury i proste obiekty
// ============================================================

static osg::ref_ptr<osg::Texture2D> loadTexture2D(const std::string& path)
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

static osg::ref_ptr<osg::Geode> makeTexturedBox(const osg::Vec3& center, float sx, float sy, float sz,
                                                osg::Texture2D* tex, const osg::Vec4& fallbackColor)
{
    osg::ref_ptr<osg::Box> box = new osg::Box(center, sx, sy, sz);
    osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(box.get());
    sd->setColor(fallbackColor);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(sd.get());

    osg::StateSet* ss = geode->getOrCreateStateSet();
    if (tex) ss->setTextureAttributeAndModes(0, tex, osg::StateAttribute::ON);

    return geode;
}

static osg::ref_ptr<osg::Geometry> makeAimLineGeometry()
{
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;

    osg::ref_ptr<osg::Vec3Array> v = new osg::Vec3Array;
    v->push_back(osg::Vec3(0, 0, 0));
    v->push_back(osg::Vec3(1, 0, 0));
    geom->setVertexArray(v.get());
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 2));

    osg::ref_ptr<osg::Vec4Array> c = new osg::Vec4Array;
    c->push_back(osg::Vec4(1, 1, 1, 1)); // biała linia
    geom->setColorArray(c.get(), osg::Array::BIND_OVERALL);

    osg::ref_ptr<osg::LineWidth> lw = new osg::LineWidth(3.0f);
    osg::StateSet* ss = geom->getOrCreateStateSet();
    ss->setAttributeAndModes(lw.get(), osg::StateAttribute::ON);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    // Dla dynamicznych wierzchołków: typowe ustawienia w OSG. [web:99][web:138]
    geom->setDataVariance(osg::Object::DYNAMIC);
    geom->setUseDisplayList(false);
    geom->setUseVertexBufferObjects(true);

    // Rysuj później niż podłoże, żeby nie ginęła w trawie. [web:93]
    ss->setRenderBinDetails(11, "RenderBin");

    return geom;
}

static osg::ref_ptr<osg::Geode> makeHoleGeode(const osg::Vec3& holePos, float holeRadius)
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode;

    osg::ref_ptr<osg::Cylinder> rim =
        new osg::Cylinder(holePos + osg::Vec3(0, 0, -0.01f), holeRadius * 1.05f, 0.02f);
    osg::ref_ptr<osg::ShapeDrawable> rimSD = new osg::ShapeDrawable(rim.get());
    rimSD->setColor(osg::Vec4(0.02f, 0.02f, 0.02f, 1.0f));

    osg::ref_ptr<osg::Cylinder> bottom =
        new osg::Cylinder(holePos + osg::Vec3(0, 0, -0.14f), holeRadius * 0.95f, 0.02f);
    osg::ref_ptr<osg::ShapeDrawable> bottomSD = new osg::ShapeDrawable(bottom.get());
    bottomSD->setColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    geode->addDrawable(rimSD.get());
    geode->addDrawable(bottomSD.get());

    // Renderuj po podłożu. [web:93]
    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setRenderBinDetails(12, "RenderBin");

    return geode;
}

// ============================================================
// Dane poziomu
// ============================================================

struct LevelData
{
    osg::Vec2 minXY = osg::Vec2(-10.0f, -2.0f);
    osg::Vec2 maxXY = osg::Vec2( 10.0f,  2.0f);

    osg::Vec3 holePos = osg::Vec3(9.0f, 0.0f, 0.0f);
    float holeRadius = 0.25f;

    bool hasRamp = false;
    float rampX0 = -1.0f;
    float rampX1 =  3.0f;
    float rampZ0 =  0.0f;
    float rampZ1 =  1.5f;

    bool hasWindmill = false;
    osg::Vec3 windmillCenter = osg::Vec3(0.0f, 0.0f, 0.0f);
    float windmillRadius = 1.2f;
    float windmillKick = 12.0f;
};

static float groundHeightAt(const LevelData& lvl, float x)
{
    if (!lvl.hasRamp) return 0.0f;
    if (x < lvl.rampX0) return 0.0f;
    if (x > lvl.rampX1) return lvl.rampZ1;
    float t = (x - lvl.rampX0) / (lvl.rampX1 - lvl.rampX0);
    return (1.0f - t) * lvl.rampZ0 + t * lvl.rampZ1;
}

static float rampSlopeDzDx(const LevelData& lvl, float x)
{
    if (!lvl.hasRamp) return 0.0f;
    if (x < lvl.rampX0 || x > lvl.rampX1) return 0.0f;
    return (lvl.rampZ1 - lvl.rampZ0) / (lvl.rampX1 - lvl.rampX0);
}

// ============================================================
// Fizyka piłki (promień + dołek)
// ============================================================

class BallUpdateCallback : public osg::NodeCallback
{
public:
    explicit BallUpdateCallback(LevelData* level) : _level(level) {}

    void setLevel(LevelData* level) { _level = level; }
    void setVelocity(const osg::Vec3& v) { _vel = v; }
    osg::Vec3 getVelocity() const { return _vel; }

    bool isStopped() const { return _vel.length() < STOP_EPS; }
    bool inHole() const { return _inHole; }

    void reset(osg::MatrixTransform* mt, const osg::Vec3& pos)
    {
        _vel.set(0,0,0);
        _prevSimTime = -1.0;
        _inHole = false;

        osg::Matrix m;
        m.makeTranslate(pos);
        mt->setMatrix(m);
    }

    void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        osg::MatrixTransform* mt = dynamic_cast<osg::MatrixTransform*>(node);
        if (!mt || !_level || !nv || !nv->getFrameStamp())
        {
            traverse(node, nv);
            return;
        }

        double t = nv->getFrameStamp()->getSimulationTime();
        if (_prevSimTime < 0.0) _prevSimTime = t;
        double dt = t - _prevSimTime;
        _prevSimTime = t;
        dt = std::clamp(dt, 0.0, 0.05);

        osg::Vec3 pos = mt->getMatrix().getTrans();

        if (_inHole)
        {
            osg::Matrix m;
            m.makeTranslate(pos);
            mt->setMatrix(m);
            traverse(node, nv);
            return;
        }

        const float g = 9.81f;

        _vel.z() -= g * static_cast<float>(dt);

        float ground = groundHeightAt(*_level, pos.x());
        bool nearGround = (pos.z() <= ground + BALL_R + 0.02f);

        if (_level->hasRamp && nearGround)
        {
            float slope = rampSlopeDzDx(*_level, pos.x());
            _vel.x() -= g * slope * static_cast<float>(dt);
        }

        pos += _vel * static_cast<float>(dt);

        ground = groundHeightAt(*_level, pos.x());
        nearGround = (pos.z() <= ground + BALL_R + 0.02f);

        if (pos.z() < ground + BALL_R)
        {
            pos.z() = ground + BALL_R;
            if (_vel.z() < -1.0f) _vel.z() = -_vel.z() * 0.35f;
            else _vel.z() = 0.0f;
        }

        // bandy z promieniem
        if (pos.x() < _level->minXY.x() + BALL_R) { pos.x() = _level->minXY.x() + BALL_R; _vel.x() = -_vel.x(); }
        if (pos.x() > _level->maxXY.x() - BALL_R) { pos.x() = _level->maxXY.x() - BALL_R; _vel.x() = -_vel.x(); }
        if (pos.y() < _level->minXY.y() + BALL_R) { pos.y() = _level->minXY.y() + BALL_R; _vel.y() = -_vel.y(); }
        if (pos.y() > _level->maxXY.y() - BALL_R) { pos.y() = _level->maxXY.y() - BALL_R; _vel.y() = -_vel.y(); }

        if (_level->hasWindmill)
        {
            osg::Vec3 d = pos - _level->windmillCenter;
            float dist = d.length();
            if (dist < _level->windmillRadius)
            {
                osg::Vec3 dir = (dist > 1e-4f) ? (d / dist) : osg::Vec3(1,0,0);
                _vel += dir * _level->windmillKick;
                _vel.z() += 4.0f;
            }
        }

        // tarcie XY
        float frictionPerSecond = 0.60f;
        float f = std::pow(frictionPerSecond, static_cast<float>(dt));
        _vel.x() *= f;
        _vel.y() *= f;

        if (nearGround)
        {
            if (std::abs(_vel.x()) < STOP_EPS) _vel.x() = 0.0f;
            if (std::abs(_vel.y()) < STOP_EPS) _vel.y() = 0.0f;
        }

        // dołek
        {
            osg::Vec3 hole = _level->holePos;
            osg::Vec2 dxy(pos.x() - hole.x(), pos.y() - hole.y());
            float dist = dxy.length();
            float speedXY = std::sqrt(_vel.x()*_vel.x() + _vel.y()*_vel.y());

            if (dist < _level->holeRadius * 0.90f && nearGround && speedXY < 0.6f)
            {
                _inHole = true;
                _vel.set(0,0,0);
                pos.x() = hole.x();
                pos.y() = hole.y();
                pos.z() = ground + BALL_R - 0.12f;
            }
        }

        osg::Matrix m;
        m.makeTranslate(pos);
        mt->setMatrix(m);

        traverse(node, nv);
    }

private:
    LevelData* _level = nullptr;
    osg::Vec3 _vel = osg::Vec3(0,0,0);
    double _prevSimTime = -1.0;
    bool _inHole = false;
};

// ============================================================
// Wiatrak: animacja obrotu
// ============================================================

static osg::ref_ptr<osg::AnimationPath> createRotorAnimationPath(float angularVelRadPerSec)
{
    osg::ref_ptr<osg::AnimationPath> ap = new osg::AnimationPath;
    ap->setLoopMode(osg::AnimationPath::LOOP);

    const int steps = 16;
    const float period = 2.0f * osg::PI / std::max(angularVelRadPerSec, 1e-3f);
    for (int i = 0; i <= steps; ++i)
    {
        float tt = (period * i) / steps;
        float a = (2.0f * osg::PI * i) / steps;
        osg::Quat q(a, osg::Vec3(0,0,1));
        ap->insert(tt, osg::AnimationPath::ControlPoint(osg::Vec3(0,0,0), q));
    }
    return ap;
}

// ============================================================
// Scena gry + kamera gry
// ============================================================

struct GameScene
{
    osg::ref_ptr<osg::Group> root = new osg::Group;
    osg::ref_ptr<osg::Group> levelRoot = new osg::Group;
    LevelData level;

    osg::ref_ptr<osg::MatrixTransform> ballMT = new osg::MatrixTransform;
    osg::ref_ptr<BallUpdateCallback> ballCB;

    osg::ref_ptr<osg::Geometry> aimLineGeom;
    osg::ref_ptr<osg::Texture2D> grassTex;
    osg::ref_ptr<osg::Texture2D> woodTex;

    bool freeCam = false;
    bool charging = false;

    osg::ref_ptr<osgGA::TrackballManipulator> tb = new osgGA::TrackballManipulator;

    float camYaw = 0.0f;
    float loftDeg = 10.0f;
    float power = 0.0f;

    void updateAimVisualization()
    {
        osg::Vec3 ballPos = ballMT->getMatrix().getTrans();

        // długość = funkcja power (na bieżąco)
        float L = 1.0f + 6.0f * std::clamp(power, 0.0f, 1.0f);
        osg::Vec3 dir(std::cos(camYaw), std::sin(camYaw), 0.0f);

        // start linii niemal na środku piłki (lekko nad) — bez "wiszenia"
        osg::Vec3 p0 = ballPos + osg::Vec3(0,0,AIM_Z_OFFSET);
        osg::Vec3 p1 = p0 + dir * L;

        osg::Vec3Array* v = dynamic_cast<osg::Vec3Array*>(aimLineGeom->getVertexArray());
        (*v)[0] = p0;
        (*v)[1] = p1;
        v->dirty();
        aimLineGeom->dirtyBound();
    }

    osg::Vec3 computeShotVelocity() const
    {
        float speed = SHOT_SPEED_MAX * std::clamp(power, 0.0f, 1.0f);

        float loft = osg::DegreesToRadians(std::clamp(loftDeg, -5.0f, 60.0f));
        float xy = speed * std::cos(loft);
        float z  = speed * std::sin(loft);

        osg::Vec3 dir(std::cos(camYaw), std::sin(camYaw), 0.0f);
        osg::Vec3 v = dir * xy;
        v.z() = z;
        return v;
    }

    GameScene()
    {
        grassTex = loadTexture2D("data/grass.jpg");
        woodTex  = loadTexture2D("data/wood.jpg");

        root->addChild(levelRoot.get());

        osg::ref_ptr<osg::Sphere> sphere = new osg::Sphere(osg::Vec3(0,0,0), BALL_R);
        osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(sphere.get());
        sd->setColor(osg::Vec4(1,1,1,1));
        osg::ref_ptr<osg::Geode> ballGeode = new osg::Geode;
        ballGeode->addDrawable(sd.get());
        ballMT->addChild(ballGeode.get());

        ballCB = new BallUpdateCallback(&level);
        ballMT->setUpdateCallback(ballCB.get());

        aimLineGeom = makeAimLineGeometry();
        osg::ref_ptr<osg::Geode> aimGeode = new osg::Geode;
        aimGeode->addDrawable(aimLineGeom.get());

        root->addChild(ballMT.get());
        root->addChild(aimGeode.get());
    }

    void rebuildLevel(int idx)
    {
        levelRoot->removeChildren(0, levelRoot->getNumChildren());
        level = LevelData();

        auto addGround = [&](const osg::Vec3& c, float sx, float sy, float sz)
        {
            levelRoot->addChild(makeTexturedBox(c, sx, sy, sz, grassTex.get(),
                                                osg::Vec4(0.2f,0.6f,0.2f,1.0f)).get());
        };

        auto addWall = [&](const osg::Vec3& c, float sx, float sy, float sz)
        {
            levelRoot->addChild(makeTexturedBox(c, sx, sy, sz, woodTex.get(),
                                                osg::Vec4(0.55f,0.35f,0.2f,1.0f)).get());
        };

        if (idx == 1)
        {
            level.minXY = osg::Vec2(-10.0f, -1.5f);
            level.maxXY = osg::Vec2( 10.0f,  1.5f);
            level.holePos = osg::Vec3(9.5f, 0.0f, 0.0f);
            level.holeRadius = 0.35f;

            addGround(osg::Vec3(0,0,-0.1f), 25.0f, 5.0f, 0.2f);
            addWall(osg::Vec3(0, level.minXY.y()-0.2f, 0.3f), 25.0f, 0.4f, 0.6f);
            addWall(osg::Vec3(0, level.maxXY.y()+0.2f, 0.3f), 25.0f, 0.4f, 0.6f);
            addWall(osg::Vec3(level.minXY.x()-0.2f, 0, 0.3f), 0.4f, 5.0f, 0.6f);
            addWall(osg::Vec3(level.maxXY.x()+0.2f, 0, 0.3f), 0.4f, 5.0f, 0.6f);

            ballCB->reset(ballMT.get(), osg::Vec3(-9.0f, 0.0f, BALL_R));
        }
        else if (idx == 2)
        {
            level.minXY = osg::Vec2(-12.0f, -2.0f);
            level.maxXY = osg::Vec2( 12.0f,  2.0f);
            level.holePos = osg::Vec3(10.5f, 0.0f, 1.5f);
            level.holeRadius = 0.35f;

            level.hasRamp = true;
            level.rampX0 = -1.0f;
            level.rampX1 =  4.0f;
            level.rampZ0 =  0.0f;
            level.rampZ1 =  1.5f;

            addGround(osg::Vec3(0,0,-0.1f), 30.0f, 6.0f, 0.2f);
            addWall(osg::Vec3(0, level.minXY.y()-0.2f, 0.3f), 30.0f, 0.4f, 0.6f);
            addWall(osg::Vec3(0, level.maxXY.y()+0.2f, 0.3f), 30.0f, 0.4f, 0.6f);
            addWall(osg::Vec3(level.minXY.x()-0.2f, 0, 0.3f), 0.4f, 6.0f, 0.6f);
            addWall(osg::Vec3(level.maxXY.x()+0.2f, 0, 0.3f), 0.4f, 6.0f, 0.6f);

            osg::ref_ptr<osg::MatrixTransform> rampMT = new osg::MatrixTransform;
            float dx = (level.rampX1 - level.rampX0);
            float dz = (level.rampZ1 - level.rampZ0);
            float angle = std::atan2(dz, dx);
            osg::Matrix R = osg::Matrix::rotate(-angle, osg::Vec3(0,1,0));
            osg::Vec3 rampCenter((level.rampX0+level.rampX1)/2.0f, 0.0f,
                                 (level.rampZ0+level.rampZ1)/2.0f - 0.05f);

            rampMT->setMatrix(osg::Matrix::translate(rampCenter) * R);
            rampMT->addChild(makeTexturedBox(osg::Vec3(0,0,0), dx, 4.0f, 0.2f,
                                            grassTex.get(), osg::Vec4(0.2f,0.6f,0.2f,1)).get());
            levelRoot->addChild(rampMT.get());

            ballCB->reset(ballMT.get(), osg::Vec3(-10.0f, 0.0f, BALL_R));
        }
        else if (idx == 3)
        {
            level.minXY = osg::Vec2(-12.0f, -2.5f);
            level.maxXY = osg::Vec2( 12.0f,  2.5f);
            level.holePos = osg::Vec3(10.5f, 0.0f, 0.0f);
            level.holeRadius = 0.35f;

            level.hasWindmill = true;
            level.windmillCenter = osg::Vec3(0.0f, 0.0f, 0.0f);
            level.windmillRadius = 1.2f;
            level.windmillKick = 14.0f;

            addGround(osg::Vec3(0,0,-0.1f), 30.0f, 7.0f, 0.2f);
            addWall(osg::Vec3(0, level.minXY.y()-0.2f, 0.3f), 30.0f, 0.4f, 0.6f);
            addWall(osg::Vec3(0, level.maxXY.y()+0.2f, 0.3f), 30.0f, 0.4f, 0.6f);
            addWall(osg::Vec3(level.minXY.x()-0.2f, 0, 0.3f), 0.4f, 7.0f, 0.6f);
            addWall(osg::Vec3(level.maxXY.x()+0.2f, 0, 0.3f), 0.4f, 7.0f, 0.6f);

            levelRoot->addChild(makeTexturedBox(osg::Vec3(0,0,0.8f), 0.3f, 0.3f, 1.6f,
                                                woodTex.get(), osg::Vec4(0.5f,0.3f,0.2f,1)).get());

            osg::ref_ptr<osg::MatrixTransform> rotor = new osg::MatrixTransform;
            rotor->setMatrix(osg::Matrix::translate(level.windmillCenter + osg::Vec3(0,0,1.2f)));

            osg::ref_ptr<osg::Group> blades = new osg::Group;
            blades->addChild(makeTexturedBox(osg::Vec3(0.8f,0,0), 1.6f, 0.2f, 0.2f,
                                             woodTex.get(), osg::Vec4(0.6f,0.4f,0.25f,1)).get());
            blades->addChild(makeTexturedBox(osg::Vec3(0,0.8f,0), 0.2f, 1.6f, 0.2f,
                                             woodTex.get(), osg::Vec4(0.6f,0.4f,0.25f,1)).get());
            rotor->addChild(blades.get());

            osg::ref_ptr<osg::AnimationPath> ap = createRotorAnimationPath(2.5f);
            rotor->setUpdateCallback(new osg::AnimationPathCallback(ap.get(), 0.0, 1.0));
            levelRoot->addChild(rotor.get());

            ballCB->reset(ballMT.get(), osg::Vec3(-10.0f, 0.0f, BALL_R));
        }

        levelRoot->addChild(makeHoleGeode(level.holePos, level.holeRadius).get());

        power = 0.0f;
        charging = false;
        camYaw = 0.0f;
        updateAimVisualization();
    }
};

class GameCameraCallback : public osg::NodeCallback
{
public:
    GameCameraCallback(GameScene* game) : _game(game) {}

    void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        osg::Camera* cam = dynamic_cast<osg::Camera*>(node);
        if (!cam || !_game)
        {
            traverse(node, nv);
            return;
        }

        if (_game->freeCam)
        {
            traverse(node, nv);
            return;
        }

        // w trakcie ładowania mocy blokujemy kamerę
        if (_game->charging)
        {
            traverse(node, nv);
            return;
        }

        osg::Vec3 ballPos = _game->ballMT->getMatrix().getTrans();
        osg::Vec3 back(-std::cos(_game->camYaw), -std::sin(_game->camYaw), 0.0f);

        osg::Vec3 eye = ballPos + back * GAME_CAM_DISTANCE + osg::Vec3(0,0,GAME_CAM_HEIGHT);
        osg::Vec3 center = ballPos + osg::Vec3(0,0,0.15f);
        osg::Vec3 up(0,0,1);

        cam->setViewMatrixAsLookAt(eye, center, up);

        traverse(node, nv);
    }

private:
    GameScene* _game = nullptr;
};

// ============================================================
// Oświetlenie
// ============================================================

static osg::ref_ptr<osg::LightSource> makeBasicLight()
{
    osg::ref_ptr<osg::Light> light = new osg::Light;
    light->setLightNum(0);
    light->setPosition(osg::Vec4(10.0f, -10.0f, 15.0f, 1.0f));
    light->setAmbient(osg::Vec4(0.2f,0.2f,0.2f,1.0f));
    light->setDiffuse(osg::Vec4(0.9f,0.9f,0.9f,1.0f));
    light->setSpecular(osg::Vec4(0.8f,0.8f,0.8f,1.0f));

    osg::ref_ptr<osg::LightSource> ls = new osg::LightSource;
    ls->setLight(light.get());
    ls->setLocalStateSetModes(osg::StateAttribute::ON);
    return ls;
}

// ============================================================
// Event handler: zawsze odświeża linię w FRAME
// ============================================================

class GolfEventHandler : public osgGA::GUIEventHandler
{
public:
    GolfEventHandler(GameScene* game, osgViewer::Viewer* viewer)
        : _game(game), _viewer(viewer) {}

    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& /*aa*/) override
    {
        if (!_game || !_viewer) return false;

        // Co klatkę odświeżaj linię (real-time), niezależnie od eventów myszy. [web:45]
        if (ea.getEventType() == osgGA::GUIEventAdapter::FRAME)
        {
            _game->updateAimVisualization();
            return false;
        }

        // FREECAM: manipulator przejmuje mysz, ale klawisze obsługujemy tutaj.
        if (_game->freeCam)
        {
            if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN)
                return handleKeys(ea);
            return false;
        }

        switch (ea.getEventType())
        {
        case osgGA::GUIEventAdapter::KEYDOWN:
            return handleKeys(ea);

        case osgGA::GUIEventAdapter::PUSH:
            if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
            {
                if (!_game->ballCB->isStopped() || _game->ballCB->inHole())
                    return true;

                _game->charging = true;
                _lastX = ea.getX();
                _lastY = ea.getY();
                return true;
            }
            break;

        case osgGA::GUIEventAdapter::RELEASE:
            if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
            {
                if (_game->charging)
                {
                    _game->charging = false;

                    osg::Vec3 v = _game->computeShotVelocity();
                    _game->ballCB->setVelocity(v);

                    _game->power = 0.0f;
                }
                return true;
            }
            break;

        case osgGA::GUIEventAdapter::MOVE:
        case osgGA::GUIEventAdapter::DRAG:
        {
            float x = ea.getX();
            float y = ea.getY();
            float dx = x - _lastX;
            float dy = y - _lastY;

            if (!_game->charging)
            {
                _game->camYaw += dx * 0.01f;
            }
            else
            {
                _game->power += (-dy) * POWER_GAIN;
                _game->power = std::clamp(_game->power, 0.0f, 1.0f);
            }

            _lastX = x;
            _lastY = y;
            return true;
        }

        default:
            break;
        }

        return false;
    }

private:
    bool handleKeys(const osgGA::GUIEventAdapter& ea)
    {
        if (ea.getKey() == 'c' || ea.getKey() == 'C')
        {
            _game->freeCam = !_game->freeCam;
            _game->charging = false;
            _game->power = 0.0f;

            if (_game->freeCam)
                _viewer->setCameraManipulator(_game->tb.get(), false);
            else
                _viewer->setCameraManipulator(nullptr, false);

            return true;
        }

        if (!_game->charging && _game->ballCB->isStopped())
        {
            if (ea.getKey() == osgGA::GUIEventAdapter::KEY_Up)
            {
                _game->loftDeg += 2.0f;
                return true;
            }
            if (ea.getKey() == osgGA::GUIEventAdapter::KEY_Down)
            {
                _game->loftDeg -= 2.0f;
                return true;
            }
        }

        if (ea.getKey() == 'h' || ea.getKey() == 'H')
        {
            osg::Vec3 ball = _game->ballMT->getMatrix().getTrans();
            osg::Vec3 hole = _game->level.holePos;
            osg::Vec2 d(hole.x() - ball.x(), hole.y() - ball.y());
            _game->camYaw = std::atan2(d.y(), d.x());
            return true;
        }

        if (ea.getKey() == '1') { _game->rebuildLevel(1); return true; }
        if (ea.getKey() == '2') { _game->rebuildLevel(2); return true; }
        if (ea.getKey() == '3') { _game->rebuildLevel(3); return true; }

        if (ea.getKey() == 'r' || ea.getKey() == 'R')
        {
            _game->ballCB->reset(_game->ballMT.get(), osg::Vec3(-9.0f, 0.0f, BALL_R));
            _game->power = 0.0f;
            _game->charging = false;
            return true;
        }

        return false;
    }

private:
    GameScene* _game = nullptr;
    osgViewer::Viewer* _viewer = nullptr;

    float _lastX = 0.0f;
    float _lastY = 0.0f;
};

// ============================================================
// main
// ============================================================

int main(int argc, char** argv)
{
    osgViewer::Viewer viewer;

    GameScene game;
    game.rebuildLevel(1);

    game.root->addChild(makeBasicLight().get());

    viewer.setCameraManipulator(nullptr, false);
    viewer.getCamera()->setUpdateCallback(new GameCameraCallback(&game));

    viewer.addEventHandler(new GolfEventHandler(&game, &viewer));

    viewer.setSceneData(game.root.get());
    viewer.setUpViewInWindow(50, 50, 1280, 720);

    return viewer.run();
}
