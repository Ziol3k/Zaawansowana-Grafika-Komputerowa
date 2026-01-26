#pragma once
#include <osgGA/GUIEventHandler>

struct Game;

class InputHandler : public osgGA::GUIEventHandler
{
public:
    explicit InputHandler(Game* g) : _g(g) {}
    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter&) override;

private:
    bool onKey(int key);

    Game* _g = nullptr;
    float _lastX = 0.0f;
    float _lastY = 0.0f;
};
