#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <winhttp.h>
#include <psapi.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include "core.hpp"
#include "telemetry.hpp"
#include "embedded.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
using Microsoft::WRL::ComPtr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
HWND control{},overlay{},game{},manualGame{};DWORD manualPid=0;bool running=true;bool smokeTest=false,recoveryTest=false;int smokeFrames=0;float overlayScale=1;wt::Settings settings;wt::Database database;std::filesystem::path settingsPath,databasePath;std::string error;bool settingsWritable=true;NOTIFYICONDATAW tray{};bool trayAvailable=false;UINT taskbarCreated=0;
ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> swapchain;ComPtr<ID3D11RenderTargetView> target;
wt::TelemetryChannel telemetryChannel;
std::atomic<int> interval{200};
struct InstanceGuard {HANDLE mutex{};~InstanceGuard(){if(mutex)CloseHandle(mutex);}};
struct TrayGuard {~TrayGuard(){if(tray.hWnd)Shell_NotifyIconW(NIM_DELETE,&tray);}};
void saveSettings(){if(settingsWritable)settings.save(settingsPath);}
void loadDatabase(){try{database.load(databasePath);}catch(const std::exception&e){database.loadJson(wt::json::parse(embeddedDatabase));error=std::string("Database could not be loaded; using bundled SIMULATED profiles. ")+e.what();}}
std::wstring wide(const std::string& text){if(text.empty())return {};int count=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);std::wstring result(count,0);MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),result.data(),count);return result;}
struct Internet {
 HINTERNET h{};
 explicit Internet(HINTERNET value):h(value){}
 Internet(const Internet&)=delete;Internet& operator=(const Internet&)=delete;
 ~Internet(){if(h)WinHttpCloseHandle(h);}operator HINTERNET()const{return h;}
};
std::string httpError(const char* operation){return std::string(operation)+" (Windows error "+std::to_string(GetLastError())+")";}
std::string request(const wchar_t* path){
 Internet session{WinHttpOpen(L"WTArmourVision/0.4",WINHTTP_ACCESS_TYPE_NO_PROXY,nullptr,nullptr,0)};
 if(!session.h)throw std::runtime_error(httpError("HTTP initialization"));
 WinHttpSetTimeouts(session,500,500,500,500);
 Internet connection{WinHttpConnect(session,L"127.0.0.1",8111,0)};
 if(!connection.h)throw std::runtime_error(httpError("HTTP connection"));
 Internet req{WinHttpOpenRequest(connection,L"GET",path,nullptr,nullptr,nullptr,0)};
 if(!req.h)throw std::runtime_error(httpError("HTTP request"));
 DWORD redirectPolicy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
 WinHttpSetOption(req,WINHTTP_OPTION_REDIRECT_POLICY,&redirectPolicy,sizeof redirectPolicy);
 if(!WinHttpSendRequest(req,nullptr,0,nullptr,0,0,0)||!WinHttpReceiveResponse(req,nullptr))throw std::runtime_error(httpError("Port 8111 unavailable"));
 DWORD code=0,size=sizeof code;
 if(!WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&code,&size,nullptr))throw std::runtime_error(httpError("HTTP status"));
 if(code!=200)throw std::runtime_error("HTTP "+std::to_string(code));
 std::string body;char buffer[4096];DWORD n=0;
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 for(;;){
  if(!WinHttpReadData(req,buffer,sizeof buffer,&n))throw std::runtime_error(httpError("HTTP body read"));
  if(!n)break;
  body.append(buffer,n);
  if(body.size()>262144)throw std::runtime_error("Response exceeds 256 KiB");
  if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Response deadline exceeded");
 }
 return body;
}
wt::Endpoint pollEndpoint(const wchar_t* path){try{return wt::parseEndpoint(request(path));}catch(const std::exception&e){wt::Endpoint result;result.status=e.what();return result;}}
void telemetryWorker(std::stop_token stop){while(!stop.stop_requested()){
 auto request=telemetryChannel.request();auto next=telemetryChannel.read();
 if(request.simulated){next.state=wt::parseEndpoint(R"JSON({"valid":true,"demo speed (fixture units)":24})JSON");if(!telemetryChannel.publish(request,next))continue;}
 else {
  next.indicators=pollEndpoint(L"/indicators");if(stop.stop_requested())break;if(!telemetryChannel.publish(request,next))continue;
  next.state=pollEndpoint(L"/state");if(stop.stop_requested())break;if(!telemetryChannel.publish(request,next))continue;
  next.map=pollEndpoint(L"/map_info.json");if(stop.stop_requested())break;if(!telemetryChannel.publish(request,next))continue;
 }
 int delay=(!request.simulated&&!next.connected())?1500:interval.load();
 for(int elapsed=0;elapsed<delay&&!stop.stop_requested()&&telemetryChannel.request().generation==request.generation;elapsed+=25)std::this_thread::sleep_for(std::chrono::milliseconds(25));
 }}
std::string utf8(const std::wstring& value){if(value.empty())return {};int size=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);std::string text(size,0);WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),text.data(),size,nullptr,nullptr);return text;}
struct WindowChoice {HWND hwnd;DWORD pid;std::string title;};
std::vector<WindowChoice> choices;
BOOL CALLBACK listWindows(HWND h,LPARAM){
 if(h==control||h==overlay||!IsWindowVisible(h)||GetWindow(h,GW_OWNER))return TRUE;
 wchar_t title[512];if(!GetWindowTextW(h,title,512))return TRUE;
 DWORD pid=0;GetWindowThreadProcessId(h,&pid);choices.push_back({h,pid,utf8(title)});return TRUE;
}
void exportDiagnostics(){try{
 auto current=telemetryChannel.read();
 wt::json report={{"version","0.4.1"},{"demo",settings.demo},{"game_window_detected",game!=nullptr},{"manual_window",manualGame!=nullptr},{"overlay_enabled",settings.overlay},{"overlay_visible",IsWindowVisible(overlay)!=FALSE},{"telemetry_summary",current.summary()},{"reported_vehicle",current.vehicle()}};
 auto endpoint=[](const wt::Endpoint&e){wt::json j={{"received",e.received},{"status",e.status},{"fields",e.fields},{"army",e.army},{"vehicle_id",e.vehicleId}};return j;};
 report["manual_ammunition"]={{"name",settings.manualShell},{"type",settings.manualShellType},{"reported_penetration_mm",settings.manualPenetration},{"source","user supplied; not automatically detected"},{"conditions","distance and impact angle unknown"}};report["state"]=endpoint(current.state);report["indicators"]=endpoint(current.indicators);report["map_info"]=endpoint(current.map);
 auto path=settingsPath.parent_path()/L"compatibility-report.json";std::ofstream f(path);f<<report.dump(2);if(!f){error="Cannot save compatibility report";return;}f.close();if(!f){error="Compatibility report write failed";return;}ShellExecuteW(control,L"open",settingsPath.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
 }catch(const std::exception&e){error=std::string("Report export failed: ")+e.what();}
}
BOOL CALLBACK findGame(HWND hwnd,LPARAM){if(!IsWindowVisible(hwnd)||hwnd==control||hwnd==overlay)return TRUE;DWORD pid;GetWindowThreadProcessId(hwnd,&pid);HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return TRUE;wchar_t path[32768];DWORD size=32768;bool match=false;if(QueryFullProcessImageNameW(process,0,path,&size)){std::wstring name=std::filesystem::path(path).filename().wstring();match=_wcsicmp(name.c_str(),L"aces.exe")==0;}CloseHandle(process);if(match){game=hwnd;return FALSE;}return TRUE;}
void createTarget(){ComPtr<ID3D11Texture2D> buffer;if(FAILED(swapchain->GetBuffer(0,IID_PPV_ARGS(&buffer)))||FAILED(device->CreateRenderTargetView(buffer.Get(),nullptr,&target)))throw std::runtime_error("Render target creation failed");}
LRESULT CALLBACK overlayProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_NCHITTEST)return HTTRANSPARENT;if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(m==WM_PAINT){PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);FillRect(dc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(57,169,255));auto font=CreateFontW(int(18*overlayScale),0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto old=SelectObject(dc,font);std::string t=telemetryChannel.read().summary();std::string text="WT ARMOUR VISION\n"+t+"\nManual shell: "+settings.manualShell+" / "+settings.manualShellType+"\nReported penetration: "+std::to_string(int(settings.manualPenetration))+" mm (distance/angle unknown)";r.left=12;r.top=12;auto displayText=wide(text);DrawTextW(dc,displayText.c_str(),int(displayText.size()),&r,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,old);DeleteObject(font);EndPaint(h,&ps);return 0;}return DefWindowProcW(h,m,w,l);}
LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l){if(taskbarCreated&&m==taskbarCreated){trayAvailable=Shell_NotifyIconW(NIM_ADD,&tray)!=FALSE;if(!trayAvailable)ShowWindow(h,SW_RESTORE);return 0;}if(ImGui_ImplWin32_WndProcHandler(h,m,w,l))return 1;switch(m){case WM_SIZE:if(swapchain&&w!=SIZE_MINIMIZED){context->OMSetRenderTargets(0,nullptr,nullptr);target.Reset();context->Flush();if(SUCCEEDED(swapchain->ResizeBuffers(0,LOWORD(l),HIWORD(l),DXGI_FORMAT_UNKNOWN,0))){try{createTarget();}catch(...){running=false;}}else running=false;}return 0;case WM_GETMINMAXINFO:{auto info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={980,700};return 0;}case WM_DPICHANGED:{auto r=(RECT*)l;SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}case WM_CLOSE:running=false;return 0;case WM_APP+1:if(l==WM_LBUTTONDBLCLK){ShowWindow(h,SW_RESTORE);SetForegroundWindow(h);}if(l==WM_RBUTTONUP)running=false;return 0;case WM_DESTROY:PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
void updateOverlay(){static auto lastScan=std::chrono::steady_clock::time_point{};auto now=std::chrono::steady_clock::now();if(manualGame){DWORD pid=0;GetWindowThreadProcessId(manualGame,&pid);if(!IsWindow(manualGame)||pid!=manualPid){manualGame=nullptr;manualPid=0;}}if(manualGame)game=manualGame;else if(now-lastScan>std::chrono::milliseconds(500)){game=nullptr;EnumWindows(findGame,0);lastScan=now;}if(game&&!IsWindow(game))game=nullptr;HWND anchor=settings.demo?control:game;bool visible=settings.overlay&&anchor&&IsWindowVisible(anchor)&&!IsIconic(anchor)&&GetAncestor(GetForegroundWindow(),GA_ROOT)==GetAncestor(anchor,GA_ROOT);
 if(!visible){ShowWindow(overlay,SW_HIDE);return;}
 RECT r;GetClientRect(anchor,&r);POINT pt{0,0};ClientToScreen(anchor,&pt);overlayScale=settings.scale*ImGui_ImplWin32_GetDpiScaleForHwnd(anchor);int margin=int(24*overlayScale);SetWindowPos(overlay,HWND_TOPMOST,pt.x+margin,pt.y+margin,std::max(1,std::min(int(650*overlayScale),int(r.right)-margin)),std::max(1,std::min(int(185*overlayScale),int(r.bottom)-margin)),SWP_NOACTIVATE|SWP_SHOWWINDOW);SetLayeredWindowAttributes(overlay,RGB(0,0,0),BYTE(settings.opacity*255),LWA_COLORKEY|LWA_ALPHA);InvalidateRect(overlay,nullptr,FALSE);
}
void capturePreview(const char* filename="ui-preview.bmp"){
 ShowWindow(overlay,SW_HIDE);DwmFlush();
 RECT rect;GetClientRect(control,&rect);int width=rect.right,height=rect.bottom;
 HDC dc=GetDC(control),memory=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,width,height);auto old=SelectObject(memory,bitmap);
 bool captured=PrintWindow(control,memory,PW_CLIENTONLY|2)!=FALSE;if(!captured)captured=BitBlt(memory,0,0,width,height,dc,0,0,SRCCOPY|CAPTUREBLT)!=FALSE;SelectObject(memory,old);
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;std::vector<unsigned char> pixels(size_t(width)*height*4);
 if(captured&&GetDIBits(memory,bitmap,0,height,pixels.data(),&info,DIB_RGB_COLORS)){
  BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+DWORD(pixels.size());std::ofstream file(filename,std::ios::binary);file.write(reinterpret_cast<char*>(&header),sizeof header);file.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof info.bmiHeader);file.write(reinterpret_cast<char*>(pixels.data()),pixels.size());
 }
 DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(control,dc);
}
}
#include "ui.hpp"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int){try{
 smokeTest=std::wstring(commandLine).find(L"--smoke-test")!=std::wstring::npos;recoveryTest=std::wstring(commandLine).find(L"--smoke-test-recovery")!=std::wstring::npos;
 InstanceGuard instanceGuard;TrayGuard trayGuard;
 if(!smokeTest){instanceGuard.mutex=CreateMutexW(nullptr,FALSE,L"Local\\WTArmourVision-ControlCentre");if(!instanceGuard.mutex)throw std::runtime_error("Cannot create application instance guard");if(GetLastError()==ERROR_ALREADY_EXISTS){auto existing=FindWindowW(L"WTControl",nullptr);if(existing){ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);}return 0;}}
 taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
 ImGui_ImplWin32_EnableDpiAwareness();wchar_t executable[32768];GetModuleFileNameW(nullptr,executable,32768);std::filesystem::current_path(std::filesystem::path(executable).parent_path());wchar_t local[32768];auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);if(!n||n>=32768)throw std::runtime_error("LOCALAPPDATA unavailable");auto dir=smokeTest?std::filesystem::temp_directory_path()/(L"WTArmourVision-smoke-"+std::to_wstring(GetCurrentProcessId())):std::filesystem::path(local)/L"WTArmourVision";std::filesystem::create_directories(dir);settingsPath=dir/L"settings.json";databasePath=std::filesystem::path("data/database.json");if(!std::filesystem::exists(databasePath)){databasePath=dir/L"database-demo-1.json";if(!std::filesystem::exists(databasePath)){std::ofstream f(databasePath);f<<embeddedDatabase;if(!f)throw std::runtime_error("Cannot create bundled demo database");}}if(recoveryTest){std::ofstream f(settingsPath);f<<"broken settings fixture";databasePath=dir/L"broken-database.json";std::ofstream d(databasePath);d<<"broken database fixture";}try{settings.load(settingsPath);}catch(const std::exception&e){settingsWritable=false;error=std::string("Settings could not be loaded. Existing file preserved; use Reset settings to replace it. ")+e.what();}loadDatabase();if(recoveryTest&&(settingsWritable||database.plates.empty()))throw std::runtime_error("Corrupt-file recovery failed");if(smokeTest){settings.demo=true;settings.overlay=true;}
 WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=L"WTControl";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);wc.lpfnWndProc=overlayProc;wc.lpszClassName=L"WTOverlay";RegisterClassW(&wc);
 control=CreateWindowW(L"WTControl",L"WT Armour Vision",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,880,nullptr,nullptr,instance,nullptr);overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,L"WTOverlay",L"Telemetry",WS_POPUP,0,0,650,150,nullptr,nullptr,instance,nullptr);if(!control||!overlay)throw std::runtime_error("Window creation failed");
 DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=control;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 auto hr=D3D11CreateDeviceAndSwapChain(nullptr,smokeTest?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swapchain,&device,nullptr,&context);if(FAILED(hr))hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swapchain,&device,nullptr,&context);if(FAILED(hr))throw std::runtime_error("DirectX initialization failed");createTarget();
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::StyleColorsDark();setupVisuals();if(!ImGui_ImplWin32_Init(control)||!ImGui_ImplDX11_Init(device.Get(),context.Get()))throw std::runtime_error("UI backend initialization failed");
 tray.cbSize=sizeof tray;tray.hWnd=control;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=WM_APP+1;tray.hIcon=LoadIcon(nullptr,IDI_APPLICATION);wcscpy_s(tray.szTip,L"WT Armour Vision");trayAvailable=Shell_NotifyIconW(NIM_ADD,&tray)!=FALSE;if(!RegisterHotKey(control,1,MOD_ALT|MOD_NOREPEAT,VK_F8))error="Alt+F8 is already in use";
 if(smokeTest){auto flags=GetWindowLongPtrW(overlay,GWL_EXSTYLE);if((flags&(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_LAYERED))!=(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_LAYERED))throw std::runtime_error("Overlay style check failed");}
 telemetryChannel.setMode(settings.demo);ShowWindow(control,SW_SHOW);std::jthread worker(telemetryWorker);auto lastSave=std::chrono::steady_clock::now();
 while(running){auto begin=std::chrono::steady_clock::now();MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)running=false;if(msg.message==WM_HOTKEY)settings.overlay=!settings.overlay;TranslateMessage(&msg);DispatchMessageW(&msg);}if(!running)break;telemetryChannel.setMode(settings.demo);interval=settings.preset==0?1000:settings.preset==1?200:100;updateOverlay();if(IsWindowVisible(control)&&!IsIconic(control)){ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ui();ImGui::Render();float clear[]={.035f,.043f,.063f,1};auto rt=target.Get();if(rt){context->OMSetRenderTargets(1,&rt,nullptr);context->ClearRenderTargetView(rt,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());if(FAILED(swapchain->Present(1,0)))throw std::runtime_error("Graphics device lost");}}if(smokeTest){++smokeFrames;if(smokeFrames==2&&!recoveryTest)capturePreview();if(smokeFrames==7&&!recoveryTest)capturePreview("armour-preview.bmp");if(smokeFrames>=3&&smokeFrames<10)activePage=smokeFrames-3;if(smokeFrames==3)SetWindowPos(control,nullptr,0,0,900,600,SWP_NOMOVE|SWP_NOZORDER);if(smokeFrames==12)running=false;}if(begin-lastSave>std::chrono::seconds(2)){try{saveSettings();}catch(const std::exception&e){error=e.what();}lastSave=begin;}std::this_thread::sleep_until(begin+std::chrono::milliseconds(1000/settings.fps));}
 worker.request_stop();worker.join();bool saveFailed=false;try{saveSettings();}catch(const std::exception&e){error=e.what();saveFailed=true;}UnregisterHotKey(control,1);Shell_NotifyIconW(NIM_DELETE,&tray);tray.hWnd=nullptr;ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();DestroyWindow(overlay);DestroyWindow(control);if(smokeTest){if(smokeFrames<12||saveFailed)return 2;if(recoveryTest){std::ifstream f(settingsPath);std::string content((std::istreambuf_iterator<char>(f)),{});if(content!="broken settings fixture")return 3;}std::filesystem::remove_all(settingsPath.parent_path());}return 0;
 }catch(const std::exception&e){if(smokeTest){std::fprintf(stderr,"%s\n",e.what());}else MessageBoxA(nullptr,e.what(),"WT Armour Vision startup/runtime error",MB_ICONERROR);return 1;}}
