#include "kgfw_defines.h"

#if (KGFW_OPENGL == 33)

#include "kgfw.h"
#include "kgfw_window.h"
#include "kgfw_input.h"
#include <GLFW/glfw3.h>

static void kgfw_glfw_window_close(GLFWwindow * glfw_window);
static void kgfw_glfw_window_resize(GLFWwindow * glfw_window, int width, int height);

int kgfw_window_create(kgfw_window_t * out_window, unsigned int width, unsigned int height, char * title) {
	out_window->width = width;
	out_window->height = height;
	out_window->closed = 0;
	out_window->internal = NULL;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	#ifdef KGFW_APPLE_MACOS
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	#endif
	out_window->internal = glfwCreateWindow(width, height, title, NULL, NULL);
	if (out_window->internal == NULL) {
		kgfw_log(KGFW_LOG_SEVERITY_ERROR, "GLFW window creation failed");
		return 1;
	}
	glfwSetWindowUserPointer(out_window->internal, (void *) out_window);
	glfwSetWindowCloseCallback(out_window->internal, kgfw_glfw_window_close);
	glfwSetWindowSizeCallback(out_window->internal, kgfw_glfw_window_resize);
	if (kgfw_input_register_window(out_window) != 0) {
		glfwDestroyWindow(out_window->internal);
		return 2;
	}

	return 0;
}

void kgfw_window_destroy(kgfw_window_t * window) {
	glfwDestroyWindow(window->internal);
}

int kgfw_window_update(kgfw_window_t * window) {
	glfwSwapBuffers(window->internal);

	return 0;
}

static void kgfw_glfw_window_close(GLFWwindow * glfw_window) {
	kgfw_window_t * window = glfwGetWindowUserPointer(glfw_window);
	if (window == NULL) {
		kgfw_log(KGFW_LOG_SEVERITY_WARN, "GLFW window has invalid user pointer");
		return;
	}

	window->closed = 1;
}

static void kgfw_glfw_window_resize(GLFWwindow * glfw_window, int width, int height) {
	kgfw_window_t * window = glfwGetWindowUserPointer(glfw_window);
	if (window == NULL) {
		kgfw_log(KGFW_LOG_SEVERITY_WARN, "GLFW window has invalid user pointer");
		return;
	}

	window->width = (unsigned int) width;
	window->height = (unsigned int) height;
}


#elif (KGFW_DIRECTX == 11)

#include "kgfw.h"
#include "kgfw_input.h"
#include <windows.h>
#include <windowsx.h>

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

int kgfw_window_create(kgfw_window_t * out_window, unsigned int width, unsigned int height, char * title) {
	out_window->width = width;
	out_window->height = height;
	out_window->closed = 0;
	out_window->internal = NULL;
	
	const char * class_name = "krisvers' window class";
	WNDCLASSA cls;
	ZeroMemory(&cls, sizeof(cls));
	cls.lpfnWndProc = window_proc;
	cls.hInstance = GetModuleHandle(NULL);
	cls.lpszClassName = class_name;
	RegisterClassA(&cls);

	RECT r = {
		0, 0,
		width, height
	};

	AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);

	out_window->internal = CreateWindowExA(0, class_name, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, NULL, NULL);
	if (out_window->internal == NULL) {
		kgfw_log(KGFW_LOG_SEVERITY_ERROR, "GLFW window creation failed");
		return 1;
	}

	ShowWindow(out_window->internal, SW_SHOW);
	
	if (kgfw_input_register_window(out_window) != 0) {
		DestroyWindow(out_window->internal);
		UnregisterClassA("krisvers' window class", GetModuleHandle(NULL));
		return 2;
	}

	return 0;
}

void kgfw_window_destroy(kgfw_window_t * window) {
	DestroyWindow(window->internal);
	UnregisterClassA("krisvers' window class", GetModuleHandle(NULL));
}

int kgfw_window_update(kgfw_window_t * window) {
	if (!IsWindow(window->internal)) {
		window->closed = 1;
		return 1;
	}

	MSG msg;
	if (PeekMessageA(&msg, window->internal, 0, 0, PM_REMOVE) > 0) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	return 0;
}

#define WIN_KEY_TO_KGFW(w, k) if (key == VK_##w) return KGFW_KEY_##k;
#define WIN_KEY_TO_KGFW_SAME(k) if (key == VK_##k) return KGFW_KEY_##k;

static kgfw_input_key_enum win_key_to_kgfw(int key) {
	WIN_KEY_TO_KGFW_SAME(ESCAPE);
	WIN_KEY_TO_KGFW(CAPITAL, CAPS_LOCK);
	WIN_KEY_TO_KGFW_SAME(LEFT);
	WIN_KEY_TO_KGFW_SAME(RIGHT);
	WIN_KEY_TO_KGFW_SAME(DOWN);
	WIN_KEY_TO_KGFW_SAME(UP);
	WIN_KEY_TO_KGFW(OEM_3, GRAVE);
	WIN_KEY_TO_KGFW(BACK, BACKSPACE);
	WIN_KEY_TO_KGFW(RETURN, ENTER);

	return key;
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
	switch (msg) {
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC dc = BeginPaint(hwnd, &ps);
				EndPaint(hwnd, &ps);
			}
			return 0;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			kgfw_input_press_key_down(win_key_to_kgfw(w));
			return 0;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			kgfw_input_press_key_up(win_key_to_kgfw(w));
			return 0;
		case WM_LBUTTONDOWN:
			kgfw_input_press_mouse_button_down(KGFW_MOUSE_LBUTTON);
			return 0;
		case WM_RBUTTONDOWN:
			kgfw_input_press_mouse_button_down(KGFW_MOUSE_RBUTTON);
			return 0;
		case WM_LBUTTONUP:
			kgfw_input_press_mouse_button_up(KGFW_MOUSE_LBUTTON);
			return 0;
		case WM_RBUTTONUP:
			kgfw_input_press_mouse_button_up(KGFW_MOUSE_RBUTTON);
			return 0;
		case WM_MOUSEWHEEL:
			{
				float dx, dy;
				kgfw_input_mouse_scroll(&dx, &dy);
				kgfw_input_set_mouse_scroll(dx, (GET_WHEEL_DELTA_WPARAM(w)));
			}
			return 0;
		case WM_MOUSEHWHEEL:
			{
				float dx, dy;
				kgfw_input_mouse_scroll(&dx, &dy);
				kgfw_input_set_mouse_scroll((GET_WHEEL_DELTA_WPARAM(w)), dy);
			}
			return 0;
		case WM_MOUSEMOVE:
			kgfw_input_set_mouse_pos((float) GET_X_LPARAM(l), (float) GET_Y_LPARAM(l));
			return 0;
	}

	return DefWindowProcA(hwnd, msg, w, l);
}
#endif
