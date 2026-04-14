using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
namespace FrameworkCore.Http;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal unsafe struct NativeHttpResponse
{
    public byte* BufferRawPtr;
    public UIntPtr ClientSocket;
}
public unsafe class Response
{
    private readonly NativeHttpResponse* _ptr;
    private readonly Dictionary<string, string> _headers = new();

    public int StatusCode { get; set; } = 200;
    public string StatusText { get; set; } = "OK";
    public byte[] Body { get; set; } = Array.Empty<byte>();

    public Response(IntPtr nativePointer)
    {
        _ptr = (NativeHttpResponse*)nativePointer;
    }

    public void SetHeader(string name, string value)
    {
        _headers[name] = value;
    }

    public void SetClientSocket(UIntPtr socket)
    {
        _ptr->ClientSocket = socket;
    }

    /// <summary>
    /// Parses all attributes into a single HTTP/1.1 wire-format string,
    /// allocates unmanaged memory for it, and updates the C++ struct.
    /// </summary>


    public void FinalizeResponse()
    {
        // 1. Build the Header section
        StringBuilder sb = new StringBuilder();
        sb.Append($"HTTP/1.1 {StatusCode} {StatusText}\r\n");

        // Ensure Content-Length is set correctly
        _headers["Content-Length"] = Body.Length.ToString();
        _headers["Server"] = "WinShield-Net-Core";

        foreach (var header in _headers)
        {
            sb.Append($"{header.Key}: {header.Value}\r\n");
        }
        sb.Append("\r\n"); // End of headers

        // 2. Convert headers to bytes
        byte[] headerBytes = Encoding.ASCII.GetBytes(sb.ToString());
        int totalLength = headerBytes.Length + Body.Length;

        // 3. Allocate unmanaged memory that C++ will eventually free
        // Use NativeMemory.Alloc (available in .NET 6+) for high performance
        byte* finalBuffer = (byte*)NativeMemory.Alloc((nuint)totalLength);

        // 4. Copy headers and body into the unmanaged buffer
        fixed (byte* hPtr = headerBytes)
        {
            Buffer.MemoryCopy(hPtr, finalBuffer, headerBytes.Length, headerBytes.Length);
        }

        if (Body.Length > 0)
        {
            fixed (byte* bPtr = Body)
            {
                Buffer.MemoryCopy(bPtr, finalBuffer + headerBytes.Length, Body.Length, Body.Length);
            }
        }

        // 5. Update the C++ structure pointers
        _ptr->BufferRawPtr = finalBuffer;

        // Note: You should also pass 'totalLength' back to C++ 
        // either through the struct or a return value so it knows how many bytes to send.
    }
}