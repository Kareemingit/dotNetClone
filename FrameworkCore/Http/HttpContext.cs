using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using FrameworkCore.Http.Native;

namespace FrameworkCore.Http;
internal class HttpContext
{
    public Request Request;
    public Response Response;

    public HttpContext(NativeHttpRequest nativeRequest, IntPtr rawDataPtr)
    {
        Request = new Request(nativeRequest, rawDataPtr);
        Response = new Response(nativeRequest.ClientSocket);
    }
}