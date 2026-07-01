#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <ctime>
#include <mmsystem.h>
#include <algorithm>

using namespace Gdiplus;

#pragma comment (lib, "winmm.lib")

enum GameState { START, PLAYING, GAMEOVER };

struct Pipe {
    float x;
    float gapY;
    bool passed;
};

// Constants
const int WINDOW_WIDTH = 288;
const int WINDOW_HEIGHT = 512;
const float GRAVITY = 0.4f;
const float JUMP_STRENGTH = -6.5f;
const int PIPE_GAP = 130;
const int PIPE_WIDTH = 52;
const float GAME_SPEED = 3.0f;

GameState currentState = START;
float birdY = 256.0f;
float birdVelocity = 0.0f;
float birdRotation = 0.0f;
int score = 0;
bool isMuted = false;
std::vector<Pipe> pipes;

ULONG_PTR gdiplusToken;
Image* imgBird;
Image* imgBackground;
Image* imgPipe;
Image* imgGetReady;
Image* imgGameOver;
Image* imgDigits[10];

// Audio System
void InitAudio() {
    // SFX
    mciSendStringA("open \"flappy-bird-assets-public/sounds/jump.wav\" type waveaudio alias jump", NULL, 0, NULL);
    mciSendStringA("open \"flappy-bird-assets-public/sounds/score.wav\" type waveaudio alias score", NULL, 0, NULL);
    mciSendStringA("open \"flappy-bird-assets-public/sounds/death.wav\" type waveaudio alias death", NULL, 0, NULL);

    // Set volumes (0 to 1000)
    mciSendStringA("setaudio score volume to 200", NULL, 0, NULL); // 20% volume
    mciSendStringA("setaudio jump volume to 500", NULL, 0, NULL);  // 50% volume

    // Background Music
    mciSendStringA("open \"flappy-bird-assets-public/sounds/backgroundsound.mp3\" type mpegvideo alias music", NULL, 0, NULL);
    mciSendStringA("setaudio music volume to 400", NULL, 0, NULL); // 40% volume
    mciSendStringA("play music repeat", NULL, 0, NULL);
}

void PlayGameSound(const char* alias) {
    if (isMuted) return;
    char cmd[100];
    wsprintfA(cmd, "play %s from 0", alias);
    mciSendStringA(cmd, NULL, 0, NULL);
}

void ToggleMute() {
    isMuted = !isMuted;
    if (isMuted) {
        mciSendStringA("pause music", NULL, 0, NULL);
    } else {
        mciSendStringA("resume music", NULL, 0, NULL);
    }
}

void ResetGame() {
    birdY = 256.0f;
    birdVelocity = 0.0f;
    birdRotation = 0.0f;
    score = 0;
    pipes.clear();
    pipes.push_back({ (float)WINDOW_WIDTH + 100, (float)(rand() % 200 + 100), false });
}

void Update() {
    if (currentState == START) return;

    if (currentState == PLAYING) {
        birdVelocity += GRAVITY;
        birdY += birdVelocity;
        birdRotation = birdVelocity * 3.0f;
        if (birdRotation > 90) birdRotation = 90;
        if (birdRotation < -20) birdRotation = -20;

        for (size_t i = 0; i < pipes.size(); i++) {
            pipes[i].x -= GAME_SPEED;
            RectF birdRect(50, birdY - 10, 25, 20);
            RectF topPipe(pipes[i].x, 0, (float)PIPE_WIDTH, pipes[i].gapY - (float)PIPE_GAP / 2);
            RectF bottomPipe(pipes[i].x, pipes[i].gapY + (float)PIPE_GAP / 2, (float)PIPE_WIDTH, (float)WINDOW_HEIGHT);

            if (birdRect.IntersectsWith(topPipe) || birdRect.IntersectsWith(bottomPipe)) {
                currentState = GAMEOVER;
                PlayGameSound("death");
            }
            if (!pipes[i].passed && pipes[i].x < 50) {
                pipes[i].passed = true;
                score++;
                // PlayGameSound("score"); // Disabled as requested
            }
        }

        if (pipes.size() > 0 && pipes.back().x < WINDOW_WIDTH - 160) {
            pipes.push_back({ (float)WINDOW_WIDTH, (float)(rand() % 200 + 100), false });
        }
        if (pipes.size() > 0 && pipes.front().x < -PIPE_WIDTH) {
            pipes.erase(pipes.begin());
        }

        if (birdY > WINDOW_HEIGHT || birdY < 0) {
            currentState = GAMEOVER;
            PlayGameSound("death");
        }
    }
}

void Draw(HDC hdc, HWND hwnd) {
    Graphics graphics(hdc);
    graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

    graphics.DrawImage(imgBackground, (REAL)0, (REAL)0, (REAL)WINDOW_WIDTH, (REAL)WINDOW_HEIGHT);

    for (const auto& pipe : pipes) {
        float topH = pipe.gapY - PIPE_GAP / 2;
        float botStart = pipe.gapY + PIPE_GAP / 2;
        graphics.DrawImage(imgPipe, RectF(pipe.x, 0, (REAL)PIPE_WIDTH, (REAL)topH - 20), 0.0f, 32.0f, 32.0f, 32.0f, UnitPixel);
        graphics.DrawImage(imgPipe, RectF(pipe.x, topH - 20, (REAL)PIPE_WIDTH, 20), 0.0f, 0.0f, 32.0f, 32.0f, UnitPixel);
        graphics.DrawImage(imgPipe, RectF(pipe.x, botStart, (REAL)PIPE_WIDTH, 20), 0.0f, 0.0f, 32.0f, 32.0f, UnitPixel);
        graphics.DrawImage(imgPipe, RectF(pipe.x, botStart + 20, (REAL)PIPE_WIDTH, (REAL)(WINDOW_HEIGHT - botStart - 20)), 0.0f, 32.0f, 32.0f, 32.0f, UnitPixel);
    }

    graphics.TranslateTransform(67.0f, (REAL)birdY);
    graphics.RotateTransform((REAL)birdRotation);
    if (imgBird) {
        graphics.DrawImage(imgBird, RectF(-15, -15, 30, 30), 0.0f, 0.0f, (REAL)imgBird->GetWidth(), (REAL)imgBird->GetHeight(), UnitPixel);
    }
    graphics.ResetTransform();

    if (currentState == START) {
        if (imgGetReady) graphics.DrawImage(imgGetReady, (REAL)((WINDOW_WIDTH - 184) / 2), (REAL)150, (REAL)184, (REAL)50);

        // Settings Button (Bottom Right)
        FontFamily fontFamily(L"Arial");
        Font font(&fontFamily, 14, FontStyleBold, UnitPixel);
        SolidBrush whiteBrush(Color(255, 255, 255, 255));
        SolidBrush grayBrush(Color(180, 0, 0, 0));

        graphics.FillRectangle(&grayBrush, WINDOW_WIDTH - 70, WINDOW_HEIGHT - 40, 60, 30);
        std::wstring muteText = isMuted ? L"UNMUTE" : L"MUTE";
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        graphics.DrawString(muteText.c_str(), -1, &font, RectF(WINDOW_WIDTH - 70, WINDOW_HEIGHT - 40, 60, 30), &format, &whiteBrush);
    } else if (currentState == GAMEOVER && imgGameOver) {
        graphics.DrawImage(imgGameOver, (REAL)((WINDOW_WIDTH - 192) / 2), (REAL)150, (REAL)192, (REAL)42);
    }

    // Score
    std::string s = std::to_string(score);
    int dw = 24;
    int tw = (int)s.length() * dw;
    for (int i = 0; i < (int)s.length(); i++) {
        int digit = s[i] - '0';
        if (imgDigits[digit]) {
            graphics.DrawImage(imgDigits[digit], (REAL)((WINDOW_WIDTH - tw) / 2 + i * dw), (REAL)50, (REAL)dw, (REAL)36);
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: SetTimer(hwnd, 1, 16, NULL); return 0;
        case WM_TIMER: Update(); InvalidateRect(hwnd, NULL, FALSE); return 0;
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // Mute Button Check (only in START screen)
            if (currentState == START && x >= WINDOW_WIDTH - 70 && x <= WINDOW_WIDTH - 10 && y >= WINDOW_HEIGHT - 40 && y <= WINDOW_HEIGHT - 10) {
                ToggleMute();
                return 0;
            }

            if (currentState == START || currentState == GAMEOVER) {
                if(currentState == GAMEOVER) ResetGame();
                currentState = PLAYING;
            }
            birdVelocity = JUMP_STRENGTH;
            PlayGameSound("jump");
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam != VK_SPACE) break;
            if (currentState == START || currentState == GAMEOVER) {
                if(currentState == GAMEOVER) ResetGame();
                currentState = PLAYING;
            }
            birdVelocity = JUMP_STRENGTH;
            PlayGameSound("jump");
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
            SelectObject(memDC, memBitmap);
            Draw(memDC, hwnd);
            BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, memDC, 0, 0, SRCCOPY);
            DeleteObject(memBitmap); DeleteDC(memDC); EndPaint(hwnd, &ps);
        }
        return 0;
        case WM_DESTROY:
            mciSendStringA("close jump", NULL, 0, NULL);
            mciSendStringA("close score", NULL, 0, NULL);
            mciSendStringA("close death", NULL, 0, NULL);
            mciSendStringA("close music", NULL, 0, NULL);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    InitAudio();
    imgBackground = Image::FromFile(L"flappy-bird-assets-public/Background/Background1.png");
    imgPipe = Image::FromFile(L"flappy-bird-assets-public/Tiles/Style 1/PipeStyle1.png");
    imgBird = Image::FromFile(L"flappy-bird-assets-public/Player/StyleBird2/birdalone.png");
    imgGetReady = Image::FromFile(L"flappy-bird-assets-public/UI Messages/textGetReady.png");
    imgGameOver = Image::FromFile(L"flappy-bird-assets-public/UI Messages/textGameOver.png");
    for(int i=0; i<10; i++) {
        std::wstring p = L"flappy-bird-assets-public/Numbers/number" + std::to_wstring(i) + L".png";
        imgDigits[i] = Image::FromFile(p.c_str());
    }
    const char CN[] = "FlappyBirdPublicClass";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CN;
    wc.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    RECT r = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);
    HWND hwnd = CreateWindowEx(0, CN, "Flappy Re-Bird",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    ResetGame();
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    GdiplusShutdown(gdiplusToken);
    return 0;
}
