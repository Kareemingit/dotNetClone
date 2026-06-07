using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using FrameworkCore.Http.Native;

namespace FrameworkCore.Http.Headers;
public unsafe class HeaderCollection
{
    private Dictionary<string , HeaderValueRange> storage = new Dictionary<string , HeaderValueRange>();
    private readonly NativeHttpRequest _ptr;
    private readonly IntPtr _actualDataHeader;
    public ReadOnlySpan<byte> RawBodyBuffer
    {
        get
        {
            int totalSize = (int)(_ptr.BodyStartOffset + _ptr.ContentLength);
            return new ReadOnlySpan<byte>(_actualDataHeader.ToPointer(), totalSize);
        }
    }
    public HeaderCollection(NativeHttpRequest nativePointer, IntPtr actualDataPtr)
    {
        _ptr = nativePointer;
        _actualDataHeader = actualDataPtr;
    }
}