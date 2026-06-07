using System;
using System.Collections.Generic;
using System.Text;
using FrameworkCore.Http.Native;
using FrameworkCore.Http.Headers;
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

public unsafe sealed class Request
{
    private readonly NativeHttpRequest _native;
    private readonly byte* _buffer;
    private string? _cachedPath;
    private string? _cachedBody;
    private Dictionary<string, string> _cachedHeaders;
    private ReadOnlySpan<byte> PathSpan=> new(_buffer + _native.UriOffset,(int)_native.UriLen);
    private ReadOnlySpan<byte> BodySpan=> new(_buffer + _native.BodyStartOffset,(int)_native.ContentLength);
    public HttpMethod Method => _native.MethodId switch
    {
        1 => HttpMethod.Get,
        2 => HttpMethod.Post,
        3 => HttpMethod.Put,
        4 => HttpMethod.Delete,
        5 => HttpMethod.Head,
        6 => HttpMethod.Options,
        7 => HttpMethod.Patch,
        8 => HttpMethod.Trace,
        9 => HttpMethod.Connect,
        _ => HttpMethod.Get
    };
    public string Path
    {
        get
        {
            if(_cachedPath != null)
                return _cachedPath;
            var pathSpan = PathSpan;
            int queryIndex = pathSpan.IndexOf((byte)'?');
            if (queryIndex >= 0)
            {
                _cachedPath = Encoding.UTF8.GetString(pathSpan.Slice(0, queryIndex));
                return _cachedPath;
            }
            _cachedPath = Encoding.UTF8.GetString(pathSpan);
            return _cachedPath;
        }
    }
    public string QueryString { get; set; }
    public string Body => GetBody();
    public Dictionary<string, string> RouteValues { get; }
    public long ContentLength { get; set; }
    public string ContentType { get; set; }
    private string GetBody()
    {
        if (_cachedBody != null)
            return _cachedBody;
        if (ContentLength == 0)
            return string.Empty;
        _cachedBody = Encoding.UTF8.GetString(BodySpan);
        return _cachedBody;
    }
    public Request(NativeHttpRequest nativePointer, IntPtr actualDataPtr)
    {
        _native = nativePointer;
        _buffer = (byte*)actualDataPtr;
        ContentLength = GetHeader(HeaderNames.ContentLength) != null ? long.Parse(GetHeader(HeaderNames.ContentLength)!) : 0;
        _cachedHeaders = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        ContentType = GetHeader(HeaderNames.ContentType) ?? string.Empty;
        ExtractQueryString();
    }
    private void ExtractQueryString()
    {
        var pathSpan = PathSpan;
        int queryIndex = pathSpan.IndexOf((byte)'?');
        if (queryIndex >= 0)
        {
            QueryString = Encoding.UTF8.GetString(pathSpan.Slice(queryIndex + 1));
        }
        else
        {
            QueryString = string.Empty;
        }
    }
    public string? GetHeader(string name)
    {
        if (_cachedHeaders != null && _cachedHeaders.TryGetValue(name, out var value))
            return value;
        for (int i = 0; i < _native.NumHeaders; i++)
        {
            var header = _native.Headers[i];
            var key = Encoding.UTF8.GetString(_buffer + header.NameOffset, (int)header.NameLen).Trim();
            if (key.Equals(name, StringComparison.OrdinalIgnoreCase))
            {
                return Encoding.UTF8.GetString(_buffer + header.ValueOffset, (int)header.ValueLen).Trim();
            }
        }
        return null;
    }

    public Dictionary<string, string> GetAllHeaders()
    {
        if (_cachedHeaders != null && _cachedHeaders.Count > 0)
            return _cachedHeaders;
        var headers = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        for (int i = 0; i < _native.NumHeaders; i++)
        {
            var header = _native.Headers[i];
            var key = Encoding.UTF8.GetString(_buffer + header.NameOffset, (int)header.NameLen);
            var value = Encoding.UTF8.GetString(_buffer + header.ValueOffset, (int)header.ValueLen);
            headers[key] = value;
        }
        _cachedHeaders = headers;
        return headers;
    }

    public override string ToString()
    {
        string request = "";
        request += $"\nReceived request: {Method} {Path}";
        request += $"\nContent Length: {ContentLength}";
        request += $"\nContent Type: {ContentType}";
        request += $"\nQuery String: {QueryString}";
        request += $"\nBody start offset: {_native.BodyStartOffset}";
        request += $"\nHeaders:";
        foreach (var header in GetAllHeaders())
        {
            request += $"\n  {header.Key}: {header.Value}";
        }
        request += $"\nBody: {Body}";
        return request;
    }
}