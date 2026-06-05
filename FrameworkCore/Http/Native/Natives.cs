using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
namespace FrameworkCore.Http.Native;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct NativeHttpRequest
{
    private fixed byte vector_padding[32];
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
public struct NativeHeaderField
{
    public uint NameOffset;
    public uint NameLen;
    public uint ValueOffset;
    public uint ValueLen;
}