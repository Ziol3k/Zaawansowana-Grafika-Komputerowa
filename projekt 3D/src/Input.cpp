#include "Input.h"
#include "Game.h"
#include "Physics.h"
#include <osgViewer/Viewer>

bool InputHandler::handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter&)
{
    if (!_g) return false;

    if (_g->freeCam) {
        if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN) return onKey(ea.getKey());
        return false;
    }

    switch (ea.getEventType())
    {
        case osgGA::GUIEventAdapter::KEYDOWN:
            return onKey(ea.getKey());

        case osgGA::GUIEventAdapter::PUSH:
            if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) {
                if (!isStopped(_g->ball) || _g->ball.inHole) return true;
                _g->charging = true;
                _lastX = ea.getX(); _lastY = ea.getY();
                return true;
            }
            break;

        case osgGA::GUIEventAdapter::RELEASE:
            if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) {
                if (_g->charging) {
                    _g->charging = false;
                    _g->ball.vel = _g->computeShotVelocity();
                    _g->power = 0.0f;
                    _g->hud.incStroke();
                }
                return true;
            }
            break;

        case osgGA::GUIEventAdapter::MOVE:
        case osgGA::GUIEventAdapter::DRAG:
        {
            float x = ea.getX(), y = ea.getY();
            float dx = x - _lastX, dy = y - _lastY;

            if (!_g->charging) _g->camYaw += dx * 0.01f;
            else {
                _g->power += (-dy) * cfg::POWER_GAIN;
                _g->power = cfg::clamp01(_g->power);
            }

            _lastX = x; _lastY = y;
            return true;
        }

        default:
            break;
    }

    return false;
}

bool InputHandler::onKey(int key)
{
    if (key == 'c' || key == 'C') {
        _g->freeCam = !_g->freeCam;
        _g->charging = false;
        _g->power = 0.0f;

        if (_g->viewer) {
            if (_g->freeCam) _g->viewer->setCameraManipulator(_g->tb.get(), false);
            else _g->viewer->setCameraManipulator(nullptr, false);
        }
        _g->setCursorGameMode(!_g->freeCam);
        return true;
    }

    if (!_g->charging && isStopped(_g->ball)) {
        if (key == osgGA::GUIEventAdapter::KEY_Up)   { _g->loftDeg += 2.0f; return true; }
        if (key == osgGA::GUIEventAdapter::KEY_Down) { _g->loftDeg -= 2.0f; return true; }
    }

    if (key == '1') { _g->loadLevel(1); return true; }
    if (key == '2') { _g->loadLevel(2); return true; }

    return false;
}
