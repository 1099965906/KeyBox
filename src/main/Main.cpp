// Win32Project2.cpp : ����Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <Shellapi.h>
#include <Windows.h>
#include <thread>
#include <mutex>
#include <comdef.h>
#include "main.h"
#include "KeyParser.h"
#include "manager\KeyManager.h"
#include <string>

#define MAX_LOADSTRING 100
#define USER_NOTIFY_ICON_CLICK (WM_USER+100)
#define APP_NAME L"KeyBox"

// ȫ�ֱ���: 
WCHAR szTitle[MAX_LOADSTRING];                  // �������ı�
WCHAR szWindowClass[MAX_LOADSTRING];            // ����������
HINSTANCE hInst;                                // ��ǰʵ��
HWND hWnd;										// �����ھ��
HHOOK keyBoardHook;								// ���̹���
HHOOK mouseHook;								// ��깳��
NOTIFYICONDATA stateIcon;						// ϵͳ����ͼ��
HANDLE hMutex;									// ����ֻ��������һ��ʵ��
HKEY hRunWhenBootKey;							// ��������ע����
std::wstring currentPath;						// ��ǰĿ¼
LPCTSTR bootKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
HMENU hMenu = GetSubMenu(LoadMenu((HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), MAKEINTRESOURCE(IDR_MENU)), 0);
std::mutex mutex;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	return parserKey(hWnd, keyBoardHook, lParam, nCode, wParam);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
	return parserMouse(hWnd, mouseHook, lParam, nCode, wParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// һ��ʵ��
	hMutex = OpenMutex(MUTEX_ALL_ACCESS, 0, APP_NAME);
	if (!hMutex)
		hMutex = CreateMutex(0, 0, APP_NAME);
	else
		return 0;

	// ����DPIʶ��
	SetProcessDPIAware();

	// ��ȡ��ǰĿ¼
	TCHAR moduleFileName[MAX_PATH];
	GetModuleFileName(NULL, moduleFileName, MAX_PATH);
	currentPath = std::wstring(moduleFileName);
	std::wstring sep(L"\\");
	auto it = currentPath.find_last_of(sep);
	if (it != std::wstring::npos) {
		currentPath = currentPath.substr(0, it + 1);
	}

	// ��ʼ��ȫ���ַ���
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WIN32PROJECT, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// ִ��Ӧ�ó����ʼ��: 
	if (!InitInstance(hInstance, nCmdShow))
		return FALSE;

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WIN32PROJECT));

	// ���ü���ȫ�ּ���
	keyBoardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
	if (keyBoardHook == NULL)
		MessageBox(hWnd, L"���̼���ʧ�ܣ�", TEXT("����"), MB_OK);

	// �������ȫ�ּ���
	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);
	if (mouseHook == NULL)
		MessageBox(hWnd, L"���̼���ʧ�ܣ�", TEXT("����"), MB_OK);

// ����Ϣѭ��: 
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WIN32PROJECT);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
	hInst = hInstance; // ��ʵ������洢��ȫ�ֱ�����
	hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd) return FALSE;
	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	static auto keyManager = KeyManager::getInstance();
	switch (message) {
		case WM_CREATE:
		{
			stateIcon.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
			stateIcon.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_APP));
			stateIcon.hWnd = hWnd;
			wcscpy_s(stateIcon.szTip, APP_NAME);
			stateIcon.uCallbackMessage = USER_NOTIFY_ICON_CLICK;
			Shell_NotifyIcon(NIM_ADD, &stateIcon);

			// ����Ƿ񿪻�����������ѡ�������������˵�
			RegOpenKeyEx(HKEY_CURRENT_USER, bootKey, 0, KEY_READ, &hRunWhenBootKey);
			DWORD fileNameSize = MAX_PATH;
			TCHAR myFileName1[MAX_PATH], myFileName2[MAX_PATH];
			GetModuleFileName(NULL, myFileName1, MAX_PATH);
			long a = RegGetValue(hRunWhenBootKey, NULL, APP_NAME, RRF_RT_ANY, NULL, myFileName2, &fileNameSize);
			if (wcscmp(myFileName1, myFileName2) == 0)
				CheckMenuItem(hMenu, ID_BOOT, MF_CHECKED | MF_BYCOMMAND);
			else
				CheckMenuItem(hMenu, ID_BOOT, MF_UNCHECKED | MF_BYCOMMAND);
			RegCloseKey(hRunWhenBootKey);
		}
		break;
		case USER_NOTIFY_ICON_CLICK:
		{
			POINT point;
			if (lParam == WM_RBUTTONUP) {
				GetCursorPos(&point);
				SetForegroundWindow(hWnd);
				TrackPopupMenu(hMenu, TPM_LEFTBUTTON, point.x, point.y, 0, hWnd, NULL);
			}
		}
		break;
		case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
				case ID_RECORD_DOUBLE_CLICK:
					if (keyManager->getRecordDoubleClick() == 0)
						keyManager->setRecordDoubleClick(1);
					else
						keyManager->setRecordDoubleClick(0);
					break;
				case ID_MOUSE_MODE_0:
				case ID_MOUSE_MODE_1:
				case ID_MOUSE_MODE_2:
				case ID_MOUSE_MODE_3:
				case ID_MOUSE_MODE_4:
					keyManager->setMouseMode(LOWORD(wParam) - ID_MOUSE_MODE_0);
					break;
				case ID_SLEEP_TIME_0:
				case ID_SLEEP_TIME_1:
				case ID_SLEEP_TIME_2:
				case ID_SLEEP_TIME_3:
				case ID_SLEEP_TIME_4:
				case ID_SLEEP_TIME_5:
				case ID_SLEEP_TIME_6:
				case ID_SLEEP_TIME_7:
				case ID_SLEEP_TIME_8:
				case ID_SLEEP_TIME_9:
					keyManager->setSleepTime(keyManager->getSleepTimeByLevel(LOWORD(wParam) - ID_SLEEP_TIME_0));
					break;
				case ID_LOOP_TIME_0:
				case ID_LOOP_TIME_1:
				case ID_LOOP_TIME_2:
				case ID_LOOP_TIME_3:
				case ID_LOOP_TIME_4:
				case ID_LOOP_TIME_5:
				case ID_LOOP_TIME_6:
				case ID_LOOP_TIME_7:
				case ID_LOOP_TIME_8:
				case ID_LOOP_TIME_9:
					keyManager->setLoopSleepTime(keyManager->getLoopTimeByLevel(LOWORD(wParam) - ID_LOOP_TIME_0));
					break;
				case ID_BOOT:
				{
					RegOpenKeyEx(HKEY_CURRENT_USER, bootKey, 0, KEY_WRITE, &hRunWhenBootKey);
					auto checkedState = CheckMenuItem(hMenu, ID_BOOT, MF_BYCOMMAND);
					if (checkedState == MF_CHECKED) {
						RegDeleteValue(hRunWhenBootKey, APP_NAME);
						CheckMenuItem(hMenu, ID_BOOT, MF_UNCHECKED | MF_BYCOMMAND);
					}
					else {
						TCHAR myFileName[MAX_PATH];
						GetModuleFileName(NULL, myFileName, MAX_PATH);
						RegSetValueEx(hRunWhenBootKey, APP_NAME, NULL, REG_SZ, (const BYTE *)myFileName, (wcslen(myFileName) + 1) * sizeof(TCHAR));
						CheckMenuItem(hMenu, ID_BOOT, MF_CHECKED | MF_BYCOMMAND);
					}
					RegCloseKey(hRunWhenBootKey);
				}
				break;
				case ID_EXIT:
					DestroyWindow(hWnd);
					ReleaseMutex(hMutex);
					Shell_NotifyIcon(NIM_DELETE, &stateIcon);
					break;
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		break;
		case WM_DESTROY:
			PostQuitMessage(0);
			ReleaseMutex(hMutex);
			Shell_NotifyIcon(NIM_DELETE, &stateIcon);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
