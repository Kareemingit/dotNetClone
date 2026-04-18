using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FrameworkCore.Http;
internal class HttpContext
{
    public Request Request;
    public Response Response;

    public HttpContext(Request request, Response response)
    {
        Request = request;
        Response = response;
    }
}