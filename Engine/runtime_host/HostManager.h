#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "nethost.h"
#include "hostfxr.h"
#include "coreclr_delegates.h"
#include "..\GeneralEntities\HttpContext.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define STR(s) L##s
#else
#include <dlfcn.h>
#define STR(s) s
#endif
#ifndef UNMANAGEDCALLERSONLY_METHOD
#define UNMANAGEDCALLERSONLY_METHOD ((const char_t*)-1)
#endif

typedef void* (*handle_request_fn)(void*);


class HostManager {
private:
    hostfxr_initialize_for_runtime_config_fn init_fptr;
    hostfxr_get_runtime_delegate_fn get_delegate_fptr;
    hostfxr_close_fn close_fptr;
    load_assembly_and_get_function_pointer_fn load_assembly;
    hostfxr_handle cxt = nullptr;
    handle_request_fn request_handler = nullptr;

    std::wstring get_executable_directory() {
#ifdef _WIN32
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(NULL, buffer, MAX_PATH);
        std::wstring path(buffer);
        return path.substr(0, path.find_last_of(L"\\/"));
#else
        char buffer[PATH_MAX];
        readlink("/proc/self/exe", buffer, PATH_MAX);
        std::string path(buffer);
        return path.substr(0, path.find_last_of("/"));
#endif
    }

    void* load_library(const char_t* path)
    {
#ifdef _WIN32
        return LoadLibraryW(path);
#else
        return dlopen(path, RTLD_LAZY);
#endif
    }

    void* get_export(void* lib, const char* name)
    {
#ifdef _WIN32
        return GetProcAddress((HMODULE)lib, name);
#else
        return dlsym(lib, name);
#endif
    }

public:
    HostManager() {
        // 1. Dynamically build paths relative to the EXE location
        std::wstring base_path = get_executable_directory();
        std::wstring config_path = base_path + L"\\FrameworkCore.runtimeconfig.json";
        std::wstring dll_path = base_path + L"\\FrameworkCore.dll";

        // 2. Locate hostfxr using nethost
        char_t hostfxr_path[MAX_PATH];
        size_t buffer_size = sizeof(hostfxr_path) / sizeof(char_t);
        int rc = get_hostfxr_path(hostfxr_path, &buffer_size, nullptr);
        if (rc != 0) {
            std::cerr << "Error: Could not find .NET Runtime (hostfxr)." << std::endl;
            return;
        }

        // 3. Load hostfxr and get function pointers
        void* lib = load_library(hostfxr_path);
        init_fptr = (hostfxr_initialize_for_runtime_config_fn)get_export(lib, "hostfxr_initialize_for_runtime_config");
        get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)get_export(lib, "hostfxr_get_runtime_delegate");
        close_fptr = (hostfxr_close_fn)get_export(lib, "hostfxr_close");

        // 4. Initialize the runtime context

        rc = init_fptr(config_path.c_str(), nullptr, &cxt);
        if (rc != 0 || cxt == nullptr) {
            std::wcout << L"Error: Failed to init runtime with config: " << config_path << L" (Error code: 0x" << std::hex << rc << L")" << std::endl;
            return;
        }

        // 5. Get the assembly loader delegate
        rc = get_delegate_fptr(cxt, hdt_load_assembly_and_get_function_pointer, (void**)&load_assembly);

        // 6. Load the C# method (The "Bridge")

        rc = load_assembly(
            dll_path.c_str(),
            STR("InternalBootstrap, FrameworkCore"), // Namespace.Class, AssemblyName
            STR("HandleRequest"),                                                 // Method
            ((const char_t*)-1),                                              // UnmanagedCallersOnly signifier
            nullptr,
            (void**)&request_handler);

        if (rc != 0 || request_handler == nullptr) {
            std::cerr << "Error: Handshake failed. Ensure the C# DLL is in the same folder as the EXE." << std::endl;
            close_fptr(cxt);
            return;
        }

        std::cout << "--- Framework Host Manager Started ---" << std::endl;
    }

    char* send(http_request* data) {
        if (!data) return nullptr;
        // Pass the pointer directly to C#
        return (char*)request_handler(data);
    }

    ~HostManager() {
        close_fptr(cxt);
    }
};
