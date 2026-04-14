using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
namespace FrameworkCore.Http;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal unsafe struct NativeHttpRequest
{
    public byte* BufferRawPtr;
    public UIntPtr ClientSocket;

    public uint UriOffset;
    public uint UriLen;
    public uint BodyStartOffset;
    public uint ContentLength;

    public ushort MethodOffset;
    public ushort MethodLen;
    public ushort NumHeaders;

    public byte MethodId;
    public byte VersionMajor;
    public byte VersionMinor;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
    public NativeHeaderField[] Headers;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal struct NativeHeaderField
{
    public uint NameOffset;
    public uint NameLen;
    public uint ValueOffset;
    public uint ValueLen;
}
internal unsafe class Request
{
    private readonly NativeHttpRequest* _ptr;
    public Request(IntPtr nativePointer)
    {
        _ptr = (NativeHttpRequest*)nativePointer;
    }
    /// <summary>
    /// Accesses the raw bytes of the request without copying.
    /// </summary>
    public ReadOnlySpan<byte> RawBuffer => new(_ptr->BufferRawPtr, (int)(_ptr->BodyStartOffset + _ptr->ContentLength));

    public int GetHeaderCount() => _ptr->NumHeaders;
    public ReadOnlySpan<byte> GetMethodSpan() => RawBuffer.Slice(_ptr->MethodOffset, _ptr->MethodLen);

    public ReadOnlySpan<byte> GetUriSpan() => RawBuffer.Slice((int)_ptr->UriOffset, (int)_ptr->UriLen);

    public ReadOnlySpan<byte> GetBody() => RawBuffer.Slice((int)_ptr->BodyStartOffset, (int)_ptr->ContentLength);

    // Convenience methods that allocate strings only when needed
    public string GetMethod() => Encoding.ASCII.GetString(GetMethodSpan());
    public string GetUri() => Encoding.UTF8.GetString(GetUriSpan());

    public string GetHeaderByIndex(int index)
    {
        if (index < 0 || index >= _ptr->NumHeaders)
            throw new ArgumentOutOfRangeException(nameof(index));
        var header = _ptr->Headers[index];
        var nameSpan = RawBuffer.Slice((int)header.NameOffset, (int)header.NameLen);
        var valueSpan = RawBuffer.Slice((int)header.ValueOffset, (int)header.ValueLen);
        return $"{Encoding.ASCII.GetString(nameSpan)}: {Encoding.UTF8.GetString(valueSpan)}";
    }
    public string GetHeaderByName(string name)
    {
        ReadOnlySpan<byte> buffer = RawBuffer;
        for (int i = 0; i < _ptr->NumHeaders; i++)
        {
            var header = _ptr->Headers[i];
            var nameSpan = buffer.Slice((int)header.NameOffset, (int)header.NameLen);

            if (Encoding.ASCII.GetString(nameSpan).Equals(name, StringComparison.OrdinalIgnoreCase))
            {
                var valueSpan = buffer.Slice((int)header.ValueOffset, (int)header.ValueLen);
                return Encoding.UTF8.GetString(valueSpan);
            }
        }
        return null;
    }

    public string GetHeaderValueByIndex(int index)
    {
        if (index < 0 || index >= _ptr->NumHeaders)
            throw new ArgumentOutOfRangeException(nameof(index));
        var header = _ptr->Headers[index];
        return Encoding.UTF8.GetString(RawBuffer.Slice((int)header.ValueOffset, (int)header.ValueLen));
    }
    public string GetHeaderValueByName(string headerName)
    {
        var buffer = RawBuffer;
        // Header array starts immediately after the fixed members of NativeHttpRequest
        NativeHeaderField* headerArray = (NativeHeaderField*)((byte*)_ptr + sizeof(NativeHttpRequest));

        for (int i = 0; i < _ptr->NumHeaders; i++)
        {
            var h = headerArray[i];
            var nameSpan = buffer.Slice((int)h.NameOffset, (int)h.NameLen);

            // Fast check without string allocation
            if (IsMatch(nameSpan, headerName))
            {
                return Encoding.UTF8.GetString(buffer.Slice((int)h.ValueOffset, (int)h.ValueLen));
            }
        }
        return null;
    }

    private bool IsMatch(ReadOnlySpan<byte> span, string name)
    {
        // Simple ASCII case-insensitive comparison
        if (span.Length != name.Length) return false;
        for (int i = 0; i < span.Length; i++)
        {
            if (char.ToLowerInvariant((char)span[i]) != char.ToLowerInvariant(name[i]))
                return false;
        }
        return true;
    }
}


//[StructLayout(LayoutKind.Sequential, Pack = 1)]
//internal struct NativeHttpRequest
//{
//    public IntPtr BufferPtr;
//    public UIntPtr ClientSocket;

//    public uint UriOffset;
//    public uint UriLen;
//    public uint BodyStartOffset;
//    public uint ContentLength;

//    public ushort MethodOffset;
//    public ushort MethodLen;
//    public ushort NumHeaders;

//    public byte MethodId;
//    public byte VersionMajor;
//    public byte VersionMinor;

//    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
//    public NativeHeaderField[] Headers;
//}

//[StructLayout(LayoutKind.Sequential, Pack = 1)]
//internal struct NativeHeaderField
//{
//    public uint NameOffset;
//    public uint NameLen;
//    public uint ValueOffset;
//    public uint ValueLen;
//}
//internal unsafe class Request
//{
//    private readonly NativeHttpRequest* _nativeRequest;

//    public Request(IntPtr nativePtr)
//    {
//        _nativeRequest = (NativeHttpRequest*)nativePtr;
//    }

//    // Helper to create a ReadOnlySpan over the raw buffer
//    private ReadOnlySpan<byte> GetRawBuffer()
//    {
//        return new ReadOnlySpan<byte>(_nativeRequest->BufferPtr.ToPointer(),
//            (int)(_nativeRequest->BodyStartOffset + _nativeRequest->ContentLength));
//    }

//    public string GetMethod()
//    {
//        ReadOnlySpan<byte> buffer = GetRawBuffer();
//        var methodSpan = buffer.Slice(_nativeRequest->MethodOffset, _nativeRequest->MethodLen);
//        return Encoding.ASCII.GetString(methodSpan);
//    }

//    public string GetUri()
//    {
//        ReadOnlySpan<byte> buffer = GetRawBuffer();
//        var uriSpan = buffer.Slice((int)_nativeRequest->UriOffset, (int)_nativeRequest->UriLen);
//        return Encoding.UTF8.GetString(uriSpan);
//    }

//    public ReadOnlySpan<byte> GetBody()
//    {
//        ReadOnlySpan<byte> buffer = GetRawBuffer();
//        return buffer.Slice((int)_nativeRequest->BodyStartOffset, (int)_nativeRequest->ContentLength);
//    }

//    public string GetHeader(string name)
//    {
//        ReadOnlySpan<byte> buffer = GetRawBuffer();
//        for (int i = 0; i < _nativeRequest->NumHeaders; i++)
//        {
//            var header = _nativeRequest->Headers[i];
//            var nameSpan = buffer.Slice((int)header.NameOffset, (int)header.NameLen);

//            if (Encoding.ASCII.GetString(nameSpan).Equals(name, StringComparison.OrdinalIgnoreCase))
//            {
//                var valueSpan = buffer.Slice((int)header.ValueOffset, (int)header.ValueLen);
//                return Encoding.UTF8.GetString(valueSpan);
//            }
//        }
//        return null;
//    }
//}
