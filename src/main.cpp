#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, int) {
  MessageBox(nullptr, u8"😊", u8"😃", MB_OK | MB_SETFOREGROUND);
}
