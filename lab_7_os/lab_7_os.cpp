#include <atomic>
#include <iostream>
#include <cstring>
#include <windows.h>
#include <string>

#include "f.h"
#include "g.h"

struct param {
    int input;
    int(*func)(int);
    std::atomic<bool> is_done;
    int output;
};

DWORD WINAPI wrapper(LPVOID ptr) {
    auto arg = static_cast<param*>(ptr);
    arg->output = arg->func(arg->input);
    arg->is_done = true;
    std::cout << "Computing is over for one of functions, res: " << arg->output << "\n";
    return 0;
}

int main() {
    int x;
    std::cout << "Enter a value for x: ";
    std::cin >> x;

    const wchar_t* name = L"shmfile";
    size_t len = 2 * sizeof(param);

    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, len, name);
    if (hMapFile == nullptr) {
        std::cerr << "Unable to create file mapping object, error: " << GetLastError() << "\n";
        return -1;
    }

    auto params = static_cast<param*>(MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, len));
    if (params == nullptr) {
        std::cerr << "Unable to map view of file, error: " << GetLastError() << "\n";
        CloseHandle(hMapFile);
        return -1;
    }

    std::memset(params, 0, len);
    params[0].func = f;
    params[1].func = g;
    params[0].input = x;
    params[1].input = x;

    std::cout << "Starting threads, for aborting pls press Ctrl+C\n";

    HANDLE threads[2];
    threads[0] = CreateThread(nullptr, 0, wrapper, &params[0], 0, nullptr);
    threads[1] = CreateThread(nullptr, 0, wrapper, &params[1], 0, nullptr);

    std::cout << std::boolalpha;
    while (true) {
        if (params[0].is_done && params[0].output != 0) {
            WaitForSingleObject(threads[0], INFINITE);
            TerminateThread(threads[1], 0);
            CloseHandle(threads[1]);
            std::cout << "Result: " << true << "\n";
            break;
        }
        if (params[1].is_done && params[1].output != 0) {
            WaitForSingleObject(threads[1], INFINITE);
            TerminateThread(threads[0], 0);
            CloseHandle(threads[0]);
            std::cout << "Result: " << true << "\n";
            break;
        }
        if (params[0].is_done && params[1].is_done) {
            WaitForSingleObject(threads[0], INFINITE);
            WaitForSingleObject(threads[1], INFINITE);
            CloseHandle(threads[0]);
            CloseHandle(threads[1]);
            std::cout << "Result: " << (params[0].output || params[1].output) << "\n";
            break;
        }
    }

    UnmapViewOfFile(params);
        CloseHandle(hMapFile);

    return 0;
}