using FrameworkCore.Http;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
namespace FrameworkCore.Middlewares;
public class EndpointMiddleware : IMiddleware
{
    public async Task InvokeAsync(HttpContext context , RequestDelegate? next = null)
    {
        if (context.Endpoint != null)
        {
            await context.Endpoint(context);
            return;
        }
        context.Response.NotFound();
    }
}
