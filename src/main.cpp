#include <windows.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, PSTR cmd, int show) {
  ::MessageBoxA(nullptr, u8"\r\nUnicode: \"😃\"", u8"Unicode: \"😊\"", MB_OK | MB_SETFOREGROUND);
}
