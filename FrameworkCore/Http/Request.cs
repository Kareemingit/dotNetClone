using System;
using System.Collections.Generic;
using System.Text;
using FrameworkCore.Http.Native;
namespace FrameworkCore.Http;

//[StructLayout(LayoutKind.Sequential, Pack = 1)]
//public unsafe struct NativeHttpRequest
//{
//    private fixed byte vector_padding[32];
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
//public struct NativeHeaderField
//{
//    public uint NameOffset;
//    public uint NameLen;
//    public uint ValueOffset;
//    public uint ValueLen;
//}

internal unsafe class Request
{
    private readonly NativeHttpRequest _ptr;
    private readonly IntPtr _actualDataHeader;

    public Request(NativeHttpRequest nativePointer, IntPtr actualDataPtr)
    {
        _ptr = nativePointer;
        _actualDataHeader = actualDataPtr;
    }
    public ReadOnlySpan<byte> RawBodyBuffer
    {
        get{
            int totalSize = (int)(_ptr.BodyStartOffset + _ptr.ContentLength);
            return new ReadOnlySpan<byte>(_actualDataHeader.ToPointer(), totalSize);
        }
    }

    public int GetHeaderCount() => _ptr.NumHeaders;
    public ReadOnlySpan<byte> GetMethodSpan() => RawBodyBuffer.Slice(_ptr.MethodOffset, _ptr.MethodLen);
    public ReadOnlySpan<byte> GetUriSpan() => RawBodyBuffer.Slice((int)_ptr.UriOffset, (int)_ptr.UriLen);
    public ReadOnlySpan<byte> GetBody() => RawBodyBuffer.Slice((int)_ptr.BodyStartOffset, (int)_ptr.ContentLength);

    public string GetMethod() => Encoding.ASCII.GetString(GetMethodSpan());

    public string GetUri() => Encoding.UTF8.GetString(GetUriSpan());

    public string GetHeaderByIndex(int index)
    {
        if (index < 0 || index >= _ptr.NumHeaders)
            throw new ArgumentOutOfRangeException(nameof(index));
        var header = _ptr.Headers[index];
        var nameSpan = RawBodyBuffer.Slice((int)header.NameOffset, (int)header.NameLen);
        var valueSpan = RawBodyBuffer.Slice((int)header.ValueOffset, (int)header.ValueLen);
        return $"{Encoding.ASCII.GetString(nameSpan)}: {Encoding.UTF8.GetString(valueSpan)}";
    }
    public string GetHeaderByName(string name)
    {
        ReadOnlySpan<byte> buffer = RawBodyBuffer;
        for (int i = 0; i < _ptr.NumHeaders; i++)
        {
            var header = _ptr.Headers[i];
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
        if (index < 0 || index >= _ptr.NumHeaders)
            throw new ArgumentOutOfRangeException(nameof(index));
        var header = _ptr.Headers[index];
        return Encoding.UTF8.GetString(RawBodyBuffer.Slice((int)header.ValueOffset, (int)header.ValueLen));
    }
}