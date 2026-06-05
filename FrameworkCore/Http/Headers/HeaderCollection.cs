using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using FrameworkCore.Http.Native;

namespace FrameworkCore.Http.Headers;

public class HeaderValueRange
{
    public uint Offset;
    public uint Length;
}
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
    public void FillHeaders(string name)
    {
        ReadOnlySpan<byte> buffer = RawBodyBuffer;
        for (int i = 0; i < _ptr.NumHeaders; i++)
        {
            var header = _ptr.Headers[i];
            var nameSpan = buffer.Slice((int)header.NameOffset, (int)header.NameLen);
            storage[Encoding.ASCII.GetString(nameSpan)] = new HeaderValueRange
            {
                Offset = header.ValueOffset,
                Length = header.ValueLen
            };
        }
    }

}