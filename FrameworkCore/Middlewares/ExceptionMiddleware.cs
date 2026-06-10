using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using FrameworkCore.Http;

namespace FrameworkCore.Middlewares;
public class ExceptionMiddleware : IMiddleware
{
    public async Task InvokeAsync(HttpContext context , RequestDelegate next)
    {
        try
        {
            await next(context);
        }
        catch (Exception ex){
            context.Response.StatusCode = 500;
            context.Response.Write("Internal Server Error");
        }
    }
}
