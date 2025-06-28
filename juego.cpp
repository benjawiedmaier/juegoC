// juego.cpp
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <cmath>
#include <algorithm>

const int WIDTH  = 480;
const int HEIGHT = 320;
const int SIZE   = 64;

const Uint32 PICKUP_TIMEOUT = 2000;
const Uint32 BOOST_DURATION = 1000;
const Uint32 BOOST_COOLDOWN = 5000;

const int BASE_SPEED  = 25;
const int BOOST_SPEED = 45;

enum class State { SPLASH, PLAYING, GAME_OVER };

// Distancia euclidiana entre centros
float dist(int x1, int y1, int x2, int y2) {
    int dx = x1 - x2;
    int dy = y1 - y2;
    return std::sqrt(dx*dx + dy*dy);
}

// Genera posición de pizza asegurando distancia mínima "no-zone"
SDL_Rect generarPizza(int score, SDL_Rect player) {
    int px, py;
    // zona de exclusión crece +20px cada 30 pizzas, hasta máximo 180px
    int minDist = std::min(20 + (score/30)*20, 180);
    do {
        px = rand() % (WIDTH - SIZE);
        py = rand() % (HEIGHT - SIZE);
    } while (dist(px + SIZE/2, py + SIZE/2, player.x + SIZE/2, player.y + SIZE/2) < minDist);
    return SDL_Rect{ px, py, SIZE, SIZE };
}

SDL_Texture* LoadTex(const char* path, SDL_Renderer* ren) {
    SDL_Surface* s = IMG_Load(path);
    if (!s) {
        std::cerr << "IMG_Load " << path << " : " << IMG_GetError() << "\n";
        return nullptr;
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_FreeSurface(s);
    return t;
}

SDL_Texture* RenderText(const char* msg, TTF_Font* f, SDL_Color c, SDL_Renderer* ren) {
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, msg, c);
    SDL_Texture* tex  = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    return tex;
}

void waitForKey(SDL_Renderer* ren, SDL_Texture* bg, SDL_Texture* txt1, SDL_Texture* txt2) {
    SDL_Event e;
    bool w = true;
    while (w) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);
            if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_Q)
                w = false;
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, bg, nullptr, nullptr);
        int w1,h1; SDL_QueryTexture(txt1,nullptr,nullptr,&w1,&h1);
        SDL_Rect r1{ (WIDTH-w1)/2, HEIGHT/3, w1, h1 };
        SDL_RenderCopy(ren, txt1, nullptr, &r1);
        int w2_,h2_; SDL_QueryTexture(txt2,nullptr,nullptr,&w2_,&h2_);
        SDL_Rect r2{ WIDTH-w2_-10, HEIGHT-h2_-10, w2_, h2_ };
        SDL_RenderCopy(ren, txt2, nullptr, &r2);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }
}

void waitForRestart(SDL_Renderer* ren, SDL_Texture* bg, SDL_Texture* txt_go, SDL_Texture* txt2) {
    SDL_Event e;
    bool w = true;
    while (w) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);
            if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_Q)
                w = false;
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, bg, nullptr, nullptr);
        int w1,h1; SDL_QueryTexture(txt_go,nullptr,nullptr,&w1,&h1);
        SDL_Rect r1{ (WIDTH-w1)/2, HEIGHT/3, w1, h1 };
        SDL_RenderCopy(ren, txt_go, nullptr, &r1);
        int w2_,h2_; SDL_QueryTexture(txt2,nullptr,nullptr,&w2_,&h2_);
        SDL_Rect r2{ WIDTH-w2_-10, HEIGHT-h2_-10, w2_, h2_ };
        SDL_RenderCopy(ren, txt2, nullptr, &r2);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }
}

int main(int argc, char* argv[]) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) < 0 ||
        IMG_Init(IMG_INIT_PNG) == 0 ||
        TTF_Init() < 0)
    {
        std::cerr << "Init error: " << SDL_GetError() << "\n";
        return 1;
    }
    srand((unsigned)time(nullptr));

    // Crear ventana fullscreen-desktop y alto DPI
    SDL_Window* win = SDL_CreateWindow(
        "Pizza Game",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        0, 0,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI
    );
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    // Ajustar tamaño lógico de render a 480×320
    SDL_RenderSetLogicalSize(ren, WIDTH, HEIGHT);

    // Cargar texturas y fuentes
    SDL_Texture* splashBg = LoadTex("assets/pizzeria.png", ren);
    SDL_Texture* bg       = LoadTex("assets/roads2.png",     ren);
    SDL_Texture* plyTex   = LoadTex("assets/player.png",     ren);
    SDL_Texture* pizzaTex = LoadTex("assets/pizza_piece.png",ren);

    TTF_Font* font = TTF_OpenFont("assets/font.ttf", 24);
    SDL_Color white{255,255,255,255};
    SDL_Texture* txtStart = RenderText("Game Start!",       font, white, ren);
    SDL_Texture* txtPress = RenderText("Press A to start",  font, white, ren);
    SDL_Texture* txtGameO = RenderText("Game Over",         font, white, ren);
    SDL_Texture* txtRetry = RenderText("Press A to restart",font, white, ren);

    State state = State::SPLASH;
    Uint32 lastPick = 0, boostStart = 0, nextBoost = 0;
    bool boostOn = false;
    int score = 0;
    int nextTurboScore = 30 + rand() % 11; // turbo automático entre 30 y 40 pizzas

    SDL_Rect player{ 60, 100, SIZE, SIZE };
    SDL_Rect pizza = generarPizza(score, player);

    // Splash
    waitForKey(ren, splashBg, txtStart, txtPress);
    state = State::PLAYING;
    lastPick = SDL_GetTicks();

    SDL_Event e;
    while (true) {
        Uint32 now = SDL_GetTicks();
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto end;
            if (state == State::PLAYING &&
                e.type == SDL_KEYDOWN &&
                e.key.keysym.scancode == SDL_SCANCODE_R)
            {
                // Reiniciar juego
                lastPick = now;
                boostOn = false;
                nextBoost = now;
                score = 0;
                nextTurboScore = 30 + rand() % 11;
                player = { 60, 100, SIZE, SIZE };
                pizza = generarPizza(score, player);
            }
        }

        if (state == State::PLAYING) {
            // Limpia fondo a negro (letterbox)
            SDL_SetRenderDrawColor(ren, 0,0,0,255);
            SDL_RenderClear(ren);

            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            // Turbo manual o automático
            if ((keys[SDL_SCANCODE_E] || score >= nextTurboScore)
                && !boostOn && now >= nextBoost)
            {
                boostOn = true;
                boostStart = now;
                if (score >= nextTurboScore)
                    nextTurboScore = score + 30 + rand() % 11;
            }
            if (boostOn && now - boostStart > BOOST_DURATION) {
                boostOn = false;
                nextBoost = now + BOOST_COOLDOWN - BOOST_DURATION;
            }
            int sp = boostOn ? BOOST_SPEED : BASE_SPEED;

            // Movimiento
            if (keys[SDL_SCANCODE_W]) player.y -= sp;
            if (keys[SDL_SCANCODE_S]) player.y += sp;
            if (keys[SDL_SCANCODE_A]) player.x -= sp;
            if (keys[SDL_SCANCODE_D]) player.x += sp;
            // Limites de pantalla lógica
            player.x = std::clamp(player.x, 0, WIDTH  - SIZE);
            player.y = std::clamp(player.y, 0, HEIGHT - SIZE);

            // Colisión y recogida
            if (SDL_HasIntersection(&player, &pizza)) {
                lastPick = now;
                score++;
                pizza = generarPizza(score, player);
            }
            // Tiempo agotado
            if (now - lastPick > PICKUP_TIMEOUT) {
                state = State::GAME_OVER;
            }

            // Dibujo de escena
            SDL_RenderCopy(ren, bg, nullptr, nullptr);
            SDL_RenderCopy(ren, pizzaTex, nullptr, &pizza);
            SDL_RenderCopy(ren, plyTex, nullptr, &player);

            // Barra de tiempo
            float pct = float(PICKUP_TIMEOUT - (now - lastPick)) / PICKUP_TIMEOUT;
            if (pct < 0) pct = 0;
            SDL_Rect bgBar{ WIDTH-110,10,100,20 }, fgBar{ WIDTH-110,10,int(100*pct),20 };
            SDL_SetRenderDrawColor(ren, 0,0,0,255);       SDL_RenderFillRect(ren, &bgBar);
            SDL_SetRenderDrawColor(ren, 255,50,50,255);   SDL_RenderFillRect(ren, &fgBar);
            SDL_SetRenderDrawColor(ren, 255,255,255,255);

            // Puntuación
            std::string scoreStr = "Pizzas: " + std::to_string(score);
            SDL_Texture* scoreTex = RenderText(scoreStr.c_str(), font, white, ren);
            int tw, th; SDL_QueryTexture(scoreTex, nullptr,nullptr,&tw,&th);
            SDL_Rect dst{10,10,tw,th};
            SDL_RenderCopy(ren, scoreTex, nullptr, &dst);
            SDL_DestroyTexture(scoreTex);

            SDL_RenderPresent(ren);
        }
        else if (state == State::GAME_OVER) {
            waitForRestart(ren, bg, txtGameO, txtRetry);
            // Reinicio tras game over
            player = { 60, 100, SIZE, SIZE };
            score = 0;
            nextTurboScore = 30 + rand() % 11;
            pizza = generarPizza(score, player);
            lastPick = SDL_GetTicks();
            boostOn = false;
            nextBoost = lastPick;
            state = State::PLAYING;
        }

        SDL_Delay(16);
    }

end:
    // Limpieza
    SDL_DestroyTexture(splashBg);
    SDL_DestroyTexture(bg);
    SDL_DestroyTexture(plyTex);
    SDL_DestroyTexture(pizzaTex);
    SDL_DestroyTexture(txtStart);
    SDL_DestroyTexture(txtPress);
    SDL_DestroyTexture(txtGameO);
    SDL_DestroyTexture(txtRetry);
    TTF_CloseFont(font);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
