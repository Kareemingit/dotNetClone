using FrameworkCore.Http;
using FrameworkCore.Http.Native;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

public static unsafe class InternalBootstrap
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ResponseCallbackDelegate(int clientSocket, IntPtr responseData);

    private static ResponseCallbackDelegate _cppResponseCallback;

    [UnmanagedCallersOnly(EntryPoint = "RegisterResponseCallback", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void RegisterResponseCallback(IntPtr callbackPtr)
    {
        _cppResponseCallback = Marshal.GetDelegateForFunctionPointer<ResponseCallbackDelegate>(callbackPtr);
        Console.WriteLine("[Framework] Callback registered successfully.");
    }

    [UnmanagedCallersOnly(EntryPoint = "HandleRequest", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void HandleRequest(IntPtr dataPtr , IntPtr rawDataPtr)
    {
        NativeHttpRequest reqNative = Marshal.PtrToStructure<NativeHttpRequest>(dataPtr);

        string responseStr = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK";
        ThrewResponseToCpp((int)reqNative.ClientSocket, responseStr);
    }

    public static void ThrewResponseToCpp(int clientSocket, string responseStr)
    {
        if (_cppResponseCallback != null)
        {
            IntPtr nativeString = Marshal.StringToHGlobalAnsi(responseStr);
            _cppResponseCallback(clientSocket, nativeString);
        }
    }
}