#include "tcp.h"
#include "crc32.h"

#include <Windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <stdint.h>
#include <thread>
#include <mutex>
#include <future>
#include <fstream>
#include <string>
#include <optional>
#include <memory>
#include <filesystem>
#include <vector>
#include <unordered_map>

#pragma comment(lib,"Comctl32.lib")

constexpr UINT WM_RESULT = WM_USER + 1;
constexpr UINT WM_SERVER_MESSAGE = WM_USER + 2;

struct Result {
    std::wstring path;
    std::wstring crc;
    std::wstring size;
    std::wstring error;
};

struct Program {
    Program(int argc, wchar_t** argv, std::unique_ptr<TcpServer> server) : server(std::move(server)) {
        instance = this;

        // Create main window
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpszClassName = L"MainWindow";
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpfnWndProc = (decltype(wc.lpfnWndProc))[](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            return Program::instance->WndProc(hwnd, msg, wParam, lParam);
        };
        RegisterClassExW(&wc);
        window = CreateWindowExW(
            WS_EX_ACCEPTFILES | WS_EX_TOPMOST,
            wc.lpszClassName,
            L"Equals",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_EX_TOPMOST,
            CW_USEDEFAULT, CW_USEDEFAULT,
            500, 200,
            NULL,
            NULL,
            NULL,
            NULL);

        // Create list view
        INITCOMMONCONTROLSEX icex{};
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);
        listView = CreateWindowExW(
            NULL,
            WC_LISTVIEW,
            L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_EDITLABELS | LVS_SORTASCENDING,
            0, 0,
            100, 100,
            window,
            NULL,
            NULL,
            NULL);
        AddListViewColumn(listView, 0, 400, L"Path", LVCFMT_LEFT);
        AddListViewColumn(listView, 1, 100, L"CRC32", LVCFMT_RIGHT);
        AddListViewColumn(listView, 2, 100, L"Size", LVCFMT_RIGHT);

        ResizeListView();

        for (int i = 1; i < argc; ++i) {
            ComputeCrc32(argv[i]);
        }

        if (this->server) {
            this->server->Run([this](auto x) { OnMessage(x); });
        }
    }

    void Run() {
        MSG msg;
        while (GetMessageW(&msg, 0, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void StoreResult(Result result) {
        StoreResult(std::make_unique<Result>(std::move(result)));
    }

    void StoreResult(std::unique_ptr<Result> result) {
        for (size_t i = 0; i < results.size(); i++) {
            if (results[i].first == result->path) {
				ListView_SetItemText(listView, i, 1, result->crc.data());
				ListView_SetItemText(listView, i, 2, result->size.data());
				results[i].second = std::move(result);
				return;
			}
		}

        int row = AddListViewItem(
            listView,
            result->path.data(),
            result->crc.data(),
            result->size.data());
        results.insert(results.begin() + row, { result->path, std::move(result) });
	}

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_RESULT: {
            std::unique_ptr<Result> result((Result*)wParam);

            if (!result->error.empty()) {
                std::wstring message = result->path + L"\n" + result->error;
				MessageBoxW(window, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
				break;
			}

            StoreResult(std::move(result));
            break;
        }
        case WM_SERVER_MESSAGE: {
			std::wstring* path = (std::wstring*)wParam;
			ComputeCrc32(*path);
			delete path;
			break;
		}
        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
			int count = DragQueryFileW(hDrop, -1, NULL, 0);
            for (int i = 0; i < count; ++i) {
				int length = DragQueryFileW(hDrop, i, NULL, 0);
				std::wstring path(length, 0);
				DragQueryFileW(hDrop, i, path.data(), length + 1);
				ComputeCrc32(path);
			}
			DragFinish(hDrop);
			break;
        }
        case WM_SIZE:
            ResizeListView();
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        return 0;
    }

    void OnMessage(std::vector<uint8_t> message) {
        std::wstring* path = new std::wstring((wchar_t*)message.data(), message.size() / sizeof(wchar_t));
        PostMessageW(window, WM_SERVER_MESSAGE, (WPARAM)path, 0);
    }

    void ResizeListView() {
        RECT rcClient;
        GetClientRect(window, &rcClient);
        int width = rcClient.right - rcClient.left;
        int height = rcClient.bottom - rcClient.top;

        SetWindowPos(listView, NULL, 0, 0, width, height, SWP_NOZORDER);

        ListView_SetColumnWidth(listView, 0, width - 200);
        ListView_SetColumnWidth(listView, 1, 100);
        ListView_SetColumnWidth(listView, 2, 100);
    }

    void AddListViewColumn(HWND hwnd, int col, int width, wchar_t* text, int fmt) {
        LVCOLUMNW lvc{};
        lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        lvc.iSubItem = col;
        lvc.pszText = (wchar_t*)text;
        lvc.cx = width;
        lvc.fmt = fmt;
        ListView_InsertColumn(hwnd, col, &lvc);
    }

    int AddListViewItem(HWND hwnd, wchar_t* path, wchar_t* crc, wchar_t* size) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = 0;
        item.pszText = path;
        item.iSubItem = 0;
        int row = ListView_InsertItem(hwnd, &item);

        ListView_SetItemText(hwnd, row, 1, crc);
        ListView_SetItemText(hwnd, row, 2, size);
        return row;
    }

    void PostResult(Result result) {
        PostMessageW(window, WM_RESULT, (WPARAM)new Result(result), NULL);
    }

    void ComputeCrc32(const std::wstring& path) {
        Result result{};
        result.path = path;

        std::error_code ec{};
        std::wstring canonPath = std::filesystem::canonical(path, ec).wstring();
        if (!ec) {
            result.path = std::move(canonPath);
        }
        NormalizePath(result.path);

        if (std::find_if(results.begin(), results.end(), [&result](auto&& x) { return x.first == result.path; }) != results.end()) {
            return;
        }

        std::thread([this, result]() mutable {
            std::ifstream file(result.path, std::ios::binary | std::ios::ate);
            if (!file) {
                result.error = L"Failed to open file";
                PostResult(std::move(result));
                return;
            }

            uint64_t size = (uint64_t)file.tellg();
            result.size = ToString(size);
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> buffer(1024 * 1024);
            uint32_t crc = 0;
            uint64_t totalRead = 0;
            float progress = 0;
            while (file) {
                file.read((char*)buffer.data(), buffer.size());
                if (file.bad()) {
                    result.error = L"Failed to read file";
                    PostResult(std::move(result));
                    return;
                }
                size_t read = (size_t)file.gcount();
                
                float newProgress = (float)totalRead / size;
                if (newProgress - progress > 0.01f) {
					progress = newProgress;
                    result.crc = Progress((float)totalRead / size);
                    StoreResult(result);
				}

                totalRead += read;
                crc = crc32_fast(buffer.data(), read, crc);
            }

            result.crc = Hex(crc);
            PostResult(std::move(result));
        }).detach();
    }

    static void NormalizePath(std::wstring& path) {
		std::replace(path.begin(), path.end(), L'\\', L'/');
	}

    static std::wstring Progress(float value) {
        return ToString((uint64_t)(value * 100)) + L'%';
	}

    static std::wstring Hex(uint32_t value) {
        std::wstring result;
        for (int i = 0; i < sizeof(uint32_t) * 2; ++i) {
            int digit = (value >> (i * 4)) & 0xF;
            result = L"0123456789ABCDEF"[digit] + result;
        }
        return result;
    }

    static std::wstring ToString(uint64_t value) {
        std::wstring result;
        uint64_t n = value;
        int i = 0;
        do {
            if (i && i % 3 == 0) {
                result = L' ' + result;
            }
            result = L"0123456789"[n % 10] + result;
            n /= 10;
            i++;
        } while (n);
        return result;
    }

    static Program* instance;
    HWND window;
    HWND listView;
    std::unique_ptr<TcpServer> server;
    std::vector<std::pair<std::wstring, std::unique_ptr<Result>>> results;
};

Program* Program::instance = nullptr;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow) {
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::unique_ptr<TcpServer> server;
    if (InitNetwork()) {
        try {
            server = std::make_unique<TcpServer>();
        } catch (NetworkException&) {
            if (argc > 1 && SendArgv(argc, argv)) {
                LocalFree(argv);
                return 0;
            }
        }
    }

    Program(argc, argv, std::move(server)).Run();
    LocalFree(argv);
    return 0;
}
