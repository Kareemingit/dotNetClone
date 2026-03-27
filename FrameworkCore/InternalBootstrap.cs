using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct MyData
{
    public int Id;
    public float Value;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
    public string Message;
}
public static class InternalBootstrap
{
    // This attribute is critical
    [UnmanagedCallersOnly(EntryPoint = "HandleRequest", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static IntPtr HandleRequest(IntPtr dataPtr)
    {
        // Convert the pointer back into a usable C# struct
        MyData data = Marshal.PtrToStructure<MyData>(dataPtr);

        string response = $"C# received ID: {data.Id}, Value: {data.Value}, Msg: {data.Message}";
        return Marshal.StringToHGlobalAnsi(response);
    }
}