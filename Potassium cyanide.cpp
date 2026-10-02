#include <Windows.h>
#include <vector>
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")
#define PI 3.14159265358979323846

typedef union _RGBQUAD {
    COLORREF rgb;
    struct { BYTE r; BYTE g; BYTE b; BYTE Reserved; };
}_RGBQUAD, * PRGBQUAD;

PRGBQUAD rgbquad = { 0 };


typedef unsigned char (*BeatFunc)(int);

struct BeatHandle {
    HANDLE thread;
    HANDLE stopEvent;
    BeatFunc formula;
};

// 线程参数结构
struct BeatThreadParam {
    BeatFunc formula;
    HANDLE stopEvent;
};

// 循环合成短块并播放
static DWORD WINAPI PlayBeatThread(LPVOID lpParam) {
    BeatThreadParam* p = static_cast<BeatThreadParam*>(lpParam);
    if (!p) return 0;
    const int sampleRate = 8000;
    const int chunkMs = 500;
    const int chunkSamples = sampleRate * chunkMs / 1000;

    WAVEFORMATEX wf = {};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = sampleRate;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = (wf.nChannels * wf.wBitsPerSample) / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    HWAVEOUT hwo = NULL;
    if (waveOutOpen(&hwo, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        delete p;
        return 0;
    }

    unsigned int t = 0;
    while (WaitForSingleObject(p->stopEvent, 0) != WAIT_OBJECT_0) {
        std::vector<short> pcm(chunkSamples);
        for (int i = 0; i < chunkSamples; ++i) {
            unsigned char v = p->formula(static_cast<int>(t));
            pcm[i] = static_cast<short>((static_cast<int>(v) - 128) << 8);
            ++t;
        }

        WAVEHDR wh = {};
        wh.lpData = reinterpret_cast<LPSTR>(pcm.data());
        wh.dwBufferLength = static_cast<DWORD>(pcm.size() * sizeof(short));

        if (waveOutPrepareHeader(hwo, &wh, sizeof(wh)) != MMSYSERR_NOERROR) break;
        if (waveOutWrite(hwo, &wh, sizeof(wh)) != MMSYSERR_NOERROR) {
            waveOutUnprepareHeader(hwo, &wh, sizeof(wh));
            break;
        }

        while (!(wh.dwFlags & WHDR_DONE)) {
            if (WaitForSingleObject(p->stopEvent, 0) == WAIT_OBJECT_0) break;
            Sleep(5);
        }
        waveOutUnprepareHeader(hwo, &wh, sizeof(wh));
    }

    waveOutReset(hwo);
    waveOutClose(hwo);
    delete p;
    return 0;
}

// 启动播放，返回 BeatHandle
static BeatHandle StartBeat(BeatFunc formula) {
    BeatHandle bh = { NULL, NULL, formula };
    BeatThreadParam* p = new BeatThreadParam();
    p->formula = formula;
    p->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // manual-reset
    DWORD tid = 0;
    bh.thread = CreateThread(NULL, 0, PlayBeatThread, p, 0, &tid);
    bh.stopEvent = p->stopEvent;
    return bh;
}

// 停止并清理
static void StopBeat(BeatHandle& bh) {
    if (!bh.thread) return;
    SetEvent(bh.stopEvent);
    WaitForSingleObject(bh.thread, INFINITE);
    CloseHandle(bh.thread);
    CloseHandle(bh.stopEvent);
    bh.thread = NULL;
    bh.stopEvent = NULL;
}
//bytebeat公式
static unsigned char beat1(int t) { return (unsigned char)(t * ((t >> 12 | t >> 8) & 63 & t >> 4)); }
static unsigned char beat2(int t) { return (unsigned char)(8E5 * t / (t >> 2 ^ t >> 12)); }
static unsigned char beat3(int t) { return (unsigned char)((t >> 1) * (1 << (t >> 16 & 3)) * (0x1fccccf1 >> (t >> 10) * (t >> 11) & 7) | t >> 3 | 128 - ((t & 4095) >> 8 ? 0 : t * (t ^ t % 9) & 63 | t >> 2)); }
static unsigned char beat4(int t) { { return (unsigned char)(10 * (t >> 7 | 3 * t | t >> (t >> 15)) + (t >> 8 & 5)); } }
static unsigned char beat5(int t) { { return (unsigned char)((9 * t & t >> 4 | 5 * t & t >> 7 | 3 * t & t >> 10) - 1); } }
static unsigned char beat6(int t) { return (unsigned char)(((-t & 4095) * (255 & t * (t & t >> 13)) >> 12) + (127 & t * (234 & t >> 8 & t >> 3) >> (3 & t >> 14))); }

void Alphablend() {
	BeatHandle bh = StartBeat(beat1);
    HDC hdc = GetDC(NULL);
   /* HICON hIcon = (HICON)LoadImage(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDI_ICON1),
        IMAGE_ICON,
        48, 48,
        LR_DEFAULTCOLOR
    );*/
    HICON hicon = LoadIcon(NULL, IDI_ERROR);
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(mem, bmp);

    int x = 0, y = 0;

    for (int t = 0; t <= 400; t++)
    {
        HBRUSH hBrush = CreateSolidBrush(RGB(rand() % 256, rand() % 256, rand() % 256));
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);

        int x = rand() % (100);
        int y = rand() % (100);

        BitBlt(mem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

		PatBlt(hdc, 0, 0, w, h, PATINVERT);

        DrawIconEx(hdc, rand() % w, rand() % h, hicon, rand() % 512 - 16, rand() % 512 - 16, 0, NULL, DI_NORMAL);

        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 100, 0 };
        AlphaBlend(hdc, x, y, w, h, mem, 0, 0, w, h, bf);

        Sleep(5);
    }
	StopBeat(bh);
}
void Geometry() {
    BeatHandle bh = StartBeat(beat2);
    HDC hdc = GetDC(NULL);
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    int i = 1;

    for (int t=1;t<=100;t++) {
        HBRUSH hBrush = CreateSolidBrush(RGB(rand() % 256, rand() % 256, rand() % 256));
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);
		PatBlt(hdc, 0, 0, w, h, PATINVERT);
        StretchBlt(hdc, 0, 0, w + i * 10, h + i * 10, hdc, 0, 0, w, h, SRCCOPY);
		Pie(hdc, rand() % w, rand() % h, rand() % w, rand() % h, rand() % w, rand() % 512, rand() % 512, rand() % 512);
		Ellipse(hdc, rand() % w, rand() % h, rand() % w, rand() % h);
        LineTo(hdc, rand() % w, rand() % h);
		Rectangle(hdc, rand() % w, rand() % h, rand() % w, rand() % h);
        Polygon(hdc, new POINT[3]{ {rand() % w, rand() % h}, {rand() % w, rand() % h}, {rand() % w, rand() % h} }, 3);
		Arc(hdc, rand() % w, rand() % h, rand() % w, rand() % h, rand() % w, rand() % 512, rand() % 512, rand() % 512);
        Sleep(20);
    }
}
void ShaderColor() {
    BeatHandle bh = StartBeat(beat3);
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC hdc = GetDC(0);
    HDC hcdc = CreateCompatibleDC(hdc);
    HDC cdc2 = CreateCompatibleDC(hdc);
    HICON hicon1 = LoadCursor(NULL, IDC_PIN)
        , hicon2 = LoadCursor(NULL, IDC_HELP)
        , hicon3 = LoadCursor(NULL, IDC_NO);
    for (int i = 1; i <= 400; i++) {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
            (void**)&rgbquad, NULL, 0);
        SelectObject(hcdc, bmp);

        HBITMAP bmp2 = CreateCompatibleBitmap(hdc, w, h), old = (HBITMAP)SelectObject(cdc2, bmp2);
        BitBlt(hcdc, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                rgbquad[x + w * y].r += x + w * y;
                rgbquad[x + w * y].g += 114;
                rgbquad[x + w * y].b += 778;
            }
        }

        BitBlt(cdc2, 0, 0, w, h, hcdc, 0, 0, SRCCOPY);
        for (int y = 0; y < h; y++) {
            int rrr = rand() % 200;
            BitBlt(hcdc, -rrr, y, w, 1, cdc2, 0, y, SRCCOPY);
            BitBlt(hcdc, w - rrr, y, w, 1, cdc2, 0, y, SRCCOPY);
        }
        BitBlt(hdc, 0, 0, w, h, hcdc, 0, 0, SRCCOPY);
        DrawIconEx(hdc, rand() % w, rand() % h, hicon1, rand() % 256 - 16, rand() % 256 - 16, NULL, NULL, DI_NORMAL);
        DrawIconEx(hdc, rand() % w, rand() % h, hicon2, rand() % 256 - 16, rand() % 256 - 16, NULL, NULL, DI_NORMAL);
        DrawIconEx(hdc, rand() % w, rand() % h, hicon3, rand() % 256 - 16, rand() % 256 - 16, NULL, NULL, DI_NORMAL);
        Sleep(10);

    }
    StopBeat(bh);
}
void ShaderColor2() {
	BeatHandle bh = StartBeat(beat4);
        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);
        HDC hdc = GetDC(0);
        HDC mem = CreateCompatibleDC(hdc);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        int t = 0;
        for (int i = 1; i <= 400;i++) {
            PRGBQUAD px;
            HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&px, NULL, 0);

            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

            BitBlt(mem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int idx = x + w * y;
                    px[idx].rgb *= (x ^ y) + 3;
                    px[idx].r = (px[idx].r * (t & 7)) & 255;
                    px[idx].g = (px[idx].g * ((t + 3) & 7)) & 255;
                    px[idx].b = (px[idx].b * ((t + 6) & 7)) & 255;
                }
            }

            BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
            Sleep(10);
            DeleteObject(bmp);
        }
        StopBeat(bh);
}
void ShaderColor3() {
    BeatHandle bh = StartBeat(beat5);
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC hdc = GetDC(0);
    HDC mem = CreateCompatibleDC(hdc);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    int t = 0;
    for (int i = 1; i <= 400; i++) {
        PRGBQUAD px;
        HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&px, NULL, 0);

        HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

        BitBlt(mem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                px[x + w * y].rgb = px[((x * (t % 8 + 2)) % w) + w * y].rgb;

		BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCINVERT);
        Sleep(10);
        DeleteObject(bmp);
    }
    StopBeat(bh);
}
void ShaderColor4() {
    BeatHandle bh = StartBeat(beat6);

    HICON hIcon = LoadCursor(NULL, IDC_PERSON);
    const int iconSize = 128;

    int x = 100;
    int y = 100;
    int dx = 40;
    int dy = 30;
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    HDC hdc = GetDC(0);
    HDC mem = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    int t = 0;
    PRGBQUAD px;
    HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&px, NULL, 0);
    HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);
    for (int i = 1; i <= 400; i++) {
        x += dx;
        y += dy;
        if (x < 0) { x = 0; dx = -dx; }
        if (y < 0) { y = 0; dy = -dy; }
        if (x + iconSize > w) { x = w - iconSize; dx = -dx; }
        if (y + iconSize > h) { y = h - iconSize; dy = -dy; }
        DrawIconEx(hdc, x, y, hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);

        BitBlt(mem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                px[x + w * y].rgb ^= ((x ^ y) + t) * rand()%123-118;

        BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
        Sleep(10);
    }
    StopBeat(bh);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow){
    if (MessageBoxW(NULL, L"Do you want to run the program? All responsibilities are yours! It will seriously affect your desktop.", L"WARNING!!!", MB_YESNO | MB_ICONWARNING) == IDYES) {
        Alphablend();
        Geometry();
        ShaderColor4();
        ShaderColor3();
        ShaderColor2();
        ShaderColor();
    }
}