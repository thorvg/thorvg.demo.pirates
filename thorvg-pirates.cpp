/*
 * Copyright (c) 2026 ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>
#include <vector>
#include <string.h>
#include <thorvg_toolkit.h>
#include "font.h"

using namespace std;
using namespace tvg;
using namespace tvg::toolkit;

struct ThorPirate : tvg::toolkit::App
{
    static constexpr float worldLeft = -0.125f;
    static constexpr float worldRight = 1.125f;
    static constexpr float playerLeft = -0.05f;
    static constexpr float playerRight = 1.05f;
    static constexpr float distantSeaLevel = 0.73f - (0.73f - 2.0f / 3.0f) * 0.30f;
    static constexpr float shipWaterline = 152.0f;
    Scene* screen = nullptr;
    float screenShakeTime = 0.0f;
    Scene* world = nullptr;
    float cameraZoom = 1.0f;
    float cameraX = 0.5f, cameraY = 0.5f;

    void updateCamera(float dt)
    {
        float targetZoom = 0.8f;
        float targetX = 0.5f, targetY = 0.5f;
        const auto& player = vessels[0];
        if (player.health > 0) {
            float nearestDistance = 2.0f;
            float enemyPosition = player.position;
            for (const auto& vessel : vessels) {
                if (!vessel.enemy || vessel.health == 0 || vessel.position < worldLeft || vessel.position > worldRight) continue;
                const float distance = std::abs(vessel.position - player.position);
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    enemyPosition = vessel.position;
                }
            }
            const float proximity = std::clamp((0.75f - nearestDistance) / 0.5625f, 0.0f, 1.0f);
            const float blend = proximity * proximity * (3.0f - 2.0f * proximity);
            targetZoom += 0.50f * blend;
            targetX += ((player.position + enemyPosition) * 0.5f - 0.5f) * blend;
            targetY += 0.10f * blend;
        }
        const float smoothing = 1.0f - std::exp(-2.5f * std::min(dt, 0.1f));
        cameraZoom += (targetZoom - cameraZoom) * smoothing;
        cameraX += (targetX - cameraX) * smoothing;
        cameraY += (targetY - cameraY) * smoothing;
        // Clamp the viewport within the world so zooming never exposes empty edges.
        const float halfView = 0.5f / cameraZoom;
        cameraX = std::clamp(cameraX, worldLeft + halfView, worldRight - halfView);
        cameraY = std::clamp(cameraY, worldLeft + halfView, worldRight - halfView);
        world->transform(Matrix{cameraZoom, 0.0f, size().w * (0.5f - cameraX * cameraZoom),
            0.0f, cameraZoom, size().h * (0.5f - cameraY * cameraZoom), 0.0f, 0.0f, 1.0f});
    }

    void updateScreenShake(float dt)
    {
        if (screenShakeTime <= 0.0f) return;
        screenShakeTime = std::max(0.0f, screenShakeTime - dt);
        const float age = 0.45f - screenShakeTime;
        const float fade = screenShakeTime / 0.45f;
        const float envelope = fade * fade;
        const float amplitude = size().h * 0.007f * envelope;
        const float x = amplitude * std::sin(age * 83.0f);
        const float y = amplitude * 0.7f * std::sin(age * 107.0f + 0.8f);
        // A small overscan keeps screen edges covered even at the widest camera zoom.
        const float zoom = 1.0f + 0.025f * envelope;
        screen->transform(Matrix{zoom, 0, x + size().w * (1.0f - zoom) * 0.5f,
            0, zoom, y + size().h * (1.0f - zoom) * 0.5f, 0, 0, 1});
    }

    Scene* gameOver = nullptr;
    bool restartRequested = false;
    bool spaceHeld = false;
    bool fontLoaded = false;
    size_t gameStart = 0;
    Text* scoreText = nullptr;
    uint64_t score = 0;

    struct ScorePopup
    {
        Text* text = nullptr;
        float x = 0.0f, y = 0.0f, birth = 0.0f;
        bool active = false;
    };
    ScorePopup scorePopups[24];
    unsigned nextScorePopup = 0;

    void showScorePopup(unsigned points, float x, float y, float time)
    {
        auto& popup = scorePopups[nextScorePopup];
        nextScorePopup = (nextScorePopup + 1) % 24;
        popup.x = x;
        popup.y = y - size().h * 0.02f;
        popup.birth = time;
        popup.active = true;
        popup.text->text(points == 100 ? "+100" : "+30");
        popup.text->size(points == 100 ? 25.0f : 20.0f);
        popup.text->fill(255, points == 100 ? 208 : 239, points == 100 ? 110 : 205);
        popup.text->translate(popup.x, popup.y);
        popup.text->opacity(255);
    }

    void updateScorePopups(float time)
    {
        for (auto& popup : scorePopups) {
            if (!popup.active) continue;
            const float age = std::max(0.0f, time - popup.birth);
            if (age >= 1.2f) {
                popup.active = false;
                popup.text->opacity(0);
                continue;
            }
            popup.text->translate(popup.x, popup.y - size().h * 0.045f * age);
            popup.text->opacity(static_cast<uint8_t>(255.0f * std::min(1.0f, (1.2f - age) / 0.7f)));
        }
    }

    void addScore(unsigned points)
    {
        score += points;
        const string label = "Score: " + std::to_string(score);
        scoreText->text(label.c_str());
    }
    Shape* sea = nullptr;
    struct SeaLayer
    {
        float level, amplitude, frequency, speed, phase;
        Shape* shape = nullptr;
    };
    SeaLayer seaLayers[3] = {
        {0.73f, 0.010f, 4.5f, 0.85f, 0.6f},
        {0.80f, 0.018f, 3.2f, 0.70f, 1.3f},
        {0.87f, 0.022f, 2.3f, 0.55f, 2.0f}
    };
    static constexpr float sunRadiusRatio = 0.09f;
    Shape* sun = nullptr;
    Shape* sunGlow = nullptr;
    Shape* sunClip = nullptr;
    Shape* sunReflections[4] = {};
    void updateSunReflections(float time)
    {
        const float width = static_cast<float>(size().w);
        const float height = static_cast<float>(size().h);
        for (unsigned layer = 1; layer < 4; ++layer) {
            auto shape = sunReflections[layer];
            shape->reset();
            const float level = layer == 0 ? distantSeaLevel : seaLayers[layer - 1].level;
            const float end = layer < 3 ? seaLayers[layer].level : worldRight;
            // Share wave samples across all glints instead of evaluating waves per endpoint.
            constexpr unsigned sampleCount = 96;
            constexpr float sampleStep = (worldRight - worldLeft) / sampleCount;
            WaterSample samples[sampleCount + 1];
            for (unsigned i = 0; i <= sampleCount; ++i) {
                const float u = worldLeft + sampleStep * i;
                samples[i] = layer == 0 ? surfaceSample(u, time) : layerSample(seaLayers[layer - 1], u, time);
            }
            auto surface = [&](float u) {
                const float position = std::clamp((u - worldLeft) / sampleStep, 0.0f, float(sampleCount));
                const unsigned i = std::min(static_cast<unsigned>(position), sampleCount - 1);
                const float t = position - i;
                const auto& a = samples[i];
                const auto& b = samples[i + 1];
                const float c = sampleStep * a.slope;
                const float d = 3.0f * (b.height - a.height) - sampleStep * (2.0f * a.slope + b.slope);
                const float e = 2.0f * (a.height - b.height) + sampleStep * (a.slope + b.slope);
                return WaterSample{a.height + t * (c + t * (d + t * e)),
                    (c + t * (2.0f * d + 3.0f * t * e)) / sampleStep};
            };
            const PathCommand* commands = nullptr;
            const Point* points = nullptr;
            uint32_t commandCount = 0, pointCount = 0;
            (layer == 0 ? sea : seaLayers[layer - 1].shape)->path(&commands, &commandCount, &points, &pointCount);
            // Stable noise keeps the glints irregular without flickering between frames.
            auto noise = [](uint32_t seed) {
                seed ^= seed >> 16;
                seed *= 0x7feb352du;
                seed ^= seed >> 15;
                seed *= 0x846ca68bu;
                seed ^= seed >> 16;
                return (seed & 0xffffu) / 65535.0f;
            };
            const unsigned rows = layer == 0 ? 20 : (layer == 3 ? 70 : 32);
            for (unsigned row = 0; row < rows; ++row) {
                const uint32_t seed = 1 + layer * 10000 + row * 37;
                const float offset = (end - level) * (row + noise(seed)) / rows;
                const float distance = std::clamp((level + offset - distantSeaLevel) / 0.4f, 0.0f, 1.0f);
                const float phase = noise(seed + 1) * 6.2831853f;
                const float spread = (height * sunRadiusRatio / width) * (0.95f + distance * 2.05f);
                const float center = 0.5f + 0.004f * std::sin(time * 0.9f + phase);
                const unsigned fragments = row % 2 == 0 ? 7 : 5;
                for (unsigned fragment = 0; fragment < fragments; ++fragment) {
                    const bool scattered = fragment >= 3;
                    const uint32_t key = seed + fragment * 101;
                    const float lateral = noise(key + 2) * 2.0f - 1.0f;
                    const float edge = lateral * lateral;
                    const float edgeSpread = 1.0f + edge * (noise(key + 7) * 1.4f - 0.45f);
                    const float half = (0.0015f + noise(key + 3) * 0.010f) * (0.7f + distance)
                        * (0.8f + 0.2f * std::sin(time * 1.6f + phase + fragment))
                        * (scattered ? 0.7f : 1.0f);
                    const float x = scattered
                        ? worldLeft + 0.025f + (worldRight - worldLeft - 0.05f)
                            * ((static_cast<float>((row + fragment) % 4) + noise(key + 2)) / 4.0f)
                            + 0.002f * std::sin(time * 0.8f + phase + fragment)
                        : center + lateral * std::max(0.0f, spread - half) * edgeSpread;
                    const float irregularOffset = std::max(0.0f, offset
                        + (end - level) / rows * (noise(key + 8) - 0.5f)
                            * (scattered ? 1.0f : 3.0f * edge));
                    const float left = (x - half) * width;
                    const float right = (x + half) * width;
                    const auto leftWater = surface(x - half);
                    const auto rightWater = surface(x + half);
                    constexpr float waveFollow = 0.65f;
                    const float leftY = height * (level + irregularOffset + waveFollow * (leftWater.height - level));
                    const float rightY = height * (level + irregularOffset + waveFollow * (rightWater.height - level));
                    const float tangentScale = height * waveFollow * (2.0f * half / 3.0f);
                    const float controlLeftY = leftY + leftWater.slope * tangentScale;
                    const float controlRightY = rightY - rightWater.slope * tangentScale;
                    const float third = (right - left) / 3.0f;
                    const float thickness = height * (0.00025f + 0.00065f * noise(key + 4))
                        * (0.7f + distance * 0.6f) * (scattered ? 0.4f : 1.0f);
                    shape->moveTo(left, leftY);
                    shape->cubicTo(left + third, controlLeftY - thickness,
                        right - third, controlRightY - thickness, right, rightY);
                    shape->cubicTo(right - third, controlRightY + thickness,
                        left + third, controlLeftY + thickness, left, leftY);
                    shape->close();
                }
            }
        }
    }

    Scene* createSunReflection(const Size& size, unsigned layer)
    {
        auto shape = Shape::gen();
        sunReflections[layer] = shape;
        shape->fill(250, 218, 184);
        auto reflection = Scene::gen();
        reflection->add(shape);
        reflection->blend(BlendMethod::SoftLight);
        reflection->add(SceneEffect::GaussianBlur, 1.5, 0, 0, 20);
        return reflection;
    }
    struct Impact
    {
        float position = 0.0f, birth = 0.0f, strength = 0.0f;
        float sampleTime = -1.0f, amplitude = 0.0f;
    };
    Impact impacts[16];
    unsigned nextImpact = 0;
    struct Droplet
    {
        Shape* shape = nullptr;
        float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f;
        float birth = 0.0f, lifetime = 0.0f;
        bool active = false;
    };
    Droplet droplets[1152];
    unsigned nextDroplet = 0;
    struct Debris
    {
        Shape* shape = nullptr;
        float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f;
        float birth = 0.0f, lifetime = 0.0f, spin = 0.0f;
        bool active = false;
    };
    Debris debris[256];
    unsigned nextDebris = 0;

    void shatter(float x, float y, float time)
    {
        for (unsigned i = 0; i < 32; ++i) {
            auto& piece = debris[nextDebris];
            nextDebris = (nextDebris + 1) % 256;
            piece.x = x;
            piece.y = y;
            piece.vx = randomRange(-0.10f, 0.10f) * size().w;
            piece.vy = randomRange(-0.22f, 0.035f) * size().h;
            piece.birth = time;
            piece.lifetime = randomRange(0.7f, 1.4f);
            piece.spin = randomRange(-12.0f, 12.0f);
            piece.active = true;
            const float length = randomRange(3.0f, 8.0f) * size().h / 1280.0f;
            piece.shape->reset();
            piece.shape->moveTo(-length, -length * 0.3f);
            piece.shape->lineTo(length, -length * 0.15f);
            piece.shape->lineTo(length * 0.4f, length * 0.5f);
            piece.shape->close();
            if (i % 3 == 0) piece.shape->fill(65, 35, 40);
            else if (i % 3 == 1) piece.shape->fill(75, 50, 35);
            else piece.shape->fill(35, 35, 45);
            piece.shape->transform(Matrix{1, 0, x, 0, 1, y, 0, 0, 1});
            piece.shape->opacity(255);
        }
    }

    void updateDebris(float time)
    {
        for (auto& piece : debris) {
            if (!piece.active) continue;
            const float age = time - piece.birth;
            const float x = piece.x + piece.vx * age;
            const float y = piece.y + piece.vy * age + size().h * 0.25f * age * age;
            if (age >= piece.lifetime || y > size().h * worldRight || x < size().w * worldLeft - 20.0f || x > size().w * worldRight + 20.0f) {
                piece.active = false;
                piece.shape->opacity(0);
                continue;
            }
            const float c = std::cos(age * piece.spin);
            const float s = std::sin(age * piece.spin);
            piece.shape->transform(Matrix{c, -s, x, s, c, y, 0, 0, 1});
            piece.shape->opacity(static_cast<uint8_t>(255.0f * (1.0f - age / piece.lifetime)));
        }
    }

    bool hitEnemy(float fromX, float fromY, float toX, float toY, float& hitTime, int* hitIndex = nullptr, bool targetEnemies = true)
    {
        hitTime = 2.0f;
        if (hitIndex) *hitIndex = -1;
        for (auto& vessel : vessels) {
            if (vessel.enemy != targetEnemies || vessel.health == 0) continue;
            const auto& m = vessel.ship->transform();
            const float determinant = m.e11 * m.e22 - m.e12 * m.e21;
            if (std::abs(determinant) < 0.000001f) continue;
            auto local = [&](float x, float y) {
                x -= m.e13;
                y -= m.e23;
                return Point{(m.e22 * x - m.e12 * y) / determinant,
                    (-m.e21 * x + m.e11 * y) / determinant};
            };
            const auto start = local(fromX, fromY);
            const auto end = local(toX, toY);
            const float radius = size().h * 0.0105f * 0.8f / std::hypot(m.e11, m.e21);
            // Sweep through separate hull, sail and mast bounds to avoid tunneling.
            auto box = [&](Point a, Point b, float left, float top, float right, float bottom) {
                float entry = 0.0f, exit = 1.0f;
                auto slab = [&](float origin, float delta, float low, float high) {
                    if (std::abs(delta) < 0.000001f) return origin >= low && origin <= high;
                    float near = (low - origin) / delta;
                    float far = (high - origin) / delta;
                    if (near > far) std::swap(near, far);
                    entry = std::max(entry, near);
                    exit = std::min(exit, far);
                    return entry <= exit;
                };
                if (slab(a.x, b.x - a.x, left - radius, right + radius) &&
                    slab(a.y, b.y - a.y, top - radius, bottom + radius)) {
                    if (entry < hitTime) {
                        hitTime = entry;
                        if (hitIndex) *hitIndex = static_cast<int>(&vessel - vessels.data());
                    }
                }
            };
            box(start, end, 20.0f, 127.0f, 162.0f, 154.0f);
            const auto& rig = vessel.rig->transform();
            const Point rigStart{start.x - rig.e12 * start.y - rig.e13, start.y};
            const Point rigEnd{end.x - rig.e12 * end.y - rig.e13, end.y};
            box(rigStart, rigEnd, 46.0f, 51.0f, 130.0f, 99.0f);
            box(rigStart, rigEnd, 84.5f, 3.0f, 89.5f, 132.0f);
        }
        return hitTime <= 1.0f;
    }

    void splash(float x, float y, float time, float strength)
    {
        const float power = std::clamp(strength / 0.022f, 0.7f, 1.4f);
        constexpr float riseScale = 2.0f;  // Height is proportional to launch speed squared.
        for (unsigned i = 0; i < 120; ++i) {
            auto& drop = droplets[nextDroplet];
            nextDroplet = (nextDroplet + 1) % 1152;
            drop.x = x + randomRange(-0.001f, 0.001f) * size().w;
            drop.y = y - size().h * 0.004f;
            const float spread = (i + randomRange(0.0f, 1.0f)) / 120.0f;
            // Sample a radial fan around the upward direction, rather than independent X/Y speeds.
            const float angle = (spread * 2.0f - 1.0f) * 1.134464f;
            const float speed = size().h * 0.23f * power * riseScale * std::sqrt(randomRange(0.16f, 1.0f));
            drop.vx = std::sin(angle) * speed * 0.5f;
            drop.vy = -std::cos(angle) * speed;
            drop.birth = time;
            drop.lifetime = -2.0f * drop.vy / (size().h * 0.65f) + 0.2f;
            drop.active = true;
            const float radius = randomRange(0.75f, 1.75f) * size().h / 1280.0f;
            drop.shape->reset();
            drop.shape->appendCircle(0.0f, 0.0f, radius, radius * 3.0f);
            drop.shape->translate(drop.x, drop.y);
            drop.shape->opacity(255);
        }
    }

    void updateDroplets(float time)
    {
        const float gravity = size().h * 0.65f;
        for (auto& drop : droplets) {
            if (!drop.active) continue;
            const float age = time - drop.birth;
            const float x = drop.x + drop.vx * age;
            const float y = drop.y + drop.vy * age + 0.5f * gravity * age * age;
            if (age >= drop.lifetime || x < size().w * worldLeft - 10.0f || x > size().w * worldRight + 10.0f ||
                (drop.vy + gravity * age > 0.0f && y >= size().h * sailingSurface(x / size().w, time).height)) {
                drop.active = false;
                drop.shape->opacity(0);
            } else {
                drop.shape->translate(x, y);
                const float fade = std::clamp((age / drop.lifetime - 0.7f) / 0.3f, 0.0f, 1.0f);
                drop.shape->opacity(static_cast<uint8_t>(255.0f * (1.0f - fade)));
            }
        }
    }

    struct WaterSample { float height = 0.0f, slope = 0.0f; };

    WaterSample impactSample(float u, float time)
    {
        WaterSample result;
        for (auto& impact : impacts) {
            const float age = time - impact.birth;
            if (impact.strength == 0.0f || age <= 0.0f || age >= 4.0f) continue;
            if (impact.sampleTime != time) {
                const float fade = 1.0f - age / 4.0f;
                impact.amplitude = impact.strength * std::min(age / 0.08f, 1.0f) *
                    fade * fade * std::exp(-age * 0.6f);
                impact.sampleTime = time;
            }
            for (float direction : {-1.0f, 1.0f}) {
                const float z = (u - impact.position - direction * age * 0.13f) / 0.035f;
                if (std::abs(z) > 4.0f) continue;
                const float envelope = impact.amplitude * std::exp(-z * z);
                result.height += envelope * std::cos(3.0f * z);
                result.slope += envelope * (-2.0f * z * std::cos(3.0f * z)
                    - 3.0f * std::sin(3.0f * z)) / 0.035f;
            }
        }
        return result;
    }

    WaterSample surfaceSample(float u, float time)
    {
        constexpr float tau = 6.283185307f;
        WaterSample result;
        result.height += distantSeaLevel + 0.006f * (std::sin(tau * 9.0f * u - time * 1.0f)
            + 0.35f * std::sin(tau * 15.0f * u + time * 0.7f));
        result.slope += 0.006f * tau * (9.0f * std::cos(tau * 9.0f * u - time * 1.0f)
            + 5.25f * std::cos(tau * 15.0f * u + time * 0.7f));
        return result;
    }

    WaterSample layerSample(const SeaLayer& layer, float u, float time)
    {
        const float frequency = 6.283185307f * layer.frequency;
        const float a = frequency * u - time * layer.speed + layer.phase;
        const float b = frequency * 1.8f * u + time * layer.speed * 0.7f;
        const auto ripple = &layer == &seaLayers[0] ? impactSample(u, time) : WaterSample{};
        return {layer.level + layer.amplitude * (std::sin(a) + 0.25f * std::sin(b)) + ripple.height,
            layer.amplitude * frequency * (std::cos(a) + 0.45f * std::cos(b)) + ripple.slope};
    }

    WaterSample sailingSurface(float u, float time)
    {
        return layerSample(seaLayers[0], u, time);
    }
    struct Cannonball
    {
        Shape* shape = nullptr;
        Shape* speedLines = nullptr;
        LinearGradient* speedGradient = nullptr;
        unsigned speedLineCount = 3;
        float speedLineOffsets[6] = {};
        float speedLineLengths[6] = {};
        float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f;
        bool active = false;
        bool submerged = false;
        bool enemyShot = false;
        float sinkTime = 0.0f;
    };
    Cannonball cannonballs[32];
    unsigned nextCannonball = 0;
    Shape* cannon = nullptr;
    Shape* aimGuide = nullptr;
    float cannonAngle = 35.0f;
    bool aimingUp = false;
    bool aimingDown = false;

    void aimCannon()
    {
        const float angle = cannonAngle * 0.01745329252f;
        cannon->reset();
        cannon->moveTo(38.0f, 127.0f);
        cannon->lineTo(38.0f - 27.0f * std::cos(angle), 127.0f - 27.0f * std::sin(angle));
    }
    bool charging = false;
    Scene* chargeGauge = nullptr;
    Shape* chargeFill = nullptr;
    std::chrono::steady_clock::time_point chargeStart;
    std::chrono::steady_clock::time_point reloadReady{};

    void randomizeSpeedLines(Cannonball& ball)
    {
        ball.speedLineCount = std::uniform_int_distribution<unsigned>(3, 6)(randomEngine);
        for (unsigned i = 0; i < ball.speedLineCount; ++i) {
            ball.speedLineOffsets[i] = randomRange(-0.95f, 0.95f);
            ball.speedLineLengths[i] = randomRange(0.4f, 1.0f);
        }
    }

    void updateSpeedLines(Cannonball& ball)
    {
        const float speed = std::hypot(ball.vx, ball.vy);
        if (!ball.active || ball.submerged || speed < 40.0f) {
            ball.speedLines->opacity(0);
            return;
        }
        const float radius = size().h * 0.0105f * 0.8f;
        const float length = std::clamp(speed * 0.09f, radius * 2.0f, radius * 9.0f) * 1.5f;
        const float c = ball.vx / speed;
        const float s = ball.vy / speed;
        const float lookback = length / speed;
        const float curve = c * size().h * 0.25f * lookback * lookback;
        ball.speedLines->reset();
        for (unsigned i = 0; i < ball.speedLineCount; ++i) {
            const float offset = ball.speedLineOffsets[i] * radius;
            const float front = -std::sqrt(radius * radius - offset * offset) - radius * 0.15f;
            const float lineLength = length * ball.speedLineLengths[i];
            const float tail = front - lineLength;
            const float lineCurve = curve * ball.speedLineLengths[i] * ball.speedLineLengths[i];
            ball.speedLines->moveTo(front, offset);
            ball.speedLines->cubicTo(front - lineLength * 0.3f, offset,
                tail + lineLength * 0.3f, offset + lineCurve * 0.4f, tail, offset + lineCurve);
        }
        ball.speedGradient->linear(-radius * 1.3f - length, 0.0f, 0.0f, 0.0f);
        ball.speedLines->transform(Matrix{c, -s, ball.x, s, c, ball.y, 0, 0, 1});
        ball.speedLines->opacity(220);
    }

    void positionCannonball(Cannonball& ball)
    {
        const float dx = size().w * 0.5f - ball.x;
        const float dy = size().h * distantSeaLevel - ball.y;
        const float distance = std::hypot(dx, dy);
        const float c = distance > 0.001f ? dx / distance : 1.0f;
        const float s = distance > 0.001f ? dy / distance : 0.0f;
        ball.shape->transform(Matrix{c, -s, ball.x, s, c, ball.y, 0.0f, 0.0f, 1.0f});
        updateSpeedLines(ball);
    }

    void updateChargeGauge()
    {
        if (vessels[0].health == 0) {
            charging = false;
            chargeGauge->opacity(0);
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        const bool reloading = now < reloadReady;
        chargeGauge->opacity((charging || reloading) ? 255 : 0);
        if (!charging && !reloading) return;
        const float seconds = std::chrono::duration<float>(now - chargeStart).count();
        const float progress = reloading
            ? std::clamp(1.0f - std::chrono::duration<float>(reloadReady - now).count() / 3.0f, 0.0f, 1.0f)
            : std::clamp(seconds * 1.5f / 4.0f, 0.0f, 1.0f);
        chargeFill->reset();
        if (progress > 0.0f) chargeFill->appendRect(3.0f, 3.0f, 114.0f * progress, 8.0f, 2.0f, 2.0f);
        if (reloading) chargeFill->fill(125, 180, 225);
        else chargeFill->fill(255, static_cast<uint8_t>(210.0f - 110.0f * progress), 75);
        const auto& transform = vessels[0].ship->transform();
        const float scale = size().w * 0.08f / 120.0f;
        const float x = transform.e11 * 87.0f + transform.e12 * -30.0f + transform.e13;
        const float y = transform.e21 * 87.0f + transform.e22 * -30.0f + transform.e23;
        chargeGauge->transform(Matrix{scale, 0.0f, x - 60.0f * scale,
            0.0f, scale, y - 14.0f * scale, 0.0f, 0.0f, 1.0f});
    }

    void playerLaunch(float charge, Cannonball& ball)
    {
        const auto& transform = vessels[0].ship->transform();
        const float angle = cannonAngle * 0.01745329252f;
        const float dx = -std::cos(angle);
        const float dy = -std::sin(angle);
        const float muzzleX = 38.0f + 27.0f * dx;
        const float muzzleY = 127.0f + 27.0f * dy;
        ball.x = transform.e11 * muzzleX + transform.e12 * muzzleY + transform.e13;
        ball.y = transform.e21 * muzzleX + transform.e22 * muzzleY + transform.e23;
        const float worldX = transform.e11 * dx + transform.e12 * dy;
        const float worldY = transform.e21 * dx + transform.e22 * dy;
        // Range at a fixed angle is proportional to speed squared, hence charge time.
        const float speed = std::sqrt(0.135f * size().w * size().h * std::max(charge, 0.0f));
        const float scale = speed / std::hypot(worldX, worldY);
        ball.vx = worldX * scale;
        ball.vy = worldY * scale;
    }

    void updateAimGuide()
    {
        const auto now = std::chrono::steady_clock::now();
        aimGuide->reset();
        if (vessels[0].health == 0 || now < reloadReady) {
            aimGuide->opacity(0);
            return;
        }
        const float charge = charging ? std::chrono::duration<float>(now - chargeStart).count() : 1.0f;
        Cannonball preview;
        playerLaunch(charge, preview);
        const float gravity = size().h * 0.5f;
        const float spacing = size().w * 0.006f;
        float distance = 0.0f;
        float nextDot = spacing;
        float previousX = preview.x, previousY = preview.y;
        unsigned dots = 0;
        for (unsigned step = 1; step <= 100 && dots < 9; ++step) {
            const float t = step * 0.004f;
            const float x = preview.x + preview.vx * t;
            const float y = preview.y + preview.vy * t + 0.5f * gravity * t * t;
            distance += std::hypot(x - previousX, y - previousY);
            if (distance >= nextDot) {
                const float radius = size().h * 0.0015f;
                aimGuide->appendCircle(x, y, radius, radius);
                nextDot += spacing;
                ++dots;
            }
            previousX = x;
            previousY = y;
        }
        aimGuide->opacity(190);
    }

    void fire(float charge)
    {
        if (vessels[0].health == 0) return;
        const auto now = std::chrono::steady_clock::now();
        if (now < reloadReady) return;
        reloadReady = now + std::chrono::seconds(3);
        auto& ball = cannonballs[nextCannonball];
        nextCannonball = (nextCannonball + 1) % 32;
        playerLaunch(charge, ball);
        ball.active = true;
        randomizeSpeedLines(ball);
        ball.enemyShot = false;
        ball.submerged = false;
        ball.sinkTime = 0.0f;
        positionCannonball(ball);
        ball.shape->opacity(255);
    }

    void updateCannonballs(float dt, float time)
    {
        const float gravity = size().h * 0.5f;
        for (auto& ball : cannonballs) {
            if (!ball.active) continue;
            const float previousX = ball.x;
            const float previousY = ball.y;
            if (ball.submerged) {
                const float drag = std::exp(-3.0f * dt);
                const float terminalSpeed = size().h * 0.075f;
                ball.x += ball.vx * (1.0f - drag) / 3.0f;
                ball.y += terminalSpeed * dt + (ball.vy - terminalSpeed) * (1.0f - drag) / 3.0f;
                ball.vx *= drag;
                ball.vy = terminalSpeed + (ball.vy - terminalSpeed) * drag;
                ball.sinkTime += dt;
                ball.shape->opacity(static_cast<uint8_t>(255.0f * std::max(0.0f, 1.0f - ball.sinkTime / 3.0f)));
            } else {
                ball.x += ball.vx * dt;
                ball.y += ball.vy * dt + 0.5f * gravity * dt * dt;
                ball.vy += gravity * dt;
            }
            float hitTime;
            int hitIndex;
            if (!ball.submerged && hitEnemy(previousX, previousY, ball.x, ball.y, hitTime, &hitIndex, !ball.enemyShot)) {
                auto& target = vessels[hitIndex];
                if (target.enemy) {
                    addScore(30);
                    showScorePopup(30, previousX + (ball.x - previousX) * hitTime,
                        previousY + (ball.y - previousY) * hitTime, time);
                }
                else screenShakeTime = 0.45f;
                target.hitStart = time;
                if (target.flameCount < 7) {
                    target.flamePositions[target.flameCount++] = Point{randomRange(35.0f, 145.0f), randomRange(118.0f, 130.0f)};
                }
                if (--target.health == 0) {
                    if (target.enemy && randomRange(0.0f, 1.0f) < 0.5f) spawnCrate(target.position);
                    target.sinkStart = time;
                    const auto& m = target.ship->transform();
                    target.sinkY = m.e21 * 90.0f + m.e22 * shipWaterline + m.e23;
                    target.sinkAngle = std::atan2(m.e21, m.e11);
                }
                updateFlames(target, time);
                shatter(previousX + (ball.x - previousX) * hitTime,
                    previousY + (ball.y - previousY) * hitTime, time);
                ball.active = false;
                ball.shape->opacity(0);
                ball.speedLines->opacity(0);
                continue;
            }
            const float u = ball.x / size().w;
            const float surface = size().h * sailingSurface(u, time).height;
            if (!ball.submerged && u >= worldLeft && u <= worldRight && ball.vy > 0.0f && ball.y >= surface) {
                const float strength = 0.022f * std::clamp(ball.vy / (size().h * 0.25f), 0.6f, 1.5f);
                impacts[nextImpact] = {u, time, strength};
                nextImpact = (nextImpact + 1) % 16;
                splash(ball.x, surface, time, strength);
                ball.submerged = true;
                ball.vx *= 0.3f;
                ball.vy *= 0.3f;
            }
            if (ball.x < size().w * worldLeft - 16.0f || ball.x > size().w * worldRight + 16.0f ||
                ball.y > size().h * worldRight + 16.0f || ball.sinkTime >= 3.0f) {
                ball.active = false;
                ball.shape->opacity(0);
                ball.speedLines->opacity(0);
            } else {
                positionCannonball(ball);
            }
        }
    }
    struct Vessel
    {
        float position;
        bool enemy;
        float phase;
        float patrolCenter = 0.0f;
        float patrolRadius = 0.0f;
        float patrolSpeed = 0.0f;
        float hitStart = -10.0f;
        int health = 3;
        float patrolStart = 0.0f;
        float sinkStart = 0.0f, sinkY = 0.0f, sinkAngle = 0.0f;
        Scene* ship = nullptr;
        Scene* rig = nullptr;
        Shape* sail = nullptr;
        Shape* sailFolds = nullptr;
        Shape* sailHighlights = nullptr;
        Shape* flag = nullptr;
        Picture* emblem = nullptr;
        Shape* cannon = nullptr;
        float nextAttack = 0.0f, attackCharge = 0.0f, aimOffset = 0.0f;
        bool attackCharging = false;
        Scene* fire = nullptr;
        Shape* flames[7] = {};
        Shape* flameCores[7] = {};
        Point flamePositions[7] = {};
        unsigned flameCount = 0;
        Paint* reflection = nullptr;
        float reflectionTime = -1.0f;
    };

    void updateFlames(Vessel& vessel, float time)
    {
        vessel.fire->opacity(vessel.flameCount > 0 ? 255 : 0);
        for (unsigned i = 0; i < vessel.flameCount; ++i) {
            const float phase = time * 11.0f + i * 1.7f + vessel.phase;
            const float x = vessel.flamePositions[i].x;
            const float height = 40.0f + 11.0f * std::sin(phase) + 6.0f * std::sin(phase * 1.73f);
            const float sway = 7.0f * std::sin(phase * 0.8f);
            auto flame = [&](Shape* shape, float width, float h, float drift) {
                shape->reset();
                shape->moveTo(x - width, 131.0f);
                shape->cubicTo(x - width * 1.4f, 131.0f - h * 0.35f,
                    x + drift - width * 0.4f, 131.0f - h * 0.65f, x + drift, 131.0f - h);
                shape->cubicTo(x + drift + width * 0.2f, 131.0f - h * 0.65f,
                    x + width * 1.5f, 131.0f - h * 0.3f, x + width, 131.0f);
                shape->cubicTo(x + width * 0.5f, 136.0f, x - width * 0.5f, 136.0f, x - width, 131.0f);
                shape->close();
                shape->translate(0.0f, vessel.flamePositions[i].y - 131.0f);
            };
            flame(vessel.flames[i], 12.0f, height, sway);
            flame(vessel.flameCores[i], 6.0f, height * 0.65f, sway * 0.5f);
        }
    }
    std::vector<Vessel> vessels = {
        {0.75f, false, 0.0f},
        {0.12f, true, 0.8f, 0.12f, 0.03f, 0.65f},
        {0.25f, true, 1.6f, 0.25f, 0.025f, -0.50f},
        {0.38f, true, 2.4f, 0.38f, 0.03f, 0.40f}
    };
    Scene* fleet = nullptr;
    Scene* reflectionLayer = nullptr;
    Shape* reflectionClip = nullptr;

    void updateReflections(float time)
    {
        // Reuse reflected geometry between snapshots; movement stays frame-smooth.
        const PathCommand* commands = nullptr;
        const Point* points = nullptr;
        uint32_t commandCount = 0, pointCount = 0;
        seaLayers[0].shape->path(&commands, &commandCount, &points, &pointCount);
        reflectionClip->reset();
        reflectionClip->appendPath(commands, commandCount, points, pointCount);
        for (auto& vessel : vessels) {
            if (!vessel.ship) continue;
            if (!vessel.reflection || time - vessel.reflectionTime >= 0.1f) {
                if (vessel.reflection) reflectionLayer->remove(vessel.reflection);
                vessel.reflection = vessel.ship->duplicate();
                reflectionLayer->add(vessel.reflection);
                vessel.reflectionTime = time;
            }
            const auto& m = vessel.ship->transform();
            const float waterY = size().h * sailingSurface(vessel.position, time).height;
            const float shear = 0.035f * std::sin(time * 2.7f + vessel.phase);
            const float offset = size().w * 0.0015f * std::sin(time * 3.4f + vessel.phase);
            constexpr float compression = 0.55f;
            vessel.reflection->transform(Matrix{
                m.e11 + shear * m.e21, m.e12 + shear * m.e22,
                m.e13 + shear * (m.e23 - waterY) + offset,
                -compression * m.e21, -compression * m.e22,
                waterY + compression * (waterY - m.e23), 0, 0, 1});
            const float fade = vessel.health > 0 ? 1.0f
                : std::max(0.0f, 1.0f - (time - vessel.sinkStart) / 1.5f);
            vessel.reflection->opacity(static_cast<uint8_t>(128.0f * fade));
        }
    }
    size_t nextEnemySpawn = 20000;
    struct Crate
    {
        Scene* scene = nullptr;
        float position = 0.0f;
        bool active = false;
    };
    Scene* crateLayer = nullptr;
    std::vector<Crate> crates;

    void spawnCrate(float position)
    {
        auto slot = std::find_if(crates.begin(), crates.end(), [](const Crate& crate) { return !crate.active; });
        if (slot == crates.end()) {
            crates.push_back({Scene::gen(), position, true});
            slot = crates.end() - 1;
            auto box = Shape::gen();
            box->appendRect(-12.0f, -12.0f, 24.0f, 24.0f, 1.5f, 1.5f);
            auto wood = LinearGradient::gen();
            wood->linear(-12.0f, -12.0f, 12.0f, 12.0f);
            const Fill::ColorStop colors[] = {{0, 205, 146, 79, 255}, {1, 100, 59, 35, 255}};
            wood->colorStops(colors, 2);
            box->fill(wood);
            box->strokeWidth(2.0f);
            box->strokeFill(65, 41, 30);
            slot->scene->add(box);
            auto braces = Shape::gen();
            braces->fill(0, 0, 0, 0);
            braces->appendRect(-9.0f, -9.0f, 18.0f, 18.0f);
            braces->moveTo(-9.0f, -9.0f);
            braces->lineTo(9.0f, 9.0f);
            braces->moveTo(9.0f, -9.0f);
            braces->lineTo(-9.0f, 9.0f);
            braces->strokeWidth(2.0f);
            braces->strokeFill(235, 182, 111);
            slot->scene->add(braces);
            crateLayer->add(slot->scene);
        }
        slot->position = position;
        slot->active = true;
        slot->scene->opacity(255);
    }

    void updateCrates(float dt, float time)
    {
        auto& player = vessels[0];
        for (auto& crate : crates) {
            if (!crate.active) continue;
            if (player.health > 0) {
                const float distance = player.position - crate.position;
                const float travel = std::min(std::abs(distance), 0.045f * dt);
                crate.position += std::copysign(travel, distance);
                if (std::abs(player.position - crate.position) < 0.05f) {
                    player.health = std::min(player.health + 1, 7);
                    if (player.flameCount > 0) {
                        --player.flameCount;
                        player.flames[player.flameCount]->reset();
                        player.flameCores[player.flameCount]->reset();
                    }
                    updateFlames(player, time);
                    crate.active = false;
                    crate.scene->opacity(0);
                    continue;
                }
            }
            const auto water = sailingSurface(crate.position, time);
            const float angle = std::atan(size().h * water.slope / size().w) + 0.06f * std::sin(time * 3.0f);
            const float scale = 2.0f * size().h / 1280.0f;
            const float c = std::cos(angle) * scale, s = std::sin(angle) * scale;
            crate.scene->transform(Matrix{c, -s, crate.position * size().w,
                s, c, water.height * size().h + 4.0f * scale, 0, 0, 1});
        }
    }

    bool createVessel(Vessel& vessel)
    {
        vessel.health = vessel.enemy ? 3 : 7;
        struct SailColor { uint8_t r, g, b; };
        static constexpr SailColor enemyColors[] = {
            {209, 65, 60}, {43, 43, 50}, {39, 57, 106},
            {123, 62, 160}, {227, 116, 39}
        };
        const auto color = enemyColors[vessel.enemy
            ? std::uniform_int_distribution<unsigned>(0, 4)(randomEngine) : 0];
        static constexpr char pirateShip[] = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" width="180" height="170" viewBox="0 0 180 170">
  <path d="M28 132 L20 117 L49 117 L54 130 M130 130 L136 111 L158 111 L155 132" fill="#49303e"/>
  <path d="M12 130 Q87 140 168 126 L149 151 Q91 165 39 151 Z" fill="#53333c"/>
  <path d="M16 134 Q88 144 163 131" fill="none" stroke="#d79a66" stroke-width="4"/>
  <path d="M43 147 L140 147" stroke="#9a5b49" stroke-width="2"/>
  <g fill="#efba79"><circle cx="62" cy="146" r="3"/><circle cx="85" cy="148" r="3"/><circle cx="108" cy="147" r="3"/><circle cx="131" cy="144" r="3"/></g>
</svg>)svg";
        auto& ship = vessel.ship;
        auto& rig = vessel.rig;
        auto& sail = vessel.sail;
        auto& flag = vessel.flag;
        auto& emblem = vessel.emblem;
        ship = Scene::gen();
        rig = Scene::gen();
        auto mast = Shape::gen();
        mast->moveTo(87.0f, 132.0f);
        mast->lineTo(87.0f, 3.0f);
        mast->moveTo(43.0f, 48.0f);
        mast->lineTo(132.0f, 48.0f);
        mast->moveTo(44.0f, 101.0f);
        mast->lineTo(131.0f, 101.0f);
        mast->strokeWidth(5.0f);
        if (vessel.enemy) mast->strokeFill(color.r * 0.8f, color.g * 0.8f, color.b * 0.8f);
        else mast->strokeFill(75, 48, 55);
        if (rig->add(mast) != Result::Success) return false;
        auto mastLight = Shape::gen();
        mastLight->moveTo(85.8f, 131.0f);
        mastLight->lineTo(85.8f, 3.0f);
        mastLight->moveTo(43.0f, 46.8f);
        mastLight->lineTo(132.0f, 46.8f);
        mastLight->moveTo(44.0f, 99.8f);
        mastLight->lineTo(131.0f, 99.8f);
        mastLight->strokeWidth(1.2f);
        mastLight->strokeFill(255, 180, 120, 125);
        if (rig->add(mastLight) != Result::Success) return false;
        sail = Shape::gen();
        auto fabric = LinearGradient::gen();
        fabric->linear(48.0f, 52.0f, 130.0f, 96.0f);
        const Fill::ColorStop creamStops[] = {
            {0.0f, 162, 116, 85, 255},
            {0.25f, 245, 212, 165, 255},
            {0.46f, 255, 237, 195, 255},
            {0.72f, 220, 175, 131, 255},
            {1.0f, 145, 102, 85, 255}
        };
        Fill::ColorStop enemyStops[5];
        constexpr float offsets[] = {0.0f, 0.25f, 0.46f, 0.72f, 1.0f};
        constexpr float shades[] = {0.50f, 1.0f, 1.0f, 0.82f, 0.43f};
        for (unsigned i = 0; i < 5; ++i) {
            auto channel = [&](uint8_t base, uint8_t light) {
                return static_cast<uint8_t>(i == 2 ? base + (light - base) * 0.28f : base * shades[i]);
            };
            enemyStops[i] = {offsets[i], channel(color.r, 255), channel(color.g, 224), channel(color.b, 195), 255};
        }
        fabric->colorStops(vessel.enemy ? enemyStops : creamStops, 5);
        sail->fill(fabric);
        if (rig->add(sail) != Result::Success) return false;
        vessel.sailFolds = Shape::gen();
        vessel.sailFolds->fill(0, 0, 0, 0);
        vessel.sailFolds->strokeWidth(1.5f);
        if (vessel.enemy) vessel.sailFolds->strokeFill(color.r * 0.3f, color.g * 0.3f, color.b * 0.3f, 90);
        else vessel.sailFolds->strokeFill(65, 30, 40, 75);
        if (rig->add(vessel.sailFolds) != Result::Success) return false;
        vessel.sailHighlights = Shape::gen();
        vessel.sailHighlights->fill(0, 0, 0, 0);
        vessel.sailHighlights->strokeWidth(0.9f);
        vessel.sailHighlights->strokeFill(255, 232, 180, 100);
        if (rig->add(vessel.sailHighlights) != Result::Success) return false;
        flag = Shape::gen();
        flag->fill(48, 39, 51);
        if (rig->add(flag) != Result::Success) return false;
        static constexpr char skull[] = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="180" height="170">
<path d="M96 7 L108 12 M96 12 L108 7" stroke="#fff0d2" stroke-width="2"/>
<circle cx="102" cy="7" r="3.5" fill="#fff0d2"/>
<circle cx="101" cy="6.5" r="0.8" fill="#302733"/><circle cx="104" cy="6.5" r="0.8" fill="#302733"/>
</svg>)svg";
        emblem = Picture::gen();
        if (emblem->load(skull, sizeof(skull) - 1, "svg") != Result::Success) return false;
        if (rig->add(emblem) != Result::Success) return false;
        if (ship->add(rig) != Result::Success) return false;
        auto hull = Picture::gen();
        if (hull->load(pirateShip, sizeof(pirateShip) - 1, "svg") != Result::Success) return false;
        if (ship->add(hull) != Result::Success) return false;
        if (vessel.enemy) {
            vessel.cannon = Shape::gen();
            vessel.cannon->moveTo(138.0f, 127.0f);
            vessel.cannon->lineTo(161.0f, 111.0f);
            vessel.cannon->strokeWidth(9.0f);
            vessel.cannon->strokeFill(45, 43, 55);
            if (ship->add(vessel.cannon) != Result::Success) return false;
            vessel.nextAttack = randomRange(1.0f, 3.0f);
        }
        vessel.fire = Scene::gen();
        if (vessel.fire->blend(BlendMethod::Add) != Result::Success) return false;
        for (unsigned i = 0; i < 7; ++i) {
            auto outer = Shape::gen();
            auto glow = LinearGradient::gen();
            glow->linear(0.0f, 135.0f, 0.0f, 72.0f);
            const Fill::ColorStop colors[] = {
                {0.0f, 255, 190, 45, 245},
                {0.45f, 255, 100, 15, 235},
                {1.0f, 220, 40, 15, 100}
            };
            glow->colorStops(colors, 3);
            outer->fill(glow);
            vessel.flames[i] = outer;
            if (vessel.fire->add(outer) != Result::Success) return false;
            auto core = Shape::gen();
            core->fill(255, 235, 145, 235);
            vessel.flameCores[i] = core;
            if (vessel.fire->add(core) != Result::Success) return false;
        }
        vessel.fire->opacity(0);
        if (ship->add(vessel.fire) != Result::Success) return false;
        if (fleet->add(ship) != Result::Success) return false;
        return true;
    }

    bool spawnEnemy(float time)
    {
        auto slot = std::find_if(vessels.begin() + 1, vessels.end(), [](const Vessel& vessel) {
            return vessel.health == 0 && vessel.ship == nullptr;
        });
        if (slot == vessels.end()) {
            vessels.push_back(Vessel{worldLeft - 0.08f, true, 0.0f});
            slot = vessels.end() - 1;
        }
        *slot = Vessel{worldLeft - 0.08f, true, randomRange(0.0f, 6.283185307f),
            randomRange(0.12f, 0.44f), randomRange(0.02f, 0.035f), randomRange(0.4f, 0.7f)};
        slot->patrolStart = time + (slot->patrolCenter - worldLeft + 0.08f) / 0.045f;
        if (!createVessel(*slot)) return false;
        slot->nextAttack += slot->patrolStart;
        return true;
    }

    void updateEnemyAttacks(float time)
    {
        const auto& player = vessels[0];
        if (player.health == 0) return;
        const auto& playerTransform = player.ship->transform();
        const float playerX = playerTransform.e11 * 90.0f + playerTransform.e12 * 78.0f + playerTransform.e13;
        const float playerY = playerTransform.e21 * 90.0f + playerTransform.e22 * 78.0f + playerTransform.e23;
        const float playerVelocity = (static_cast<int>(movingRight) - static_cast<int>(movingLeft)) * size().w * 0.105f;
        const float gravity = size().h * 0.5f;
        for (auto& vessel : vessels) {
            if (!vessel.enemy || vessel.health == 0 || time < vessel.patrolStart) continue;
            if (!vessel.attackCharging) {
                if (time < vessel.nextAttack) continue;
                vessel.attackCharging = true;
                vessel.attackCharge = randomRange(2.8f, 4.0f);
                vessel.nextAttack = time + vessel.attackCharge;
                // A third of the shots track the player; the rest deliberately aim wide.
                vessel.aimOffset = randomRange(0.0f, 1.0f) < 0.33f ? 0.0f :
                    randomRange(0.16f, 0.27f) * size().w * (randomRange(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
            }
            const auto& m = vessel.ship->transform();
            const float baseX = m.e11 * 138.0f + m.e12 * 127.0f + m.e13;
            const float baseY = m.e21 * 138.0f + m.e22 * 127.0f + m.e23;
            const float shipScale = std::hypot(m.e11, m.e21);
            const float speed = std::sqrt(0.135f * size().w * size().h * vessel.attackCharge);
            float vx = 0.0f, vy = 0.0f, flight = 0.0f;
            float muzzleX = baseX, muzzleY = baseY;
            for (unsigned iteration = 0; iteration < 4; ++iteration) {
                const float targetX = std::clamp(playerX + playerVelocity * flight,
                    size().w * playerLeft, size().w * playerRight) + vessel.aimOffset;
                const float dx = targetX - muzzleX;
                const float distance = std::max(std::abs(dx), 1.0f);
                const float dy = playerY - muzzleY;
                const float speed2 = speed * speed;
                const float discriminant = speed2 * speed2 - gravity * (gravity * distance * distance - 2.0f * dy * speed2);
                const float angle = discriminant >= 0.0f
                    ? std::clamp(std::atan((speed2 - std::sqrt(discriminant)) / (gravity * distance)),
                        0.174532925f, 1.396263402f) : 0.785398163f;
                vx = (dx < 0.0f ? -1.0f : 1.0f) * speed * std::cos(angle);
                vy = -speed * std::sin(angle);
                flight = distance / std::abs(vx);
                muzzleX = baseX + vx / speed * 27.0f * shipScale;
                muzzleY = baseY + vy / speed * 27.0f * shipScale;
            }
            const float localX = (m.e11 * vx + m.e21 * vy) / (shipScale * speed);
            const float localY = (m.e12 * vx + m.e22 * vy) / (shipScale * speed);
            vessel.cannon->reset();
            vessel.cannon->moveTo(138.0f, 127.0f);
            vessel.cannon->lineTo(138.0f + localX * 27.0f, 127.0f + localY * 27.0f);
            if (time < vessel.nextAttack) continue;
            auto& ball = cannonballs[nextCannonball];
            nextCannonball = (nextCannonball + 1) % 32;
            ball.x = muzzleX;
            ball.y = muzzleY;
            ball.vx = vx;
            ball.vy = vy;
            ball.active = true;
            randomizeSpeedLines(ball);
            ball.enemyShot = true;
            ball.submerged = false;
            ball.sinkTime = 0.0f;
            positionCannonball(ball);
            ball.shape->opacity(255);
            vessel.attackCharging = false;
            vessel.nextAttack = time + 3.0f;
        }
    }

    struct Star
    {
        Shape* shape = nullptr;
        float phase = 0.0f;
        float speed = 0.0f;
        float brightness = 0.0f;
    };
    Star stars[45];

    void twinkle(float time)
    {
        for (auto& star : stars) {
            const float shimmer = 0.65f + 0.35f * std::sin(time * star.speed + star.phase);
            star.shape->opacity(static_cast<uint8_t>(star.brightness * shimmer));
        }
    }

    struct Cloud
    {
        Scene* scene = nullptr;
        Shape* shape = nullptr;
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f, speed = 0.0f;
        float delay = 0.0f;
    };
    Cloud clouds[5];
    std::mt19937 randomEngine{std::random_device{}()};

    float randomRange(float minimum, float maximum)
    {
        return std::uniform_real_distribution<float>(minimum, maximum)(randomEngine);
    }

    void placeCloud(Cloud& cloud, const Size& size)
    {
        cloud.scene->transform(Matrix{size.w * cloud.width / 1000.0f, 0.0f, size.w * cloud.x,
            0.0f, size.h * cloud.height / 100.0f, size.h * cloud.y, 0.0f, 0.0f, 1.0f});
    }

    void shapeCloud(Cloud& cloud, float altitude)
    {
        constexpr float w = 1000.0f;
        constexpr float h = 100.0f;
        auto shape = cloud.shape;
        shape->reset();
        shape->moveTo(0.0f, h * 0.65f);
        shape->cubicTo(w * 0.08f, h * 0.35f, w * 0.13f, h * 0.50f, w * 0.22f, h * 0.30f);
        shape->cubicTo(w * 0.31f, -h * 0.15f, w * 0.41f, h * 0.05f, w * 0.49f, h * 0.25f);
        shape->cubicTo(w * 0.62f, h * 0.02f, w * 0.72f, h * 0.42f, w * 0.80f, h * 0.40f);
        shape->cubicTo(w * 0.89f, h * 0.42f, w * 0.95f, h * 0.65f, w, h * 0.70f);
        constexpr unsigned lobes = 3;
        float previousX = w;
        float previousY = h * 0.70f;
        for (unsigned i = 1; i <= lobes; ++i) {
            const float t = static_cast<float>(i) / lobes;
            const float x = w * (1.0f - t);
            const float y = h * (0.70f - 0.05f * t + 0.10f * std::sin(3.141592654f * t));
            const float bulge = h * (0.025f + 0.065f * altitude) * randomRange(0.9f, 1.1f);
            const float span = previousX - x;
            shape->cubicTo(previousX - span * 0.33f, previousY + bulge,
                x + span * 0.33f, y + bulge, x, y);
            previousX = x;
            previousY = y;
        }
        shape->close();
    }

    void spawnCloud(Cloud& cloud, const Size& size, bool initial)
    {
        cloud.width = randomRange(0.18f, 0.36f);
        cloud.y = randomRange(0.12f, 0.48f);
        const float altitude = (0.48f - cloud.y) / (0.48f - 0.12f);
        cloud.height = 0.035f + 0.105f * altitude;
        cloud.speed = 0.008f + 0.010f * altitude;
        shapeCloud(cloud, altitude);
        cloud.x = initial ? randomRange(worldLeft - cloud.width, worldRight - 0.05f) : worldLeft - cloud.width - 0.025f;
        cloud.delay = initial ? 0.0f : randomRange(1.0f, 7.0f);
        cloud.scene->opacity(initial ? 255 : 0);
        placeCloud(cloud, size);
    }

    size_t lastFrame = 0;
    bool movingLeft = false;
    bool movingRight = false;

    ThorPirate(const string& title, const Size& size) : App(title, size, true) {}

    void waves(const Size& size, float time)
    {
        constexpr unsigned segments = 96;
        const float width = static_cast<float>(size.w);
        const float baseline = size.h * distantSeaLevel;
        const float span = worldRight - worldLeft;
        const float left = width * worldLeft;
        const float right = width * worldRight;
        const float bottom = size.h * worldRight;
        const float step = width * span / segments;
        sea->reset();
        sunClip->reset();
        const auto firstSample = surfaceSample(worldLeft, time);
        float previousY = size.h * firstSample.height;
        float previousTangent = size.h * firstSample.slope * span / segments;
        sea->moveTo(left, previousY);
        sunClip->moveTo(left, previousY);
        for (unsigned i = 1; i <= segments; ++i) {
            const float x = left + step * i;
            const float u = worldLeft + span * i / segments;
            const auto water = surfaceSample(u, time);
            const float y = size.h * water.height;
            const float slope = size.h * water.slope * span / segments;
            const float cx1 = x - step * (2.0f / 3.0f);
            const float cy1 = previousY + previousTangent / 3.0f;
            const float cx2 = x - step / 3.0f;
            const float cy2 = y - slope / 3.0f;
            sea->cubicTo(cx1, cy1, cx2, cy2, x, y);
            sunClip->cubicTo(cx1, cy1, cx2, cy2, x, y);
            previousY = y;
            previousTangent = slope;
        }
        sea->lineTo(right, bottom);
        sea->lineTo(left, bottom);
        sea->close();
        for (auto& layer : seaLayers) {
            auto sample = [&](float u, float& y, float& slope) {
                const auto water = layerSample(layer, u, time);
                y = size.h * water.height;
                slope = size.h * water.slope * span / segments;
            };
            float previousY, previousSlope;
            sample(worldLeft, previousY, previousSlope);
            layer.shape->reset();
            layer.shape->moveTo(left, previousY);
            for (unsigned i = 1; i <= segments; ++i) {
                float y, slope;
                sample(worldLeft + span * i / segments, y, slope);
                const float x = left + step * i;
                layer.shape->cubicTo(x - step * (2.0f / 3.0f), previousY + previousSlope / 3.0f,
                    x - step / 3.0f, y - slope / 3.0f, x, y);
                previousY = y;
                previousSlope = slope;
            }
            layer.shape->lineTo(right, bottom);
            layer.shape->lineTo(left, bottom);
            layer.shape->close();
        }
        sunClip->lineTo(right, size.h * worldLeft);
        sunClip->lineTo(left, size.h * worldLeft);
        sunClip->close();

        // Refresh the sun geometry so the renderer also updates its animated clip.
        const float radius = size.h * sunRadiusRatio;
        sun->reset();
        sun->appendCircle(size.w * 0.5f, baseline, radius, radius);
        sunGlow->reset();
        sunGlow->appendCircle(size.w * 0.5f, baseline, radius * 2.5f, radius * 2.5f);

        for (auto& vessel : vessels) {
            auto ship = vessel.ship;
            if (!ship) continue;
            if (vessel.health == 0) {
                const float age = std::max(0.0f, time - vessel.sinkStart);
                if (age >= 3.0f) {
                    if (vessel.enemy) {
                        addScore(100);
                        showScorePopup(100, width * vessel.position,
                            size.h * sailingSurface(vessel.position, time).height, time);
                    }
                    fleet->remove(ship);
                    vessel.ship = nullptr;
                    if (vessel.reflection) reflectionLayer->remove(vessel.reflection);
                    vessel.reflection = nullptr;
                    vessel.rig = nullptr;
                    vessel.sail = nullptr;
                    vessel.sailFolds = nullptr;
                    vessel.sailHighlights = nullptr;
                    vessel.flag = nullptr;
                    vessel.emblem = nullptr;
                    vessel.cannon = nullptr;
                    vessel.fire = nullptr;
                    for (auto& flame : vessel.flames) flame = nullptr;
                    for (auto& core : vessel.flameCores) core = nullptr;
                    continue;
                }
                updateFlames(vessel, time);
                const float progress = age / 3.0f;
                const float angle = vessel.sinkAngle + progress * 0.65f;
                const float scale = width * 0.10f / 180.0f;
                const float c = std::cos(angle) * scale;
                const float s = std::sin(angle) * scale;
                const float y = vessel.sinkY + size.h * (0.02f * age + 0.025f * age * age);
                ship->transform(Matrix{c, -s, width * vessel.position - c * 90.0f + s * shipWaterline,
                    s, c, y - s * 90.0f - c * shipWaterline, 0, 0, 1});
                ship->opacity(static_cast<uint8_t>(255.0f * (1.0f - progress * progress)));
                continue;
            }
            updateFlames(vessel, time);
            auto rig = vessel.rig;
            auto sail = vessel.sail;
            auto flag = vessel.flag;
            auto emblem = vessel.emblem;
            const float gust = std::sin(time * 2.4f + vessel.phase);
            const float flutter = std::sin(time * 7.0f + vessel.phase);
            const float shear = 0.018f * gust;
            rig->transform(Matrix{1.0f, shear, -132.0f * shear,
                0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f});
            sail->reset();
            sail->moveTo(48.0f, 51.0f);
            sail->cubicTo(70.0f, 55.0f + gust * 4.0f, 105.0f, 55.0f - gust * 3.0f, 127.0f, 51.0f);
            sail->cubicTo(118.0f + gust * 5.0f, 66.0f, 120.0f + gust * 7.0f, 83.0f, 130.0f, 98.0f);
            sail->cubicTo(104.0f, 92.0f + gust * 5.0f, 72.0f, 96.0f - gust * 4.0f, 46.0f, 99.0f);
            sail->cubicTo(55.0f + gust * 6.0f, 83.0f, 56.0f + gust * 5.0f, 66.0f, 48.0f, 51.0f);
            sail->close();
            vessel.sailFolds->reset();
            vessel.sailHighlights->reset();
            for (unsigned i = 0; i < 4; ++i) {
                const float x = 62.0f + 16.0f * i;
                const float bend = 3.0f + gust * 3.0f + std::sin(time * 2.4f + i * 0.8f + vessel.phase);
                const float top = 58.0f + gust * 1.5f;
                const float bottom = 90.0f + gust * 2.0f;
                vessel.sailFolds->moveTo(x, top);
                vessel.sailFolds->cubicTo(x + bend, 67.0f, x + bend, 81.0f, x - 1.0f, bottom);
                vessel.sailHighlights->moveTo(x - 1.4f, top);
                vessel.sailHighlights->cubicTo(x + bend - 1.4f, 67.0f,
                    x + bend - 1.4f, 81.0f, x - 2.4f, bottom);
            }
            flag->reset();
            flag->moveTo(89.0f, 3.0f);
            flag->cubicTo(100.0f, -2.0f + flutter * 2.0f, 112.0f, 3.0f - flutter * 3.0f, 124.0f, 6.0f + flutter * 4.0f);
            flag->lineTo(117.0f, 11.0f + flutter * 4.0f);
            flag->lineTo(123.0f, 18.0f + flutter * 4.0f);
            flag->cubicTo(111.0f, 15.0f - flutter * 3.0f, 100.0f, 10.0f + flutter * 2.0f, 89.0f, 15.0f);
            flag->close();
            emblem->translate(0.0f, flutter * 1.2f);
            const float u = vessel.position;
            const float hitAge = time - vessel.hitStart;
            const float shake = (hitAge >= 0.0f && hitAge < 1.2f)
                ? std::sin(hitAge * 32.0f) * std::exp(-hitAge * 4.0f) : 0.0f;
            const auto water = sailingSurface(u, time);
            const float angle = std::atan(size.h * water.slope / width) + shake * 0.15f;
            const float scale = width * 0.10f / 180.0f;
            const float c = std::cos(angle) * scale;
            const float s = std::sin(angle) * scale;
            ship->transform(Matrix{c, -s, width * u - c * 90.0f + s * shipWaterline + shake * width * 0.003f,
                s, c, size.h * water.height - s * 90.0f - c * shipWaterline, 0.0f, 0.0f, 1.0f});
        }
    }

    bool content(tvg::Canvas* canvas, const Size& size) override
    {
        screen = Scene::gen();
        if (canvas->add(screen) != Result::Success) return false;
        world = Scene::gen();
        if (screen->add(world) != Result::Success) return false;
        auto background = Shape::gen();
        background->appendRect(size.w * worldLeft, size.h * worldLeft, size.w * 1.25f, size.h * 1.25f);
        auto sky = LinearGradient::gen();
        sky->linear(0.0f, 0.0f, 0.0f, static_cast<float>(size.h));
        const Fill::ColorStop skyStops[] = {
            {0.0f, 69, 54, 99, 255},
            {0.35f, 173, 78, 101, 255},
            {distantSeaLevel, 255, 175, 62, 255},
            {1.0f, 237, 112, 67, 255}
        };
        sky->colorStops(skyStops, 4);
        background->fill(sky);
        if (world->add(background) != Result::Success) return false;

        for (auto& star : stars) {
            const float x = size.w * randomRange(0.025f, 0.975f);
            const float altitude = randomRange(0.025f, 0.23f);
            const float radius = randomRange(0.8f, 1.7f) * size.h / 1280.0f;
            star.shape = Shape::gen();
            star.shape->appendCircle(x, size.h * altitude, radius, radius);
            star.shape->fill(255, 240, 215);
            star.phase = randomRange(0.0f, 6.283185307f);
            star.speed = randomRange(0.8f, 2.2f);
            star.brightness = randomRange(170.0f, 245.0f) * (1.0f - altitude * 2.0f);
            if (world->add(star.shape) != Result::Success) return false;
        }
        twinkle(0.0f);

        const float y = size.h * distantSeaLevel;
        const float radius = size.h * sunRadiusRatio;
        sun = Shape::gen();
        sun->appendCircle(size.w * 0.5f, y, radius, radius);
        auto sunlight = LinearGradient::gen();
        sunlight->linear(0.0f, y - radius, 0.0f, y + radius);
        const Fill::ColorStop sunStops[] = {
            {0.0f, 255, 235, 170, 255},
            {0.5f, 255, 165, 85, 255},
            {1.0f, 240, 95, 65, 255}
        };
        sunlight->colorStops(sunStops, 3);
        sun->fill(sunlight);
        auto blurredSun = Scene::gen();
        if (blurredSun->add(sun) != Result::Success) return false;
        if (blurredSun->add(SceneEffect::GaussianBlur, 5.0, 0, 0, 20) != Result::Success) return false;

        auto visibleSun = Scene::gen();
        const float glowRadius = radius * 2.5f;
        sunGlow = Shape::gen();
        sunGlow->appendCircle(size.w * 0.5f, y, glowRadius, glowRadius);
        auto glow = RadialGradient::gen();
        glow->radial(size.w * 0.5f, y, glowRadius, size.w * 0.5f, y, 0.0f);
        const Fill::ColorStop glowStops[] = {
            {0.0f, 255, 225, 150, 150},
            {0.36f, 255, 205, 115, 125},
            {0.50f, 255, 175, 90, 75},
            {0.72f, 255, 145, 80, 25},
            {1.0f, 255, 135, 75, 0}
        };
        glow->colorStops(glowStops, 5);
        sunGlow->fill(glow);
        if (visibleSun->add(sunGlow) != Result::Success) return false;
        if (visibleSun->add(blurredSun) != Result::Success) return false;
        sunClip = Shape::gen();
        sunClip->fill(255, 255, 255);
        visibleSun->clip(sunClip);
        if (world->add(visibleSun) != Result::Success) return false;

        // Static vector brush strokes and coastal silhouettes are built only once.
        auto scenery = Scene::gen();
        scenery->transform(Matrix{static_cast<float>(size.w), 0, 0,
            0, static_cast<float>(size.h), 0, 0, 0, 1});
        for (unsigned i = 0; i < 24; ++i) {
            const float x = randomRange(worldLeft, 1.0f);
            const float y = randomRange(0.26f, 0.65f);
            const float length = randomRange(0.07f, 0.26f);
            const float thickness = randomRange(0.002f, 0.007f);
            auto streak = Shape::gen();
            streak->moveTo(x, y);
            streak->cubicTo(x + length * .25f, y - thickness,
                x + length * .40f, y + thickness, x + length * .57f, y - thickness);
            streak->lineTo(x + length * .70f, y - thickness * .5f);
            streak->lineTo(x + length, y);
            streak->cubicTo(x + length * .65f, y + thickness,
                x + length * .3f, y + thickness * .7f, x, y);
            streak->close();
            streak->fill(255, 153, 65, static_cast<uint8_t>(75 + i % 4 * 20));
            if (scenery->add(streak) != Result::Success) return false;
        }
        for (unsigned depth = 0; depth < 3; ++depth) {
            auto ridge = Shape::gen();
            const float base = distantSeaLevel + 0.025f;
            ridge->moveTo(worldLeft, base);
            for (unsigned i = 0; i <= 40; ++i) {
                const float x = worldLeft + 1.25f * i / 40.0f;
                // Leave open water around the sun; taller headlands frame the sides.
                const float edge = std::pow(std::min(1.0f, std::abs(x - .5f) * 2.0f), 3.0f);
                const float height = (.018f + edge * (.10f + depth * .028f))
                    * (.65f + .35f * std::sin(i * 1.7f + depth * 2.1f));
                ridge->lineTo(x, distantSeaLevel + depth * .014f - height);
            }
            ridge->lineTo(worldRight, base);
            ridge->close();
            const uint8_t colors[3][3] = {{185, 99, 104}, {132, 78, 106}, {85, 63, 94}};
            ridge->fill(colors[depth][0], colors[depth][1], colors[depth][2]);
            if (scenery->add(ridge) != Result::Success) return false;
        }
        if (world->add(scenery) != Result::Success) return false;

        for (auto& cloud : clouds) {
            constexpr float h = 100.0f;
            auto shape = Shape::gen();
            cloud.shape = shape;

            auto tint = LinearGradient::gen();
            tint->linear(0.0f, 0.0f, 0.0f, h);
            const Fill::ColorStop cloudStops[] = {
                {0.0f, 255, 160, 69, 235},
                {0.16f, 175, 84, 106, 240},
                {1.0f, 112, 66, 100, 225}
            };
            tint->colorStops(cloudStops, 3);
            shape->fill(tint);
            cloud.scene = Scene::gen();
            if (cloud.scene->add(shape) != Result::Success) return false;
            spawnCloud(cloud, size, true);
            if (world->add(cloud.scene) != Result::Success) return false;
        }

        crateLayer = Scene::gen();
        if (world->add(crateLayer) != Result::Success) return false;
        fleet = Scene::gen();
        if (world->add(fleet) != Result::Success) return false;
        for (auto& vessel : vessels) {
            if (!createVessel(vessel)) return false;
        }

        cannon = Shape::gen();
        aimCannon();
        cannon->strokeWidth(9.0f);
        cannon->strokeFill(45, 43, 55);
        if (vessels[0].ship->add(cannon) != Result::Success) return false;
        aimGuide = Shape::gen();
        aimGuide->fill(255, 255, 255);
        if (world->add(aimGuide) != Result::Success) return false;
        for (auto& ball : cannonballs) {
            ball.shape = Shape::gen();
            const float radius = size.h * 0.0105f * 0.8f;
            ball.speedLines = Shape::gen();
            ball.speedLines->fill(0, 0, 0, 0);
            ball.speedLines->strokeWidth(std::max(1.0f, size.h * 0.0012f));
            ball.speedGradient = LinearGradient::gen();
            ball.speedGradient->linear(-radius * 10.0f, 0.0f, 0.0f, 0.0f);
            const Fill::ColorStop speedStops[] = {
                {0.0f, 255, 255, 255, 0},
                {0.65f, 255, 255, 255, 150},
                {1.0f, 255, 255, 255, 230}
            };
            ball.speedGradient->colorStops(speedStops, 3);
            ball.speedLines->strokeFill(ball.speedGradient);
            ball.speedLines->opacity(0);
            if (world->add(ball.speedLines) != Result::Success) return false;
            ball.shape->appendCircle(0.0f, 0.0f, radius, radius);
            auto metal = RadialGradient::gen();
            metal->radial(radius * 0.45f, 0.0f, radius * 1.5f, radius * 0.45f, 0.0f, 0.0f);
            const Fill::ColorStop metalStops[] = {
                {0.0f, 255, 237, 185, 255},
                {0.12f, 245, 183, 105, 255},
                {0.30f, 177, 106, 66, 255},
                {0.53f, 85, 60, 65, 255},
                {0.78f, 38, 35, 52, 255},
                {1.0f, 15, 19, 31, 255}
            };
            metal->colorStops(metalStops, 6);
            ball.shape->fill(metal);
            ball.shape->opacity(0);
            if (world->add(ball.shape) != Result::Success) return false;
        }

        sea = Shape::gen();

        auto gradient = LinearGradient::gen();
        gradient->linear(0.0f, y, 0.0f, static_cast<float>(size.h));
        const Fill::ColorStop stops[] = {
            {0.0f, 65, 47, 85, 235},
            {1.0f, 10, 23, 49, 255}
        };
        gradient->colorStops(stops, 2);
        sea->fill(gradient);
        if (world->add(sea, crateLayer) != Result::Success) return false;

        for (auto& layer : seaLayers) {
            layer.shape = Shape::gen();
            auto depth = LinearGradient::gen();
            depth->linear(0.0f, size.h * (layer.level - layer.amplitude * 1.25f),
                0.0f, static_cast<float>(size.h));
            const float depthRatio = std::clamp((layer.level - 0.73f) / 0.14f, 0.0f, 1.0f);
            const Fill::ColorStop depthStops[] = {
                {0.0f, static_cast<uint8_t>(166 - 55 * depthRatio),
                    static_cast<uint8_t>(110 - 35 * depthRatio),
                    static_cast<uint8_t>(128 - 20 * depthRatio), 175},
                {0.18f, static_cast<uint8_t>(85 - 35 * depthRatio),
                    static_cast<uint8_t>(65 - 24 * depthRatio),
                    static_cast<uint8_t>(103 - 24 * depthRatio), 190},
                {0.55f, 31, 31, 64, 210},
                {1.0f, 8, 19, 42, 230}
            };
            depth->colorStops(depthStops, 4);
            layer.shape->fill(depth);
            if (world->add(layer.shape) != Result::Success) return false;
            auto reflection = createSunReflection(size, 1 + (&layer - seaLayers));
           if (world->add(reflection) != Result::Success) return false;
        }

        reflectionLayer = Scene::gen();
        reflectionClip = Shape::gen();
        if (reflectionLayer->clip(reflectionClip) != Result::Success) return false;
        if (reflectionLayer->blend(BlendMethod::Multiply) != Result::Success) return false;
        if (world->add(reflectionLayer) != Result::Success) return false;

        for (auto& drop : droplets) {
            drop.shape = Shape::gen();
            drop.shape->fill(255, 255, 255);
            drop.shape->opacity(0);
            if (world->add(drop.shape) != Result::Success) return false;
        }

        for (auto& piece : debris) {
            piece.shape = Shape::gen();
            piece.shape->opacity(0);
            if (world->add(piece.shape) != Result::Success) return false;
        }

        chargeGauge = Scene::gen();
        auto gaugeBackground = Shape::gen();
        gaugeBackground->appendRect(0.0f, 0.0f, 120.0f, 14.0f, 4.0f, 4.0f);
        gaugeBackground->fill(30, 25, 45, 230);
        gaugeBackground->strokeWidth(1.0f);
        gaugeBackground->strokeFill(255, 220, 170, 220);
        if (chargeGauge->add(gaugeBackground) != Result::Success) return false;
        chargeFill = Shape::gen();
        if (chargeGauge->add(chargeFill) != Result::Success) return false;
        chargeGauge->opacity(0);
        if (world->add(chargeGauge) != Result::Success) return false;

        if (!fontLoaded) {
            if (Text::load(FONT_NAME, reinterpret_cast<const char*>(FONT_DATA), sizeof(FONT_DATA), "ttf") != Result::Success) return false;
            fontLoaded = true;
        }
        for (auto& popup : scorePopups) {
            popup.text = Text::gen();
            if (popup.text->font(FONT_NAME) != Result::Success) return false;
            popup.text->align(0.5f, 1.0f);
            popup.text->opacity(0);
            if (world->add(popup.text) != Result::Success) return false;
        }
        auto titleText = Text::gen();
        if (titleText->font(FONT_NAME) != Result::Success) return false;
        titleText->size(24.0f);
        titleText->text("ThorVG Pirates");
        titleText->fill(255, 240, 215);
        titleText->translate(16.0f, 16.0f);
        if (screen->add(titleText) != Result::Success) return false;
        scoreText = Text::gen();
        if (scoreText->font(FONT_NAME) != Result::Success) return false;
        scoreText->size(18.0f);
        scoreText->align(0.5f, 0.0f);
        scoreText->fill(255, 240, 215);
        scoreText->translate(size.w * 0.5f, 16.0f);
        addScore(0);
        if (screen->add(scoreText) != Result::Success) return false;

        auto keyGuide = Text::gen();
        if (keyGuide->font(FONT_NAME) != Result::Success) return false;
        keyGuide->size(14.0f);
        keyGuide->text("LEFT / RIGHT: Move ship\nUP / DOWN: Adjust cannon angle\nSPACE: Hold to charge, release to fire");
        keyGuide->fill(255, 240, 215);
        keyGuide->align(1.0f, 1.0f);
        keyGuide->translate(static_cast<float>(size.w) - 20.0f, static_cast<float>(size.h) - 20.0f);
        if (screen->add(keyGuide) != Result::Success) return false;

        gameOver = Scene::gen();
        auto shade = Shape::gen();
        shade->appendRect(0, 0, size.w, size.h);
        shade->fill(12, 10, 24, 125);
        if (gameOver->add(shade) != Result::Success) return false;
        auto title = Text::gen();
        if (title->font(FONT_NAME) != Result::Success) return false;
        title->size(size.h * 0.065f);
        title->text("Game Over");
        title->fill(255, 235, 207);
        title->align(0.5f, 0.5f);
        title->translate(size.w * 0.5f, size.h * 0.5f);
        if (gameOver->add(title) != Result::Success) return false;
        auto hint = Text::gen();
        if (hint->font(FONT_NAME) != Result::Success) return false;
        hint->size(size.h * 0.018f);
        hint->text("Press SPACE to restart");
        hint->fill(255, 235, 207);
        hint->align(0.5f, 0.5f);
        hint->translate(size.w * 0.5f, size.h * 0.58f);
        if (gameOver->add(hint) != Result::Success) return false;
        gameOver->opacity(0);
        if (screen->add(gameOver) != Result::Success) return false;

        waves(size, 0.0f);
        updateSunReflections(0.0f);
        updateReflections(0.0f);
        updateAimGuide();

        return true;
    }

    bool resetGame(tvg::Canvas* canvas)
    {
        canvas->sync();
        if (canvas->remove() != Result::Success) return false;
        vessels = {{0.75f, false, 0.0f},
            {0.12f, true, 0.8f, 0.12f, 0.03f, 0.65f},
            {0.25f, true, 1.6f, 0.25f, 0.025f, -0.50f},
            {0.38f, true, 2.4f, 0.38f, 0.03f, 0.40f}};
        crates.clear();
        for (auto& popup : scorePopups) popup = {};
        nextScorePopup = 0;
        for (auto& impact : impacts) impact = {};
        for (auto& ball : cannonballs) ball = {};
        for (auto& drop : droplets) drop = {};
        for (auto& piece : debris) piece = {};
        nextImpact = nextCannonball = nextDroplet = nextDebris = 0;
        score = 0;
        nextEnemySpawn = 20000;
        lastFrame = 0;
        screenShakeTime = 0.0f;
        cameraZoom = 1.0f;
        cameraX = cameraY = 0.5f;
        cannonAngle = 35.0f;
        movingLeft = movingRight = aimingUp = aimingDown = charging = false;
        chargeStart = reloadReady = {};
        restartRequested = false;
        return content(canvas, size());
    }

    bool keydown(tvg::Canvas*, Key key) override
    {
        if (key == Key::Escape) quit();
        if (key == Key::Space) {
            if (spaceHeld) return false;
            spaceHeld = true;
            if (vessels[0].health == 0) {
                restartRequested = true;
                return false;
            }
        }
        if (vessels[0].health == 0) return false;
        if (key == Key::Left) movingLeft = true;
        if (key == Key::Right) movingRight = true;
        if (key == Key::Up) aimingUp = true;
        if (key == Key::Down) aimingDown = true;
        if (key == Key::Space && !charging && std::chrono::steady_clock::now() >= reloadReady) {
            charging = true;
            chargeStart = std::chrono::steady_clock::now();
        }
        return false;
    }

    bool keyup(tvg::Canvas*, Key key) override
    {
        if (key == Key::Space) spaceHeld = false;
        if (key == Key::Left) movingLeft = false;
        if (key == Key::Right) movingRight = false;
        if (key == Key::Up) aimingUp = false;
        if (key == Key::Down) aimingDown = false;
        if (key == Key::Space && charging) {
            charging = false;
            fire(std::chrono::duration<float>(std::chrono::steady_clock::now() - chargeStart).count());
        }
        return false;
    }

    bool update(tvg::Canvas* canvas, size_t elapsed) override
    {
        if (restartRequested) {
            gameStart = elapsed;
            if (!resetGame(canvas)) { quit(); return false; }
            return canvas->update() == Result::Success;
        }
        elapsed -= gameStart;
        if (elapsed - lastFrame < 16) return false;
        const float dt = std::min(static_cast<float>((elapsed - lastFrame) * 0.001), 0.05f);
        const float cloudDt = static_cast<float>((elapsed - lastFrame) * 0.001);
        lastFrame = elapsed;
        const int aimDirection = static_cast<int>(aimingUp) - static_cast<int>(aimingDown);
        if (aimDirection != 0 && vessels[0].health > 0) {
            cannonAngle = std::clamp(cannonAngle + aimDirection * 40.0f * dt, 10.0f, 80.0f);
            aimCannon();
        }
        const int direction = static_cast<int>(movingRight) - static_cast<int>(movingLeft);
        auto& player = vessels[0];
        constexpr float moveSpeed = 0.12f * 0.70f * 1.25f;
        if (player.health > 0) player.position = std::clamp(player.position + direction * moveSpeed * dt, playerLeft, playerRight);
        while (elapsed >= nextEnemySpawn) {
            if (!spawnEnemy(static_cast<float>(nextEnemySpawn * 0.001))) {
                quit();
                return false;
            }
            nextEnemySpawn += 20000;
        }
        const float patrolTime = static_cast<float>(elapsed * 0.001);
        for (auto& vessel : vessels) {
            if (!vessel.enemy || vessel.health == 0) continue;
            if (patrolTime < vessel.patrolStart) {
                vessel.position = vessel.patrolCenter - (vessel.patrolStart - patrolTime) * 0.045f;
            } else {
                vessel.position = vessel.patrolCenter + vessel.patrolRadius *
                    std::sin((patrolTime - vessel.patrolStart) * vessel.patrolSpeed);
            }
        }
        waves(size(), static_cast<float>(elapsed * 0.001));
        updateSunReflections(static_cast<float>(elapsed * 0.001));
        updateReflections(static_cast<float>(elapsed * 0.001));
        updateChargeGauge();
        updateAimGuide();
        updateCannonballs(cloudDt, static_cast<float>(elapsed * 0.001));
        if (vessels[0].health == 0) {
            gameOver->opacity(255);
            movingLeft = movingRight = aimingUp = aimingDown = charging = false;
            chargeGauge->opacity(0);
            aimGuide->opacity(0);
        }
        updateCrates(cloudDt, static_cast<float>(elapsed * 0.001));
        updateEnemyAttacks(static_cast<float>(elapsed * 0.001));
        updateDroplets(static_cast<float>(elapsed * 0.001));
        updateDebris(static_cast<float>(elapsed * 0.001));
        twinkle(static_cast<float>(elapsed * 0.001));
        for (auto& cloud : clouds) {
            float travelTime = cloudDt;
            if (cloud.delay > 0.0f) {
                const float waiting = std::min(cloud.delay, travelTime);
                cloud.delay -= waiting;
                travelTime -= waiting;
                if (cloud.delay > 0.0f) continue;
                cloud.scene->opacity(255);
            }
            cloud.x += cloud.speed * travelTime;
            if (cloud.x > worldRight + 0.025f) spawnCloud(cloud, size(), false);
            else placeCloud(cloud, size());
        }
        updateScorePopups(static_cast<float>(elapsed * 0.001));
        updateCamera(cloudDt);
        updateScreenShake(cloudDt);
        return canvas->update() == Result::Success;
    }
};

RenderEngine options(int argc, char **argv, string& title)
{
    // -e <engine>
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "-e") == 0) {
            if (strcmp(argv[i + 1], "sw") == 0) {
                title = "ThorVG Pirate (CPU)";
                return RenderEngine::CPU;
            }
            if (strcmp(argv[i + 1], "gl") == 0) {
                title = "ThorVG Pirate (OpenGL)";
                return RenderEngine::GL;
            }
            if (strcmp(argv[i + 1], "wg") == 0) {
                title = "ThorVG Pirate (WebGPU)";
                return RenderEngine::WEBGPU;
            }
            break;
        }
    }
    title = "ThorVG Pirate (OpenGL)";
    return RenderEngine::GL;
}

int main(int argc, char** argv)
{
    string title;
    auto engine = options(argc, argv, title);

    Initializer::init(4);
    toolkit::run(new ThorPirate(title, {1920, 1280}), engine);
    Initializer::term();

    return 0;
}
