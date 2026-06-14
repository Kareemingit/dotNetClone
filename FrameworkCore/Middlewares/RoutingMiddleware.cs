using FrameworkCore.Http;
using FrameworkCore.Routing;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
namespace FrameworkCore.Middlewares;
public class RoutingMiddleware : IMiddleware
{
    private readonly WebApplication app;

    public RoutingMiddleware(WebApplication _app)
    {
        this.app = _app;
    }
    public async Task InvokeAsync(HttpContext context, RequestDelegate next)
    {
        RequestDelegate? requestDelegate;
        Dictionary<string, string> routeValues;

        if (app.TryGetEndpoint(context.Request.Method , context.Request.Path, out requestDelegate , out routeValues))
        {
            context.Endpoint = requestDelegate;
            foreach (var routeValue in routeValues) {
                context.Request.RouteValues[routeValue.Key] = routeValue.Value;
            }
        }
        await next(context);
        context.Response.SetHeader("Server", "Kira");
    }
}
