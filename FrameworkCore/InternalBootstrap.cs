using FrameworkCore;
using FrameworkCore.Http;
using FrameworkCore.Http.Native;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
class UserTest
{
    public int id { get; set; }
    public string name { get; set; }
    public string username { get; set; }
}
public static unsafe class InternalBootstrap
{
    private static WebApplication? _app;
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ResponseCallbackDelegate(nuint clientSocket, IntPtr responseData , int responseSize);

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
        HttpContext context = new HttpContext(reqNative, rawDataPtr);
        //Console.WriteLine(context.Request.ToString());
        //if (context.Request.TryReadJson<UserTest>(out UserTest? userTest))
        //{
        //    Console.WriteLine($"Deserialized UserTest: id={userTest?.id}, name={userTest?.name}, username={userTest?.username}");
        //}
        //context.Response.Ok();
        //context.Response.SetHeader("Connection", "close");
        //context.Response.Write("OK");
        _app!.HandleRequest(context);
        byte[] responseBuffer = ResponseSerializer.SerializeResponse(context.Response);
        fixed (byte* responsePtr = responseBuffer)
        {
            ThrewResponseToCpp(reqNative.ClientSocket, responsePtr, responseBuffer.Length);
        }
    }

    public static void ThrewResponseToCpp(nuint clientSocket, byte* buffer, int bufferSize)
    {
        if (_cppResponseCallback != null)
        {
            _cppResponseCallback(clientSocket, (IntPtr)buffer, bufferSize);
        }
    }

    public static void SetApplication(WebApplication app)
    {
        _app = app;
    }
}