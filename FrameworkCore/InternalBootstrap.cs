using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using FrameworkCore.Http;

//[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
//public struct MyData
//{
//    public int Id;
//    public float Value;
//    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
//    public string Message;
//}
public static class InternalBootstrap
{
    // This attribute is critical
    [UnmanagedCallersOnly(EntryPoint = "HandleRequest", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static IntPtr HandleRequest(IntPtr dataPtr)
    {
        Request data = new Request(dataPtr);
        string response = $"C# Received request for URI: {data.GetUri()} with method: {data.GetMethod()}\n";
        for (int i = 0; i < data.GetHeaderCount(); i++)
        {
            response += $"Header {i}: {data.GetHeaderByIndex(i)} = {(string)data.GetHeaderValueByIndex(i)}\n";
        }
        return Marshal.StringToHGlobalAnsi(response);
    }
}