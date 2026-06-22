#include "export/FrameExporter.hpp"
#include "util/ProcessPipe.hpp"
#include <GL/glew.h>
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace l2dgui
{

static void report(const FrameExporterCallbacks& cb, ExportStatus* status, float progress, const std::string& msg)
{
    if (status)
    {
        status->progress = progress;
        status->message = msg;
    }
    if (cb.progress) cb.progress(progress, msg);
}

bool FrameExporter::ensureFbo(int width, int height, std::string* error)
{
    if (_fbo && _w == width && _h == height) return true;
    destroyFbo();

    _w = width;
    _h = height;
    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

    glGenTextures(1, &_tex);
    glBindTexture(GL_TEXTURE_2D, _tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _tex, 0);

    glGenRenderbuffers(1, &_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, _rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _rbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        if (error) *error = "OpenGL framebuffer is incomplete.";
        destroyFbo();
        return false;
    }
    return true;
}

void FrameExporter::destroyFbo()
{
    if (_rbo) glDeleteRenderbuffers(1, &_rbo);
    if (_tex) glDeleteTextures(1, &_tex);
    if (_fbo) glDeleteFramebuffers(1, &_fbo);
    _fbo = _tex = _rbo = 0;
    _w = _h = 0;
}

void FrameExporter::clear(bool transparent, const float bg[4])
{
    static const float fallback[4] = {0, 0, 0, 1};
    if (transparent)
    {
        glClearColor(0, 0, 0, 0);
    }
    else
    {
        const float* c = bg ? bg : fallback;
        glClearColor(c[0], c[1], c[2], c[3]);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

std::vector<uint8_t> FrameExporter::readRgba(int width, int height)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Flip bottom-up OpenGL pixels to top-down video frames.
    const int stride = width * 4;
    std::vector<uint8_t> flipped(pixels.size());
    for (int y = 0; y < height; ++y)
    {
        std::copy_n(pixels.data() + static_cast<size_t>(height - 1 - y) * stride, stride, flipped.data() + static_cast<size_t>(y) * stride);
    }
    return flipped;
}

AlphaBounds FrameExporter::computeFrameBounds(const uint8_t* rgba, int width, int height, int alphaThreshold) const
{
    AlphaBounds b;
    int minX = width, minY = height, maxX = -1, maxY = -1;
    for (int y = 0; y < height; ++y)
    {
        const uint8_t* row = rgba + static_cast<size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x)
        {
            const uint8_t a = row[x * 4 + 3];
            if (a > alphaThreshold)
            {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }
    if (maxX >= minX && maxY >= minY)
    {
        b.x = minX;
        b.y = minY;
        b.w = maxX - minX + 1;
        b.h = maxY - minY + 1;
        b.valid = true;
    }
    return b;
}

AlphaBounds FrameExporter::mergeBounds(const AlphaBounds& a, const AlphaBounds& b) const
{
    if (!a.valid) return b;
    if (!b.valid) return a;
    const int x1 = std::min(a.x, b.x);
    const int y1 = std::min(a.y, b.y);
    const int x2 = std::max(a.x + a.w, b.x + b.w);
    const int y2 = std::max(a.y + a.h, b.y + b.h);
    return AlphaBounds{x1, y1, x2 - x1, y2 - y1, true};
}

AlphaBounds FrameExporter::padAndEven(const AlphaBounds& b, int padding, int maxW, int maxH) const
{
    if (!b.valid) return AlphaBounds{0, 0, maxW, maxH, true};
    int x = std::max(0, b.x - padding);
    int y = std::max(0, b.y - padding);
    int r = std::min(maxW, b.x + b.w + padding);
    int bot = std::min(maxH, b.y + b.h + padding);

    int w = r - x;
    int h = bot - y;
    if (w % 2 != 0)
    {
        if (x + w + 1 <= maxW) ++w;
        else if (x > 0) { --x; ++w; }
    }
    if (h % 2 != 0)
    {
        if (y + h + 1 <= maxH) ++h;
        else if (y > 0) { --y; ++h; }
    }
    return AlphaBounds{x, y, std::max(2, w), std::max(2, h), true};
}

std::string FrameExporter::buildFfmpegCommand(const ExportSettings& s, const AlphaBounds& crop, bool useCrop) const
{
    std::ostringstream cmd;
    cmd << quoteArg(s.ffmpegPath.u8string())
        << " -y -f rawvideo -pix_fmt rgba -s " << s.width << "x" << s.height
        << " -r " << s.fps << " -i - ";

    if (useCrop)
    {
        cmd << "-vf " << quoteArg("crop=" + std::to_string(crop.w) + ":" + std::to_string(crop.h) + ":" + std::to_string(crop.x) + ":" + std::to_string(crop.y) + ",format=yuva420p") << " ";
    }
    else
    {
        cmd << "-vf " << quoteArg("format=yuva420p") << " ";
    }

    cmd << "-c:v libvpx-vp9 -row-mt 1 -auto-alt-ref 0 -b:v 0 -crf " << s.crf << " -an ";
    if (!s.extraFfmpegArgs.empty()) cmd << s.extraFfmpegArgs << " ";
    cmd << quoteArg(s.outputPath.u8string());
    return cmd.str();
}

static std::string buildFilterChain(const ExportSettings& settings)
{
    std::vector<std::string> filters;
    if (settings.renderScale > 1)
    {
        filters.push_back("scale=" + std::to_string(settings.width) + ":" + std::to_string(settings.height) + ":flags=lanczos");
    }
    if (settings.margin > 0)
    {
        int r = std::clamp(static_cast<int>(settings.bgColor[0] * 255.0f + 0.5f), 0, 255);
        int g = std::clamp(static_cast<int>(settings.bgColor[1] * 255.0f + 0.5f), 0, 255);
        int b = std::clamp(static_cast<int>(settings.bgColor[2] * 255.0f + 0.5f), 0, 255);
        int a = std::clamp(static_cast<int>(settings.bgColor[3] * 255.0f + 0.5f), 0, 255);
        char colorHex[32];
        std::snprintf(colorHex, sizeof(colorHex), "0x%02X%02X%02X%02X", r, g, b, a);
        
        std::string padFilter = "pad=" + std::to_string(settings.width + settings.margin * 2) + ":" 
                                       + std::to_string(settings.height + settings.margin * 2) + ":" 
                                       + std::to_string(settings.margin) + ":" 
                                       + std::to_string(settings.margin) + ":color=" + colorHex;
        filters.push_back(padFilter);
    }
    
    std::string filterChain;
    for (size_t i = 0; i < filters.size(); ++i)
    {
        if (i > 0) filterChain += ",";
        filterChain += filters[i];
    }
    return filterChain;
}

bool FrameExporter::resolveAutoResolution(ExportSettings& settings, const FrameExporterCallbacks& callbacks)
{
    if (!settings.autoResolution) return true;

    int maxRes = settings.maxResolution;
    if (maxRes <= 0) maxRes = 2048;

    std::string error;
    if (!ensureFbo(maxRes, maxRes, &error))
    {
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glViewport(0, 0, maxRes, maxRes);

    float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    clear(true, clearColor);

    callbacks.render(maxRes, maxRes, 0.0f);
    glFinish();

    const auto rgba = readRgba(maxRes, maxRes);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    AlphaBounds bounds = computeFrameBounds(rgba.data(), maxRes, maxRes, 1);
    if (bounds.valid && bounds.w > 0 && bounds.h > 0)
    {
        bounds = padAndEven(bounds, 10, maxRes, maxRes);
        settings.width = std::clamp(bounds.w, 16, maxRes);
        settings.height = std::clamp(bounds.h, 16, maxRes);
    }
    else
    {
        settings.width = 1024;
        settings.height = 1024;
    }
    return true;
}

bool FrameExporter::exportWebmAlpha(const ExportSettings& settings, const FrameExporterCallbacks& callbacks, ExportStatus* status)
{
    if (!callbacks.render) throw std::runtime_error("FrameExporter requires a render callback.");
    if (settings.width <= 0 || settings.height <= 0 || settings.fps <= 0 || settings.seconds <= 0.0f)
    {
        throw std::runtime_error("Invalid export settings.");
    }

    std::string error;
    if (!ensureFbo(settings.width, settings.height, &error))
    {
        throw std::runtime_error(error);
    }

    const int frameCount = std::max(1, static_cast<int>(settings.seconds * settings.fps + 0.5f));
    const int totalSteps = settings.autoCrop ? (frameCount * 2) : frameCount;
    AlphaBounds bounds;

    if (status) status->running = true;

    if (settings.autoCrop)
    {
        report(callbacks, status, 0.0f, "Pass 1/2: measuring alpha bounds");
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glViewport(0, 0, settings.width, settings.height);
        for (int i = 0; i < frameCount; ++i)
        {
            if (status && (status->cancelRequested || status->skipRequested))
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                if (status) status->running = false;
                return false;
            }
            const float t = static_cast<float>(i) / static_cast<float>(settings.fps);
            clear(settings.transparent, nullptr);
            callbacks.render(settings.width, settings.height, t);
            glFinish();
            const auto rgba = readRgba(settings.width, settings.height);
            bounds = mergeBounds(bounds, computeFrameBounds(rgba.data(), settings.width, settings.height, 1));
            if (status)
            {
                status->currentFrame = i + 1;
                status->totalFrames = totalSteps;
            }
            report(callbacks, status, 0.45f * (static_cast<float>(i + 1) / frameCount), "Pass 1/2: measuring alpha bounds");
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        bounds = padAndEven(bounds, settings.padding, settings.width, settings.height);
    }
    else
    {
        bounds = AlphaBounds{0, 0, settings.width, settings.height, true};
    }

    const std::string command = buildFfmpegCommand(settings, bounds, settings.autoCrop);
    if (status) status->lastCommand = command;

    WritePipe pipe;
    if (!pipe.open(command, &error))
    {
        if (status) status->running = false;
        throw std::runtime_error(error);
    }

    report(callbacks, status, 0.5f, "Pass 2/2: encoding VP9 WebM alpha");
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glViewport(0, 0, settings.width, settings.height);
    for (int i = 0; i < frameCount; ++i)
    {
        if (status && (status->cancelRequested || status->skipRequested))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            pipe.close();
            if (status) status->running = false;
            return false;
        }
        const float t = static_cast<float>(i) / static_cast<float>(settings.fps);
        clear(settings.transparent, nullptr);
        callbacks.render(settings.width, settings.height, t);
        glFinish();
        const auto rgba = readRgba(settings.width, settings.height);
        if (!pipe.write(rgba.data(), rgba.size()))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            pipe.close();
            if (status) status->running = false;
            throw std::runtime_error("Failed to write raw RGBA frame to ffmpeg.");
        }
        if (status)
        {
            status->currentFrame = (settings.autoCrop ? frameCount : 0) + i + 1;
            status->totalFrames = totalSteps;
        }
        report(callbacks, status, 0.5f + 0.5f * (static_cast<float>(i + 1) / frameCount), "Pass 2/2: encoding VP9 WebM alpha");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const int code = pipe.close();
    if (status)
    {
        status->running = false;
        status->progress = 1.0f;
        status->message = (code == 0) ? "Export complete" : "ffmpeg returned non-zero exit code: " + std::to_string(code);
    }
    return code == 0;
}

bool FrameExporter::exportSingleFrame(const ExportSettings& settings, const FrameExporterCallbacks& callbacks, ExportStatus* status)
{
    if (!callbacks.render) throw std::runtime_error("FrameExporter requires a render callback.");
    
    ExportSettings s = settings;
    if (s.autoResolution)
    {
        resolveAutoResolution(s, callbacks);
    }

    if (s.width <= 0 || s.height <= 0)
    {
        throw std::runtime_error("Invalid export settings.");
    }

    const int renderW = s.width * std::max(1, s.renderScale);
    const int renderH = s.height * std::max(1, s.renderScale);

    std::string error;
    if (!ensureFbo(renderW, renderH, &error))
    {
        throw std::runtime_error(error);
    }

    if (status) status->running = true;

    std::string ext = ".png";
    if (s.imageFormat == 0) ext = ".jpg";
    else if (s.imageFormat == 2) ext = ".webp";
    
    std::error_code ec;
    std::filesystem::path outPath = s.outputPath;
    if (outPath.empty())
    {
        std::filesystem::create_directories(s.outputFolder, ec);
        outPath = s.outputFolder / ("frame" + ext);
    }
    else
    {
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }

    const std::string filterChain = buildFilterChain(s);

    std::ostringstream cmd;
    cmd << quoteArg(s.ffmpegPath.u8string())
        << " -y -f rawvideo -pix_fmt rgba -s " << renderW << "x" << renderH
        << " -i - -vframes 1 ";
    if (!filterChain.empty()) {
        cmd << "-vf " << quoteArg(filterChain) << " ";
    }
    if (s.imageFormat == 0) {
        int q = 32 - (s.imageQuality * 30 / 100);
        cmd << "-qscale:v " << q << " ";
    }
    cmd << quoteArg(outPath.u8string());

    const std::string command = cmd.str();
    if (status) status->lastCommand = command;

    WritePipe pipe;
    if (!pipe.open(command, &error))
    {
        if (status) status->running = false;
        throw std::runtime_error(error);
    }

    if (status && (status->cancelRequested || status->skipRequested))
    {
        if (status) status->running = false;
        return false;
    }
    if (status)
    {
        status->currentFrame = 1;
        status->totalFrames = 1;
    }
    report(callbacks, status, 0.2f, "Rendering single frame");
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glViewport(0, 0, renderW, renderH);

    clear(true, s.bgColor);
    callbacks.render(renderW, renderH, 0.0f);
    glFinish();
    const auto rgba = readRgba(renderW, renderH);
    if (!pipe.write(rgba.data(), rgba.size()))
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        pipe.close();
        if (status) status->running = false;
        throw std::runtime_error("Failed to write raw RGBA frame to ffmpeg.");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const int code = pipe.close();
    if (status)
    {
        status->running = false;
        status->progress = 1.0f;
        status->message = (code == 0) ? "Export complete" : "ffmpeg returned non-zero exit code: " + std::to_string(code);
    }
    return code == 0;
}

bool FrameExporter::exportFrameSequence(const ExportSettings& settings, const FrameExporterCallbacks& callbacks, ExportStatus* status)
{
    if (!callbacks.render) throw std::runtime_error("FrameExporter requires a render callback.");
    
    ExportSettings s = settings;
    if (s.autoResolution)
    {
        resolveAutoResolution(s, callbacks);
    }

    // Resolve duration: -1 means "export exactly 1 full animation cycle"
    float dur = s.duration;
    if (dur <= 0.0f)
    {
        dur = (callbacks.getMotionDuration) ? callbacks.getMotionDuration() : 0.0f;
        if (dur <= 0.0f) dur = 1.0f; // fallback: 1 second if no motion loaded
    }
    if (s.width <= 0 || s.height <= 0 || s.fps <= 0 || dur <= 0.0f)
    {
        throw std::runtime_error("Invalid export settings.");
    }

    const int renderW = s.width * std::max(1, s.renderScale);
    const int renderH = s.height * std::max(1, s.renderScale);

    std::string error;
    if (!ensureFbo(renderW, renderH, &error))
    {
        throw std::runtime_error(error);
    }

    if (status) status->running = true;

    std::string ext = ".png";
    if (s.imageFormat == 0) ext = ".jpg";
    else if (s.imageFormat == 2) ext = ".webp";

    std::error_code ec;
    std::filesystem::path outPattern = s.outputPath;
    if (outPattern.empty())
    {
        std::filesystem::create_directories(s.outputFolder, ec);
        outPattern = s.outputFolder / ("frame_%04d" + ext);
    }
    else
    {
        std::filesystem::create_directories(outPattern.parent_path(), ec);
    }

    const std::string filterChain = buildFilterChain(s);

    std::ostringstream cmd;
    cmd << quoteArg(s.ffmpegPath.u8string())
        << " -y -f rawvideo -pix_fmt rgba -s " << renderW << "x" << renderH
        << " -r " << s.fps << " -i - ";
    if (!filterChain.empty()) {
        cmd << "-vf " << quoteArg(filterChain) << " ";
    }
    if (s.imageFormat == 0) {
        int q = 32 - (s.imageQuality * 30 / 100);
        cmd << "-qscale:v " << q << " ";
    }
    cmd << quoteArg(outPattern.u8string());

    const std::string command = cmd.str();
    if (status) status->lastCommand = command;

    WritePipe pipe;
    if (!pipe.open(command, &error))
    {
        if (status) status->running = false;
        throw std::runtime_error(error);
    }

    int frameCount = std::max(1, static_cast<int>(dur * s.fps + 0.5f));
    if (!s.keepLastFrame) frameCount = std::max(1, frameCount - 1);

    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glViewport(0, 0, renderW, renderH);

    for (int i = 0; i < frameCount; ++i)
    {
        if (status && (status->cancelRequested || status->skipRequested))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            pipe.close();
            if (status) status->running = false;
            return false;
        }
        if (status)
        {
            status->currentFrame = i + 1;
            status->totalFrames = frameCount;
        }
        const float t = static_cast<float>(i) / static_cast<float>(s.fps) * s.exportSpeed;
        clear(true, s.bgColor);
        callbacks.render(renderW, renderH, t);
        glFinish();
        const auto rgba = readRgba(renderW, renderH);
        if (!pipe.write(rgba.data(), rgba.size()))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            pipe.close();
            if (status) status->running = false;
            throw std::runtime_error("Failed to write raw RGBA frame to ffmpeg.");
        }
        report(callbacks, status, static_cast<float>(i + 1) / frameCount, "Encoding sequence frames");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const int code = pipe.close();
    if (status)
    {
        status->running = false;
        status->progress = 1.0f;
        status->message = (code == 0) ? "Export complete" : "ffmpeg returned non-zero exit code: " + std::to_string(code);
    }
    return code == 0;
}

bool FrameExporter::exportVideo(const ExportSettings& settings, const FrameExporterCallbacks& callbacks, ExportStatus* status)
{
    if (!callbacks.render) throw std::runtime_error("FrameExporter requires a render callback.");
    
    ExportSettings s = settings;
    if (s.autoResolution)
    {
        resolveAutoResolution(s, callbacks);
    }

    // Resolve duration: -1 means "export exactly 1 full animation cycle"
    float dur = s.duration;
    if (dur <= 0.0f)
    {
        dur = (callbacks.getMotionDuration) ? callbacks.getMotionDuration() : 0.0f;
        if (dur <= 0.0f) dur = 1.0f; // fallback: 1 second if no motion loaded
    }

    const int renderW = s.width * std::max(1, s.renderScale);
    const int renderH = s.height * std::max(1, s.renderScale);

    if (s.width <= 0 || s.height <= 0 || s.fps <= 0 || dur <= 0.0f)
    {
        throw std::runtime_error("Invalid export settings.");
    }

    std::string error;
    if (!ensureFbo(renderW, renderH, &error))
    {
        throw std::runtime_error(error);
    }

    if (status) status->running = true;

    std::string ext = ".mp4";
    if (s.videoFormat == 0) ext = ".gif";
    else if (s.videoFormat == 1) ext = ".webp";
    else if (s.videoFormat == 2) ext = ".png"; // apng
    else if (s.videoFormat == 4) ext = ".webm";
    else if (s.videoFormat == 5) ext = ".mkv";
    else if (s.videoFormat == 6) ext = ".mov";

    std::error_code ec;
    std::filesystem::path outPath = s.outputPath;
    if (outPath.empty())
    {
        std::filesystem::create_directories(s.outputFolder, ec);
        outPath = s.outputFolder / ("video" + ext);
    }
    else
    {
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }

    std::ostringstream cmd;
    cmd << quoteArg(s.ffmpegPath.u8string())
        << " -y -f rawvideo -pix_fmt rgba -s " << renderW << "x" << renderH
        << " -r " << s.fps << " -i - ";

    // Build filter chain
    const std::string filterChain = buildFilterChain(s);

    if (s.videoFormat == 0)
    {
        // GIF with dithering options
        std::string ditherStr = "none";
        if (s.gifDitherMode == 1) ditherStr = "bayer:bayer_scale=3";
        else if (s.gifDitherMode == 2) ditherStr = "floyd_steinberg";
        else if (s.gifDitherMode == 3) ditherStr = "sierra2_4a";

        if (filterChain.empty())
        {
            cmd << "-filter_complex \"split[a][b];[a]palettegen=max_colors="
                << s.gifMaxColors << "[p];[b][p]paletteuse=dither=" << ditherStr << "\" ";
        }
        else
        {
            cmd << "-filter_complex \"" << filterChain << ",split[a][b];[a]palettegen=max_colors="
                << s.gifMaxColors << "[p];[b][p]paletteuse=dither=" << ditherStr << "\" ";
        }
        if (s.loopPlay) cmd << "-loop 0 ";
        else cmd << "-loop -1 ";
    }
    else if (s.videoFormat == 1)
    {
        if (!filterChain.empty()) cmd << "-vf " << quoteArg(filterChain) << " ";
        cmd << "-c:v libwebp ";
        if (s.losslessCompression) cmd << "-lossless 1 ";
        else cmd << "-lossless 0 -q:v " << s.qualityParameter << " ";
        if (s.loopPlay) cmd << "-loop 0 ";
        else cmd << "-loop 1 ";
    }
    else if (s.videoFormat == 2)
    {
        if (!filterChain.empty()) cmd << "-vf " << quoteArg(filterChain) << " ";
        cmd << "-c:v apng ";
        if (s.loopPlay) cmd << "-plays 0 ";
        else cmd << "-plays 1 ";
    }
    else if (s.videoFormat == 4)
    {
        if (!filterChain.empty())
        {
            cmd << "-vf " << quoteArg(filterChain + ",format=yuva420p") << " ";
        }
        else
        {
            cmd << "-pix_fmt yuva420p ";
        }
        cmd << "-c:v libvpx-vp9 -row-mt 1 -b:v 0 -auto-alt-ref 0 -crf " << s.crfParameter << " ";
    }
    else
    {
        if (!filterChain.empty())
        {
            cmd << "-vf " << quoteArg(filterChain + ",format=yuv420p") << " ";
        }
        else
        {
            cmd << "-pix_fmt yuv420p ";
        }
        cmd << "-c:v libx264 -crf " << s.crfParameter << " ";
    }

    if (!s.extraFfmpegArgs.empty()) cmd << s.extraFfmpegArgs << " ";
    cmd << quoteArg(outPath.u8string());

    const std::string command = cmd.str();
    if (status) status->lastCommand = command;

    WritePipe pipe;
    if (!pipe.open(command, &error))
    {
        if (status) status->running = false;
        throw std::runtime_error(error);
    }

    int frameCount = std::max(1, static_cast<int>(dur * s.fps + 0.5f));
    if (!s.keepLastFrame) frameCount = std::max(1, frameCount - 1);

    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glViewport(0, 0, renderW, renderH);

    for (int i = 0; i < frameCount; ++i)
    {
        if (status && (status->cancelRequested || status->skipRequested))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            pipe.close();
            if (status) status->running = false;
            return false;
        }
        if (status)
        {
            status->currentFrame = i + 1;
            status->totalFrames = frameCount;
        }
        const float t = static_cast<float>(i) / static_cast<float>(s.fps) * s.exportSpeed;
        clear(true, s.bgColor);
        callbacks.render(renderW, renderH, t);
        glFinish();
        const auto rgba = readRgba(renderW, renderH);
        if (!pipe.write(rgba.data(), rgba.size()))
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            pipe.close();
            if (status) status->running = false;
            throw std::runtime_error("Failed to write raw RGBA frame to ffmpeg.");
        }
        report(callbacks, status, static_cast<float>(i + 1) / frameCount, "Encoding video");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const int code = pipe.close();
    if (status)
    {
        status->running = false;
        status->progress = 1.0f;
        status->message = (code == 0) ? "Export complete" : "ffmpeg returned non-zero exit code: " + std::to_string(code);
    }
    return code == 0;
}

} // namespace l2dgui
