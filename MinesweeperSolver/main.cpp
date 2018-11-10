#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>
#include <d2d1.h>
#include <thread>
#pragma comment(lib,"d2d1")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void initD2D();
void updateOverlayLocation();
void readData();
void readWindowInfo();
void checkClosedWindow();

HWND hwnd_overlay;
HWND hwnd_game;
HANDLE h_game;

ID2D1Factory * pFactory = NULL;
ID2D1HwndRenderTarget   *pRenderTarget = NULL;
ID2D1SolidColorBrush    *pBrush;

// Minesweeper properties
const int startX = 15;
const int startY = 100;
const int squareLength = 16;

// Memory locations
DWORD arow = 0x01005334;
DWORD acolumn = 0x01005338;
DWORD afirstSquare = 0x01005361;

int rowSize;
int columnSize;
byte squareData[16][32];

template <class T> void SafeRelease(T **ppT) {
	if (*ppT) {
		(*ppT)->Release();
		*ppT = NULL;
	}
}

void readWindowInfo() {
	while (true) {
		updateOverlayLocation();
		readData();
		checkClosedWindow();
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
	// Get Minesweeper process
	hwnd_game = FindWindow(NULL, L"Minesweeper");
	if (hwnd_game == NULL) {
		return 0;
	}
	DWORD procID;
	GetWindowThreadProcessId(hwnd_game, &procID);
	h_game = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procID);

	const wchar_t CLASS_NAME[] = L"Sample Window Class";

	WNDCLASSEX wcex = {};
	ZeroMemory(&wcex, sizeof(WNDCLASSEX));
	wcex.lpfnWndProc = WindowProc;
	wcex.hInstance = hInstance;
	wcex.lpszClassName = CLASS_NAME;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);

	RegisterClassEx(&wcex);

	hwnd_overlay = CreateWindowEx(
		WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED,
		CLASS_NAME,
		L"",
		WS_POPUP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hwnd_overlay == NULL) {
		return 0;
	}

	initD2D();
	SetLayeredWindowAttributes(hwnd_overlay, RGB(255, 255, 255), 255, LWA_COLORKEY);
	ShowWindow(hwnd_overlay, nCmdShow);
	UpdateWindow(hwnd_overlay);
	std::thread t1(readWindowInfo);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

void initD2D() {
	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
}

void checkClosedWindow() {
	DWORD status;
	GetExitCodeProcess(h_game, &status);
	if (status != STILL_ACTIVE) {
		exit(0);
	}
}

void updateOverlayLocation() {
	RECT rc;
	GetWindowRect(hwnd_game, &rc);
	SetWindowPos(hwnd_overlay, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, NULL);
}

void readData() {
	ReadProcessMemory(h_game, (LPCVOID)arow, &columnSize, sizeof(columnSize), NULL);
	ReadProcessMemory(h_game, (LPCVOID)acolumn, &rowSize, sizeof(rowSize), NULL);
	ReadProcessMemory(h_game, (LPCVOID)afirstSquare, &squareData, sizeof(squareData), NULL);
}

HRESULT CreateGraphicsResources() {
	HRESULT hr = S_OK;
	if (pRenderTarget == NULL) {
		RECT rc;
		GetClientRect(hwnd_overlay, &rc);
		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
		hr = pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(hwnd_overlay, size),
			&pRenderTarget);

		if (SUCCEEDED(hr)) {
			const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 0, 0);
			hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);
		}

	}
	return hr;
}

void DiscardGraphicsResources()
{
	SafeRelease(&pRenderTarget);
	SafeRelease(&pBrush);
}

void OnPaint() {
	HRESULT hr = CreateGraphicsResources();
	if (SUCCEEDED(hr)) {
		PAINTSTRUCT ps;
		BeginPaint(hwnd_overlay, &ps);
		pRenderTarget->BeginDraw();
		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

		for (int i = 0; i < rowSize; i++) {
			for (int j = 0; j < columnSize; j++) {

				int x = startX + squareLength * j;
				int y = startY + squareLength * i;

				if (squareData[i][j] == 143) {
					pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + squareLength, y + squareLength), pBrush, 1.5f);
					pRenderTarget->DrawLine(D2D1::Point2F(x, y + squareLength), D2D1::Point2F(x + squareLength, y), pBrush, 1.5f);
				}
			}
		}
		hr = pRenderTarget->EndDraw();
		if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET) {
			DiscardGraphicsResources();
		}
		EndPaint(hwnd_overlay, &ps);
		InvalidateRect(hwnd_overlay, NULL, FALSE);
	}
}

void Resize()
{
	if (pRenderTarget != NULL)
	{
		RECT rc;
		GetClientRect(hwnd_overlay, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		pRenderTarget->Resize(size);
		InvalidateRect(hwnd_overlay, NULL, FALSE);
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_SIZE:
		Resize();
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}