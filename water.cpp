#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <array>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>

struct Vector2
{
    float x, y;

    void operator+=(const Vector2 &other)
    {
        this->x += other.x;
        this->y += other.y;
    }

    Vector2 operator*(const float &scalar)
    {
        Vector2 result;
        result.x = scalar * this->x;
        result.y = scalar * this->y;
        return result;
    }

    Vector2 operator-(const float &scalar)
    {
        Vector2 result;
        result.x = this->x - scalar;
        result.y = this->y - scalar;
        return result;
    }

    Vector2 operator-(const Vector2 &other)
    {
        return Vector2{this->x - other.x, this->y - other.y};
    }
};

uint64_t lastTick = 0u;

struct AppData
{
    SDL_Renderer *renderer;
    SDL_Window *window;
    std::array<Vector2, 100> positions;
    std::array<Vector2, 100> velocities;
    std::array<float, 100> densities;
    bool running = true;
    float gravity = 9.8f;
    float radius = 0.075f;
    float bounceConstant = 0.25;
    float spacing = 0.2f;
    TTF_Font *font;
    int frameCount = 0;
    float frameTimer = 0.f;
    float smoothingRadius = 1;
};

class TextElement
{
public:
    TextElement()
        : font(nullptr), renderer(nullptr),
          texture(nullptr), rect{0, 0, 0, 0} {}
    TextElement(TTF_Font *font, SDL_Renderer *renderer)
        : font(font), renderer(renderer), texture(nullptr), rect{0, 0, 0, 0} {}
    ~TextElement()
    {
        cleanup();
    }

    void init(TTF_Font *newFont, SDL_Renderer *newRenderer)
    {
        cleanup();
        font = newFont;
        renderer = newRenderer;
    }

    void set_text(const std::string &text, SDL_Color color, int x, int y, bool solid = false)
    {
        cleanup();
        SDL_Surface *surf;
        if (solid)
        {
            surf = TTF_RenderText_Solid(font, text.c_str(), 0, color);
        }
        else
        {
            surf = TTF_RenderText_Blended(font, text.c_str(), 0, color);
        }
        if (!surf)
            return;
        texture = SDL_CreateTextureFromSurface(renderer, surf);
        if (texture)
        {
            rect.x = x;
            rect.y = y;
            rect.w = surf->w;
            rect.h = surf->h;
        }
        SDL_DestroySurface(surf);
    }

    void set_text_right(const std::string &text, SDL_Color color, int rightEdge, int y, bool solid = false)
    {
        set_text(text, color, 0, y, solid);

        if (texture)
            rect.x = rightEdge - rect.w - 5;
    }

    void render_text()
    {
        if (texture)
        {
            SDL_RenderTexture(renderer, texture, nullptr, &rect);
        }
    }

    SDL_FRect *get_rect()
    {
        return &rect;
    }

private:
    void cleanup()
    {
        if (texture)
        {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    }

    TTF_Font *font;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_FRect rect;
};

TextElement fpsCounter;

// helper functions

float smoothing_kernel(float distance, float radius)
{
    float value = std::max(0.f, radius - distance);
    return (value * value * value) / (3.14159265359f * radius * radius * radius * radius * radius) * 10.f;
}

float smoothing_kernel_slope(float distance, float radius)
{
    if (distance >= radius)
        return 0;
    float numerator = 30 * (radius - distance) * (radius - distance);
    float denominator = 3.14159265359f * radius * radius * radius * radius * radius;
    return -numerator / denominator;
}

float calculate_density(int idx, AppData &app)
{
    Vector2 thisPos = app.positions.at(idx);
    float density = 0;
    for (int i = 0; i < app.positions.size(); ++i)
    {
        if (i == idx)
        {
            continue;
        }
        Vector2 offset = app.positions.at(i) - thisPos;
        float squaredDst = offset.x * offset.x + offset.y * offset.y;
        if (squaredDst > (app.smoothingRadius * app.smoothingRadius))
        {
            continue;
        }
        float dst = std::sqrt(squaredDst);
        density += smoothing_kernel(dst, app.smoothingRadius);
    }
    return density;
}

void update_densities(AppData &app)
{
    for (int i = 0; i < app.positions.size(); ++i)
    {
        app.densities.at(i) = calculate_density(i, app);
    }
}

// main loop

void update_balls(AppData &app)
{
    uint64_t currentTick = SDL_GetTicks();
    float deltaTime = (currentTick - lastTick) / 1000.f;
    lastTick = currentTick;
    app.frameTimer += deltaTime;
    int w, h;
    SDL_GetWindowSizeInPixels(app.window, &w, &h);
    float worldW = static_cast<float>(w) / h * 10;
    float worldH = 10.f;

    for (int i = 0; i < app.positions.size(); ++i)
    {
        Vector2 &pos = app.positions.at(i);
        Vector2 &vel = app.velocities.at(i);
        vel += Vector2{0.f, app.gravity} * deltaTime;
        pos += vel * deltaTime;

        if (pos.x < app.radius || pos.x > (worldW - app.radius))
        {
            float before = pos.x;
            pos.x = std::clamp(pos.x, app.radius, worldW - app.radius);
            vel.x *= -app.bounceConstant;
            pos += vel * deltaTime;
            int iterations = 0;
            while (pos.x < app.radius || pos.x > (worldW - app.radius))
            {
                pos.x += pos.x - before;
                iterations += 1;
                if (iterations > 5)
                {
                    break;
                }
            }
        }
        if (pos.y < app.radius || pos.y > (worldH - app.radius))
        {
            float before = pos.y;
            pos.y = std::clamp(pos.y, app.radius, worldH - app.radius);
            vel.y *= -app.bounceConstant;
            pos += vel * deltaTime;
            int iterations = 0;
            while (pos.y < app.radius || pos.y > (worldH - app.radius))
            {
                pos.y += pos.y - before;
                iterations += 1;
                if (iterations > 5)
                {
                    break;
                }
            }
        }
    }
}

void render(AppData &app)
{
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
    SDL_RenderClear(app.renderer);
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 255, 255);
    for (int i = 0; i < app.positions.size(); ++i)
    {
        Vector2 pos = app.positions.at(i) - app.radius;
        int height;
        SDL_GetWindowSizeInPixels(app.window, nullptr, &height);
        pos.x = pos.x / 10 * height;
        pos.y = pos.y / 10 * height;
        SDL_FRect drawRect = {pos.x, pos.y, app.radius * 2.f / 10 * height, app.radius * 2.f / 10 * height};
        SDL_RenderFillRect(app.renderer, &drawRect);
    }
    fpsCounter.render_text();
    SDL_RenderPresent(app.renderer);
}

void setup_balls(AppData &app)
{
    int side_count = std::ceil(std::sqrt(app.positions.size()));
    int w, h;
    SDL_GetWindowSizeInPixels(app.window, &w, &h);
    Vector2 corner = Vector2{
        (w / static_cast<float>(h)) * 5.f - (side_count - 1) / 2.f * app.spacing,
        5.f - (side_count - 1) / 2.f * app.spacing};
    for (int i = 0; i < app.positions.size(); ++i)
    {
        app.positions.at(i) = Vector2{app.spacing * (i % side_count) + corner.x, app.spacing * std::floor(static_cast<float>(i) / side_count) + corner.y};
        app.velocities.at(i) = Vector2{0, 0};
    }
}

void shutdown(AppData &app)
{
    TTF_CloseFont(app.font);
    fpsCounter.init(nullptr, nullptr);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    TTF_Quit();
}

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Initialization failed");
        return -1;
    }

    if (!TTF_Init())
    {
        SDL_Log("Text initialization failed");
        return -1;
    }

    AppData app;
    app.window = SDL_CreateWindow("watery water", 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN);
    app.renderer = SDL_CreateRenderer(app.window, nullptr);
    SDL_SetRenderVSync(app.renderer, 1);
    setup_balls(app);
    app.font = TTF_OpenFont("/Users/minjun/gimkit-clone/OpenSans-SemiBold.ttf", 64);
    fpsCounter.init(app.font, app.renderer);
    while (app.running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                app.running = false;
            }

            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_Q)
                {
                    app.running = false;
                }
            }

            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                setup_balls(app);
            }
        }

        // update the fps counter
        if (app.frameTimer >= 0.5f)
        {
            std::stringstream ss;
            ss << "FPS: " << std::round(app.frameCount / app.frameTimer);
            fpsCounter.set_text(ss.str(), SDL_Color{255, 255, 255, 255}, 5, 5);
            app.frameCount = 0;
            app.frameTimer = 0;
        }
        else
        {
            app.frameCount += 1;
        }

        update_densities(app);
        update_balls(app);
        render(app);
    }
    shutdown(app);
    return 0;
}