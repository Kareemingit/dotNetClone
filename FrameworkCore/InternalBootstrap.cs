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
        Request request = new Request(reqNative, rawDataPtr);
        Console.WriteLine(request.ToString());
        if (request.TryReadJson<UserTest>(out UserTest? userTest))
        {
            Console.WriteLine($"Deserialized UserTest: id={userTest?.id}, name={userTest?.name}, username={userTest?.username}");
        }
        Response response = new Response(reqNative.ClientSocket);
        response.Ok();
        response.SetHeader("Connection", "close");
        response.Write("OK");
        //string responseStr = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK";
        byte[] responseBuffer = ResponseSerializer.SerializeResponse(response);
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
}