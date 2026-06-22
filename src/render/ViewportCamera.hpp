#pragma once

namespace l2dgui
{

struct ViewportCamera
{
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;

    void reset();
    void pan(float dxPixels, float dyPixels, int viewportW, int viewportH);
    void zoomAt(float factor, float mouseX, float mouseY, int viewportW, int viewportH);
};

} // namespace l2dgui
