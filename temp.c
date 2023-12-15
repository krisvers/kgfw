#include <stdio.h>
#include <windows.h>
#include <xinput.h>

HWND hwnd = NULL;
BOOL running = TRUE;
const char * battery_types[BATTERY_TYPE_NIMH + 2] = {
	"disconnected",
	"wired",
	"alkaline",
	"nimh",
	"unknown",
};

const char * battery_type(BYTE type) {
	if (type != BATTERY_TYPE_UNKNOWN) {
		return battery_types[type];
	}
	return battery_types[4];
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_CLOSE: {
		running = FALSE;
		return 0;
	}
	case WM_DESTROY: {
		PostQuitMessage(0);
		return 0;
	}
	}

	return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int main(int argc, char ** argv) {
	/*WNDCLASSEXA wc = {
		.cbSize = sizeof(WNDCLASSEXA),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = wnd_proc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = GetModuleHandle(NULL),
		.hIcon = LoadIcon(NULL, IDC_ARROW),
		.hCursor = LoadCursor(NULL, IDC_ARROW),
		.hbrBackground = (HBRUSH) GetStockObject(0),
		.lpszClassName = "d3d12",
		.hIconSm = LoadImageA(GetModuleHandle(NULL), MAKEINTRESOURCEA(5), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR),
	};

	RegisterClassExA(&wc);

	{
		RECT rect = {
			.top = 0,
			.left = 0,
			.right = 800,
			.bottom = 600,
		};
		AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

		hwnd = CreateWindowExA(0, wc.lpszClassName, "d3d12", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, wc.hInstance, NULL);
	}

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);*/

	XINPUT_STATE states[4];
	XINPUT_CAPABILITIES capabilites[4];
	XINPUT_BATTERY_INFORMATION battery_infos[4];
	BOOL connected[4];
	for (UINT i = 0; i < 4; ++i) {
		DWORD ret = XInputGetState(i, &states[i]);
		switch (ret) {
		case ERROR_SUCCESS:
			connected[i] = TRUE;
			//printf("state: buttons %u, lt %u, rt %u, left stick (%i, %i), right stick (%i, %i)\n", states[i].Gamepad.wButtons, states[i].Gamepad.bLeftTrigger, states[i].Gamepad.bRightTrigger, states[i].Gamepad.sThumbLX, states[i].Gamepad.sThumbLY, states[i].Gamepad.sThumbRX, states[i].Gamepad.sThumbRY);
			XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &capabilites[i]);
			//printf("types %u, subtype %u, flags %u\n", capabilites[i].Type, capabilites[i].SubType, capabilites[i].Flags);
			//printf("capable: buttons %u, lt %u, rt %u, left stick (%i, %i), right stick (%i, %i)\n", capabilites[i].Gamepad.wButtons, capabilites[i].Gamepad.bLeftTrigger, capabilites[i].Gamepad.bRightTrigger, capabilites[i].Gamepad.sThumbLX, capabilites[i].Gamepad.sThumbLY, capabilites[i].Gamepad.sThumbRX, capabilites[i].Gamepad.sThumbRY);
			XInputGetBatteryInformation(i, BATTERY_DEVTYPE_GAMEPAD, &battery_infos[i]);
			printf("battery type %s, %u%%\n", battery_type(battery_infos[i].BatteryType), (UINT) (battery_infos[i].BatteryLevel * 33.33333333f));
			break;
		case ERROR_DEVICE_NOT_CONNECTED:
			printf("controller %u not found\n", i);
			connected[i] = FALSE;
			break;
		default:
			printf("controller %u unknown error\n", i);
			connected[i] = FALSE;
			break;
		}
	}

	while (running) {
		for (UINT i = 0; i < 4; ++i) {
			XINPUT_STATE tmp;
			if (XInputGetState(i, &tmp) != ERROR_SUCCESS) {
				if (connected[i]) {
					printf("controller %u disconnected\n", i);
					connected[i] = FALSE;
				}
				continue;
			}
			if (!connected[i]) {
				printf("controller %u connected\n", i);
				connected[i] = TRUE;
			}

			if (memcmp(&tmp, &states[i], sizeof(XINPUT_STATE)) == 0) {
				continue;
			}

			memcpy(&states[i], &tmp, sizeof(XINPUT_STATE));
			printf("controller %u state: buttons %u, lt %u, rt %u, left stick (%i, %i), right stick (%i, %i)\n", i, states[i].Gamepad.wButtons, states[i].Gamepad.bLeftTrigger, states[i].Gamepad.bRightTrigger, states[i].Gamepad.sThumbLX, states[i].Gamepad.sThumbLY, states[i].Gamepad.sThumbRX, states[i].Gamepad.sThumbRY);
		}

		MSG msg;
		if (PeekMessageA(&msg, hwnd, 0, 0, PM_REMOVE) != 0) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}

	DestroyWindow(hwnd);

	return 0;
}