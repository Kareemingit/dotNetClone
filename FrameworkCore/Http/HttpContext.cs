using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using FrameworkCore.Http.Native;

namespace FrameworkCore.Http;
public class HttpContext
{
    public Request Request;
    public Response Response;
    public Dictionary<string, object> Items { get; }
    public HttpContext(NativeHttpRequest nativeRequest, IntPtr rawDataPtr)
    {
        Request = new Request(nativeRequest, rawDataPtr);
        Response = new Response(nativeRequest.ClientSocket);
        Items = new Dictionary<string, object>();
    }
}