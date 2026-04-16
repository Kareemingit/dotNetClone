using FrameworkCore.Http;
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
    [UnmanagedCallersOnly(EntryPoint = "HandleRequest", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static IntPtr HandleRequest(IntPtr dataPtr , IntPtr rawDataPtr)
    {
        NativeHttpRequest reqNative = Marshal.PtrToStructure<NativeHttpRequest>(dataPtr);
        Request data = new Request(reqNative, rawDataPtr);
        string response = $"Received request for URI: {data.GetUri()} with method: {data.GetMethod()}";
        for(int i = 0; i < data.GetHeaderCount(); i++)
        {
            response += $"\n{i+1}: {data.GetHeaderByIndex(i)}: {data.GetHeaderValueByIndex(i)}";
        }
        return Marshal.StringToHGlobalAnsi(response);
    }
}