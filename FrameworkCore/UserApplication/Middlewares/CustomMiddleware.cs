using FrameworkCore.Http;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FrameworkCore.UserApplication.Middlewares;
public class CustomMiddleware : IMiddleware
{
    public async Task InvokeAsync(HttpContext context, RequestDelegate? next)
    {
        Console.WriteLine("Request passed in My Custom Middleware");
        await next(context);
        Console.WriteLine("Response passed in My Custom Middleware");
    }
}
