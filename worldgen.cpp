// DISCLAIMER! This code is AI-Generated. I wanted to test how good AI's capabilities have gotten.
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>

struct Vec2
{
    float x, y;
    Vec2(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}

    Vec2 operator+(const Vec2 &o) const { return Vec2{x + o.x, y + o.y}; }
    Vec2 operator*(const float &scalar) const { return Vec2{x * scalar, y * scalar}; }

    float length() const
    {
        float len = std::sqrt(x * x + y * y);
        return len > 0.0f ? len : 1.0f;
    }

    Vec2 normalized() const
    {
        float len = length();
        return Vec2{x / len, y / len};
    }
};

// math helper funcs

float hash2D(float x, float y)
{
    float dot_product = x * 127.1f + y * 311.7f;
    float raw_value = std::sin(dot_product) * 43758.5453123f;
    return raw_value - std::floor(raw_value);
}

float smoothNoise(float x, float y)
{
    float ix = std::floor(x);
    float iy = std::floor(y);
    float fx = x - ix;
    float fy = y - iy;
    float ux = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
    float uy = fy * fy * fy * (fy * (fy * 6.0f - 15.0f) + 10.0f);
    float a = hash2D(ix, iy);
    float b = hash2D(ix + 1.0f, iy);
    float c = hash2D(ix, iy + 1.0f);
    float d = hash2D(ix + 1.0f, iy + 1.0f);
    return std::lerp(std::lerp(a, b, ux), std::lerp(c, d, ux), uy);
}

float getMacroBaseShape(float x, float y)
{
    float base = smoothNoise(x * 0.005f, y * 0.005f) * 400.0f;
    base += smoothNoise(x * 0.015f, y * 0.025f) * 100.0f;
    return base;
}

float getStripePattern(float flow_coord, float frequency)
{
    float wave = std::sin(flow_coord * frequency);
    return std::clamp(1.0f - std::abs(wave), 0.0f, 1.0f);
}

// main erosion engine

std::vector<float> generateErodedTerrain(int width, int height)
{
    std::vector<float> heightmap(width * height, 0.0f);

    // generate skeleton
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            heightmap[y * width + x] = getMacroBaseShape(static_cast<float>(x), static_cast<float>(y));
        }
    }

    // erosion parameters
    const int OCTAVES = 3;                // branching fractal layer count
    const float STRIPE_STRENGTH = 0.f;    // depth of carved canyons
    const float CURVATURE_BIAS = 0.4f;    // 0.0 = pure local flow, 1.0 = heavy downward curve
    const float DISTORTION_SCALE = 12.0f; // stream wander violence

    // step 2: apply math pipeline
    for (int octave = 0; octave < OCTAVES; ++octave)
    {
        std::vector<float> current_pass = heightmap;

        float freq_multiplier = std::pow(2.0f, static_cast<float>(octave));
        float current_strength = STRIPE_STRENGTH / freq_multiplier;

        for (int y = 1; y < height - 1; ++y)
        {
            for (int x = 1; x < width - 1; ++x)
            {
                int idx = y * width + x;

                // local gradient
                float dh_dx = current_pass[idx + 1] - current_pass[idx - 1];
                float dh_dy = current_pass[(y + 1) * width + x] - current_pass[(y - 1) * width + x];
                Vec2 local_gradient{dh_dx, dh_dy};

                float slope_steepness = local_gradient.length();
                if (slope_steepness < 0.001f)
                    continue;

                Vec2 local_flow_dir = local_gradient.normalized();

                // macro gradient
                int step = 8 >> octave;
                step = std::max(1, step);

                int x_left = std::max(0, x - step);
                int x_right = std::min(width - 1, x + step);
                int y_up = std::max(0, y - step);
                int y_down = std::min(height - 1, y + step);

                float macro_dx = current_pass[y * width + x_right] - current_pass[y * width + x_left];
                float macro_dy = current_pass[y_down * width + x] - current_pass[y_up * width + x];
                Vec2 macro_flow_dir = Vec2{macro_dx, macro_dy}.normalized() * -1.0f;

                // downward curvature bias
                Vec2 biased_flow_dir = (local_flow_dir * (1.0f - CURVATURE_BIAS)) + (macro_flow_dir * CURVATURE_BIAS);
                biased_flow_dir = biased_flow_dir.normalized();

                // inject noise distortion
                float noise_lookup = smoothNoise(x * 0.1f * freq_multiplier, y * 0.1f * freq_multiplier);
                float twist = (noise_lookup - 0.5f) * DISTORTION_SCALE;

                float flow_coordinate = (x * biased_flow_dir.x + y * biased_flow_dir.y) + twist;

                // eval stripes and carve
                float gully_cut = getStripePattern(flow_coordinate, 0.4f * freq_multiplier);

                float steepness_mask = std::clamp(slope_steepness * 0.2f, 0.0f, 1.0f);

                heightmap[idx] -= gully_cut * current_strength * steepness_mask;
            }
        }
    }
    std::cout << "Successfully generated terrain!" << std::endl;
    return heightmap;
}

void saveAsOBJ(const std::vector<float> &heightmap, int width, int height, const std::string &filename, float scaleX = 1.0f, float scaleY = 1.0f)
{
    std::ofstream file(filename);
    if (!file.is_open())
        return;

    // write vertices
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float z = heightmap[y * width + x];
            file << "v " << (x * scaleX) << " " << z << " " << (y * scaleY) << "\n";
        }
    }

    // write faces
    for (int y = 0; y < height - 1; ++y)
    {
        for (int x = 0; x < width - 1; ++x)
        {

            int topLeft = y * width + x + 1;
            int topRight = topLeft + 1;
            int bottomLeft = (y + 1) * width + x + 1;
            int bottomRight = bottomLeft + 1;

            file << "f " << topLeft << " " << bottomLeft << " " << bottomRight << "\n";
            file << "f " << topLeft << " " << bottomRight << " " << topRight << "\n";
        }
    }

    file.close();
    std::cout << "Exported 3D mesh to " << filename << std::endl;
}

int main()
{
    std::vector<float> result = generateErodedTerrain(512, 512);
    saveAsOBJ(result, 512, 512, "mountain-gen-result.obj");
    return 0;
}