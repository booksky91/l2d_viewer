#include "backends/MockBackend.hpp"
#include <GL/glew.h>
#include <cmath>

namespace l2dgui
{

bool MockBackend::initialize()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void MockBackend::shutdown()
{
    _runtimes.clear();
}

LoadResult MockBackend::loadModel(ModelEntry& entry)
{
    _runtimes.emplace(entry.id, Runtime{});
    if (entry.motions.empty())
    {
        entry.motions.push_back(MotionItem{"idle", 0, "mock_idle.motion3.json"});
        entry.motions.push_back(MotionItem{"tap", 0, "mock_tap.motion3.json"});
    }
    return {true, {}};
}

void MockBackend::unloadModel(uint64_t modelId)
{
    _runtimes.erase(modelId);
}

void MockBackend::playMotion(uint64_t modelId, const MotionItem& motion, bool loop)
{
    auto& rt = _runtimes[modelId];
    rt.motionIndex = motion.index;
    rt.time = 0.0f;
    rt.loop = loop;
}

void MockBackend::stopMotion(uint64_t modelId)
{
    _runtimes[modelId].motionIndex = -1;
}

void MockBackend::setExpression(uint64_t, const ExpressionItem&)
{
}

void MockBackend::update(AppState& state, float dtSeconds)
{
    for (const auto& m : state.models)
    {
        auto it = _runtimes.find(m.id);
        if (it != _runtimes.end()) it->second.time += dtSeconds * m.timeScale;
    }
}

void MockBackend::draw(const AppState& state, int width, int height, const ViewportCamera& camera)
{
    if (state.view.showWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    drawScene(state, width, height, camera, false, 0.0f);
    if (state.view.showWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void MockBackend::drawForExport(const AppState& state, int width, int height, const ViewportCamera& camera, float absoluteTimeSeconds)
{
    drawScene(state, width, height, camera, true, absoluteTimeSeconds);
}

void MockBackend::drawScene(const AppState& state, int width, int height, const ViewportCamera& camera, bool exportMode, float t)
{
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    // Apply viewport flip and rotation via the projection matrix
    const float sx = state.view.flipX ? -1.0f : 1.0f;
    const float sy = state.view.flipY ? -1.0f : 1.0f;
    glOrtho(sx * (-aspect / camera.zoom + camera.panX), sx * (aspect / camera.zoom + camera.panX),
            sy * (-1.0f / camera.zoom + camera.panY), sy * (1.0f / camera.zoom + camera.panY),
            -1.0f, 1.0f);
    if (state.view.rotationDeg != 0.0f)
    {
        glRotatef(state.view.rotationDeg, 0.0f, 0.0f, 1.0f);
    }
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    for (const auto& m : state.models)
    {
        if (!m.visible) continue;
        if (state.view.onlySelected && !m.selected) continue;
        if (exportMode && state.exportSettings.selectedOnly && !m.selected) continue;
        auto it = _runtimes.find(m.id);
        Runtime rt = (it == _runtimes.end()) ? Runtime{} : it->second;
        drawOne(m, rt, exportMode ? t : rt.time, state.view.showHitAreas);
    }
}

void MockBackend::drawOne(const ModelEntry& m, const Runtime& rt, float t, bool showHitAreas)
{
    glPushMatrix();
    glTranslatef(m.x, m.y, 0.0f);
    glRotatef(m.rotationDeg, 0.0f, 0.0f, 1.0f);
    glScalef(m.flipX ? -m.scale : m.scale, m.flipY ? -m.scale : m.scale, 1.0f);

    const float sway = 0.04f * std::sin(t * 3.1415926f * 2.0f);
    const float blink = 0.5f + 0.5f * std::sin(t * 7.0f + static_cast<float>(rt.motionIndex));
    const float a = m.alpha;

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.15f, 0.2f + 0.2f * blink, 0.95f, 0.92f * a);
    glVertex2f(0.0f + sway, 0.65f);
    glColor4f(0.6f, 0.9f, 1.0f, 0.80f * a);
    for (int i = 0; i <= 64; ++i)
    {
        const float ang = static_cast<float>(i) / 64.0f * 6.2831853f;
        const float rx = 0.27f + 0.03f * std::sin(t * 2.0f);
        const float ry = 0.42f;
        glVertex2f(sway + std::cos(ang) * rx, 0.05f + std::sin(ang) * ry);
    }
    glEnd();

    glBegin(GL_TRIANGLES);
    glColor4f(1.0f, 0.5f, 0.8f, 0.8f * a);
    glVertex2f(-0.18f + sway, 0.48f);
    glVertex2f(-0.04f + sway, 0.55f);
    glVertex2f(-0.12f + sway, 0.65f);
    glVertex2f(0.18f + sway, 0.48f);
    glVertex2f(0.04f + sway, 0.55f);
    glVertex2f(0.12f + sway, 0.65f);
    glEnd();

    if (m.selected)
    {
        glLineWidth(2.0f);
        glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(-0.34f, -0.42f);
        glVertex2f(0.34f, -0.42f);
        glVertex2f(0.34f, 0.72f);
        glVertex2f(-0.34f, 0.72f);
        glEnd();
    }

    if (showHitAreas)
    {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Fill
        glColor4f(1.0f, 0.0f, 0.0f, 0.2f);
        glBegin(GL_QUADS);
        glVertex2f(-0.34f, -0.42f);
        glVertex2f(0.34f, -0.42f);
        glVertex2f(0.34f, 0.72f);
        glVertex2f(-0.34f, 0.72f);
        glEnd();

        // Outline
        glColor4f(1.0f, 0.0f, 0.0f, 0.8f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(-0.34f, -0.42f);
        glVertex2f(0.34f, -0.42f);
        glVertex2f(0.34f, 0.72f);
        glVertex2f(-0.34f, 0.72f);
        glEnd();
        glPopAttrib();
    }

    glPopMatrix();
}

} // namespace l2dgui
