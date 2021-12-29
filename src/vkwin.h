#include "tools.h"
#define EXEC 1

#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	#include "vulkan.h"
	#include "vulkan_win32.h"
	#include "windows.h"
#else
	#define GLFW_INCLUDE_VULKAN
	#include "../ext/GLFW/glfw3.h"
#endif

typedef struct Window
{
	char *name;
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	HINSTANCE           instance;
    HWND                handle;
#else
	GLFWwindow* glfw_window; //our GLFW window!
#endif
}Window;

internal void vk_error(char *text)
{
	printf("%s\n",text);
}

f32 gettimems(void)
{
	SYSTEMTIME tm;
	GetLocalTime(&tm);
	return tm.wMilliseconds;
}
//milliseconds since program startup
internal f32 get_time(void)
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	return GetTickCount()/1000.0f;
#else
	return glfwGetTime();
#endif
}

internal char **window_get_required_instance_extensions(Window *window, u32 *ext_count)
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	char *base_extensions[] = {
		"VK_KHR_surface",
		"VK_KHR_win32_surface",
	};
	*ext_count = array_count(base_extensions);
	return base_extensions;
#else
	return glfwGetRequiredInstanceExtensions(ext_count);
#endif
}

internal void window_create_window_surface(VkInstance instance, Window *wnd,VkSurfaceKHR *surface)
{	
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	VkWin32SurfaceCreateInfoKHR surface_create_info = 
	{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,NULL,0,wnd->instance,wnd->handle};

	if (vkCreateWin32SurfaceKHR(instance, &surface_create_info, NULL, surface) != VK_SUCCESS)
		vk_error("Failed to create win32 window surface!");
#else
	if (glfwCreateWindowSurface(instance, wnd->glfw_window, NULL, surface)!=VK_SUCCESS)
        vk_error("Failed to create glfw window surface!");
#endif
}

internal void window_get_framebuffer_size(Window *wnd, u32 *width, u32 *height)
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
RECT rect;
	if(GetWindowRect(wnd->handle, &rect))
	{
	  *width = rect.right - rect.left;
	  *height = rect.bottom - rect.top;
	}
#else
	glfwGetFramebufferSize(wnd->glfw_window, width, height);
#endif
}

internal void window_set_window_title(Window *wnd, char *title)
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	SetWindowTextA(wnd->handle, title);
#else
	glfwSetWindowTitle(wnd->glfw_window, title);
#endif
}

internal void window_destroy(Window *wnd)
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	DestroyWindow(wnd->handle);
#else
	glfwDestroyWindow(wnd->glfw_window);
	glfwTerminate();
#endif
}
internal u32 window_should_close(Window *wnd)
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	return 0;
#else
	return glfwWindowShouldClose(wnd->glfw_window);
#endif
	
}

internal void window_poll_events(Window *wnd)
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	MSG message;
	b32 resize;
	b32 loop;
	if( PeekMessage( &message, NULL, 0, 0, PM_REMOVE ) ) {
          // Process events
          switch( message.message ) {
            // Resize
          case WM_USER + 1:
            resize = TRUE;
            break;
            // Close
          case WM_USER + 2:
            loop = FALSE;
            break;
          }
          TranslateMessage( &message );
          DispatchMessage( &message );
        } 
#else
	glfwPollEvents();
#endif
}

internal void window_set_resize_callback(Window *wnd, void *func)
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	
#else
	glfwSetFramebufferSizeCallback(wnd->glfw_window, func);
#endif
}
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
internal LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) {
      switch( message ) {
      case WM_SIZE:
		break;
      case WM_EXITSIZEMOVE:
        //PostMessage( hWnd, WM_USER + 1, wParam, lParam );
        break;
      case WM_KEYDOWN:
		break;
      case WM_CLOSE:
		exit(0);
        //PostMessage( hWnd, WM_USER + 2, wParam, lParam );
        break;
      default:
        return DefWindowProc( hWnd, message, wParam, lParam );
      }
      return 0;
}
#endif

#include "winuser.h"
internal u32 window_init_vulkan(Window *wnd, char *window_name, u32 window_w, u32 window_h)
{
#if defined(PLATFORM_WINDOWS) && defined(NOGLFW)
	wnd->instance = GetModuleHandle( NULL );
	
    // Register window class
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = wnd->instance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = (LPCSTR)"WIN";
    wc.hIconSm       = NULL;
    if( !RegisterClassEx( &wc ) ) {
		printf("Failed to register class\n");
		return FALSE;
    }
	
    // Create window
    wnd->handle = CreateWindowEx( WS_EX_CLIENTEDGE,(LPCSTR)"WIN",(LPCSTR)window_name, WS_OVERLAPPEDWINDOW, 
			CW_USEDEFAULT, CW_USEDEFAULT, window_w, window_h, NULL, NULL, wnd->instance, NULL );
    if( !wnd->handle ) {
		printf("Failed to create a window handle!\n");
		return FALSE;
    }
	
	ShowWindow(wnd->handle, SW_SHOWNORMAL);
	UpdateWindow(wnd->handle);
    return TRUE;
#else
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    wnd->glfw_window = glfwCreateWindow(800, 600, window_name, NULL, NULL);
	return TRUE;
#endif	
}






