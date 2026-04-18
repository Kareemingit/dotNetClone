using FrameworkCore.Http;
using System.Net.Sockets;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;


//[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
//public struct MyData
//{
//    public int Id;
//    public float Value;
//    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
//    public string Message;
//}
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
        Request request = new Request(reqNative, rawDataPtr);
        Response response = new Response(reqNative.ClientSocket);
        HttpContext context = new HttpContext(request, response);
        // threw context to the Middleware pipeline
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