#include <windows.h>
#include <fstream>
#include <vector>
#include <string>
#include <winuser.h>
#include "resources.h"

using namespace std;

/**************** functions prototypes ****************/
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK OpenDlgProc(HWND, UINT, WPARAM, LPARAM);
int onCreate(HWND hWnd, LPARAM lParam, TEXTMETRIC &tm, string &fileData);
int onPaint(HWND hWnd, TEXTMETRIC &tm, int width, int height, string &fileData);
void GetFileData(LPCSTR szFileName, string &fileName);
void ParseData(vector<string> &parsedData, TEXTMETRIC &tm, string &fileName);
void validateYBias(HWND hWnd, int scrollRange);

/**************** global variables ****************/
POINT dp = {0, 0}; // bias caused by the scroll
POINT clientAreaSize;
int linesAmount = 0;   // current amount of lines that program has to show

char *defaultFileName;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nCmdShow)
{
  HWND hwnd;
  MSG messages;
  WNDCLASSEX wincl;
  const char szClassName[] = "SEE_ME_IN_"; // program name
  const char szAppName[] = "NoteBad"; // what you will see on the top of the window
  const int defaultHeight = 600;
  const int defaultWidth = 800;
  defaultFileName = lpszArgument;

  wincl.hInstance = hInstance;
  wincl.lpszClassName = szClassName;
  wincl.lpfnWndProc = WindowProcedure;
  wincl.style = CS_DBLCLKS;
  wincl.cbSize = sizeof (WNDCLASSEX);

  wincl.hIcon = LoadIcon (NULL, IDI_APPLICATION);
  wincl.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
  wincl.hCursor = LoadCursor (NULL, IDC_ARROW);
  wincl.lpszMenuName = "NPMenu";
  wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
  wincl.cbWndExtra = 0;                      /* structure or the window instance */
  wincl.hbrBackground = WHITE_BRUSH;

  if (!RegisterClassEx(&wincl))
    return 0;

  hwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           szClassName,         /* Classname */
           szAppName,       /* Title Text */
           WS_OVERLAPPEDWINDOW | WS_VSCROLL, /* default window */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           defaultWidth,
           defaultHeight,
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           NULL,
           hInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

  ShowWindow (hwnd, nCmdShow);
  while (GetMessage (&messages, NULL, 0, 0))
  {
    TranslateMessage(&messages);
    DispatchMessage(&messages);
  }
  return messages.wParam;
}

LRESULT CALLBACK WindowProcedure (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  const int bits_in_byte = 8;   // number of bits in one byte

  static int scrollRange = 1;
  static const int maxStringLength = 255; // maximal length of file name
  static HINSTANCE hInstance;

  RECT windowRect;
  GetWindowRect(hWnd, &windowRect);
  static int width = windowRect.right - windowRect.left;
  static int height = windowRect.bottom - windowRect.top;

  static char szFile[maxStringLength];       // buffer for file name
  char szFileTitle[maxStringLength];
  OPENFILENAMEA ofn;       // common dialog box structure

  static char aboutMessage[maxStringLength];
  static TEXTMETRIC textMetrics;

  static string fileData;


  switch (message)
  {
  case WM_CREATE:
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = szFileTitle;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    hInstance = ((LPCREATESTRUCT)(lParam))->hInstance;
    onCreate(hWnd, lParam, textMetrics, fileData);
    LoadString(hInstance, IDS_ABOUT, aboutMessage, maxStringLength - 1);
    EnableScrollBar(hWnd, ESB_DISABLE_BOTH, ESB_DISABLE_BOTH);
    break;

  case WM_DESTROY:
    PostQuitMessage (0);
    break;

  case WM_SIZE:
    {
      POINT oldCoord(clientAreaSize);

      clientAreaSize.x = LOWORD(lParam);
      clientAreaSize.y = HIWORD(lParam);

      RECT tempRect = {0, 0, clientAreaSize.x, clientAreaSize.y};
      InvalidateRect(hWnd, &tempRect, true);
      scrollRange = linesAmount - clientAreaSize.y / textMetrics.tmHeight;

      // updating information about window size
      GetWindowRect(hWnd, &windowRect);
      width = windowRect.right - windowRect.left;
      height = windowRect.bottom - windowRect.top;

      UpdateWindow(hWnd);
    }
    break;

  case WM_VSCROLL:
  {
    switch(LOWORD(wParam))
    {
      case SB_LINEUP :
        dp.y -= 1;
        break;

      case SB_LINEDOWN :
        dp.y += 1;
        break;

      case SB_PAGEUP :
        dp.y -= clientAreaSize.y / textMetrics.tmHeight;
        break;

      case SB_PAGEDOWN :
        dp.y += clientAreaSize.y / textMetrics.tmHeight;
        break;

      case SB_THUMBPOSITION :
      case SB_THUMBTRACK :
        {
          SCROLLINFO si;

          RtlZeroMemory(&si, sizeof(SCROLLINFO));
          si.cbSize = sizeof(si);
          si.fMask = SIF_TRACKPOS;
          GetScrollInfo(hWnd, SB_VERT, &si);
          dp.y = si.nTrackPos;
        }
        break;

      default :
        break;
      }
      validateYBias(hWnd, scrollRange);
    return 0;
  }
  break;

  case WM_MOUSEWHEEL:
    {
      signed short dy = static_cast<signed short>(wParam >> sizeof(WORD) * bits_in_byte) / WHEEL_DELTA;
      dp.y -= dy;
      validateYBias(hWnd, scrollRange);
    }
    break;

  case WM_KEYDOWN:
    switch (wParam)
    {
      case VK_DOWN:
        SendMessage(hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
        break;
      case VK_UP:
        SendMessage(hWnd, WM_VSCROLL, SB_LINEUP, 0);
        break;
    }
    break;

  case WM_PAINT:
    onPaint(hWnd, textMetrics, width, height, fileData);
    scrollRange = linesAmount - clientAreaSize.y / textMetrics.tmHeight;
    if (scrollRange < 0)
      scrollRange = 0;
    break;

  case WM_COMMAND:
    switch (LOWORD(wParam))
    {
    case IDM_EXIT :
      PostQuitMessage(0);
      return 0;

    case IDM_ABOUT:
      MessageBoxA(hWnd, aboutMessage, "About", MB_OK);
      break;

    case IDM_OPEN:
      if (GetOpenFileNameA(&ofn) == TRUE)
      {
        GetFileData(ofn.lpstrFile, fileData);
      }
      InvalidateRect(hWnd, NULL, TRUE);
      break;
    }
    break;

  default:
    return DefWindowProc (hWnd, message, wParam, lParam);
  }

  return 0;
}

int onPaint(HWND hWnd, TEXTMETRIC &textMetrics, int width, int height, string &fileData)
{
  static const int leftSymbolCoord = 10;// distance from left border of the window to first symbol
  PAINTSTRUCT ps;
  HDC hDC = BeginPaint(hWnd, &ps);
  static vector<string> parsedData;

  HDC hdcMem = CreateCompatibleDC(hDC);
  HBITMAP hbmMem = CreateCompatibleBitmap(hDC, width, height);
  HANDLE hOld = SelectObject(hdcMem, hbmMem);

  int maxVisibleLines = clientAreaSize.y / textMetrics.tmHeight;

  if (ps.fErase == true)
  {
    FillRect(hdcMem, &(ps.rcPaint), WHITE_BRUSH);
    ParseData(parsedData, textMetrics, fileData);

    SetScrollRange(hWnd, SB_VERT, 0, max(1, linesAmount - maxVisibleLines), true);
    if (linesAmount > maxVisibleLines)
      EnableScrollBar(hWnd, SB_VERT, ESB_ENABLE_BOTH);
    else
      EnableScrollBar(hWnd, SB_VERT, ESB_DISABLE_BOTH);
  }

  int maxLines = min(static_cast<LONG>(parsedData.size()), dp.y + maxVisibleLines);

  for (int i = dp.y; i < maxLines; i++)
    TextOut(hdcMem, leftSymbolCoord, (i - dp.y) * textMetrics.tmHeight, parsedData[i].c_str(), parsedData[i].length());

  BitBlt(hDC, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);
  SelectObject(hdcMem, hOld);
  DeleteObject(hbmMem);
  DeleteDC    (hdcMem);

  EndPaint(hWnd, &ps);
  return 0;
}

int onCreate(HWND hWnd, LPARAM lParam, TEXTMETRIC &textMetrics, string &fileData)
{
  HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hWnd, GWL_HINSTANCE);

  CREATESTRUCT* lpcs = (CREATESTRUCT*)(lParam);
  GetFileData(defaultFileName, fileData);

  HDC hDC = GetDC(hWnd);
  GetTextMetrics(hDC, &textMetrics);
  ReleaseDC(hWnd, hDC);
  return 0;
}

void GetFileData(const char* szFileName, string &fileData)
{
  ifstream input(szFileName);
  if (!input)
  {
    string shout = "File not found: ";
    shout.append(szFileName);
    MessageBoxA(NULL, shout.c_str(), "Error", MB_OK);
    return;
  }

  string str;
  fileData.clear();
  while (getline(input, str))
  {
    str += '\n';
    fileData.append(str);
  }

  input.close();
}

void ParseData(vector<string> &parsedData, TEXTMETRIC &textMetrics, string &fileData)
{
  int symbolsProcessed = 0;
  int lineWidth = clientAreaSize.x / textMetrics.tmAveCharWidth;

  parsedData.clear();
  linesAmount = 0;

  while (symbolsProcessed < fileData.length())
  {
    int symbolsToPrint = static_cast<int>(fileData.length()) - symbolsProcessed;
    int lastSymbol = -1;
    bool isCarriage = false;
    int maxLastSymbol = lineWidth;

    if (symbolsToPrint < lineWidth)
      maxLastSymbol = symbolsToPrint;

    for (int j = 0; j < maxLastSymbol; j++)
    {
      if (fileData[symbolsProcessed + j] == '\n')
      {
        lastSymbol = j;
        isCarriage = true;
        break;
      }
      if (isspace(fileData[symbolsProcessed + j]))
        lastSymbol = j;
    }

    if (lastSymbol == -1) // very long string. just throw it out
      lastSymbol = lineWidth;
    parsedData.push_back(fileData.substr(symbolsProcessed, lastSymbol));
    linesAmount++;
    symbolsProcessed += (lastSymbol + 1);
  }
}

void validateYBias(HWND hWnd, int scrollRange)
{
      dp.y = max(static_cast<LONG>(0), dp.y);
      dp.y = min(static_cast<LONG>(scrollRange), dp.y);
      if (dp.y != GetScrollPos(hWnd, SB_VERT))
      {
        SetScrollPos(hWnd, SB_VERT, dp.y, TRUE);
        InvalidateRect(hWnd, NULL, TRUE);
        UpdateWindow(hWnd);
      }
}
