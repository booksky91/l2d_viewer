#include "render/ViewportCamera.hpp"
#include <algorithm>

namespace l2dgui
{

void ViewportCamera::reset()
{
    zoom = 1.0f;
    panX = 0.0f;
    panY = 0.0f;
}

void ViewportCamera::pan(float dxPixels, float dyPixels, int viewportW, int viewportH)
{
    if (viewportW <= 0 || viewportH <= 0) return;
    const float aspect = static_cast<float>(viewportW) / static_cast<float>(viewportH);
    panX += 2.0f * dxPixels * aspect / static_cast<float>(viewportW) / zoom;
    panY -= 2.0f * dyPixels / static_cast<float>(viewportH) / zoom;
}

void ViewportCamera::zoomAt(float factor, float mouseX, float mouseY, int viewportW, int viewportH)
{
    if (viewportW <= 0 || viewportH <= 0) return;
    const float oldZoom = zoom;
    zoom = std::clamp(zoom * factor, 0.05f, 20.0f);
    const float newZoom = zoom;

    const float aspect = static_cast<float>(viewportW) / static_cast<float>(viewportH);
    const float relX = mouseX / static_cast<float>(viewportW);
    const float relY = mouseY / static_cast<float>(viewportH);

    panX += aspect * (1.0f / oldZoom - 1.0f / newZoom) * (1.0f - 2.0f * relX);
    panY -= (1.0f / oldZoom - 1.0f / newZoom) * (1.0f - 2.0f * relY);
}

} // namespace l2dgui
