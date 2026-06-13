using FrameworkCore.Http;
using FrameworkCore.Middlewares;
using FrameworkCore.Routing;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FrameworkCore;
public class WebApplication
{
    private Dictionary<HttpMethod, RoutingTable> appRoute;
    List<IMiddleware> middlewarePipeline;
    private RequestDelegate? _compiledPipeline;
    private void MapMethod(HttpMethod method, string pattern , RequestDelegate endpoint)
    {
        if (appRoute.TryGetValue(method, out RoutingTable? routingTable))
        {
            routingTable.RegisterNewRoute(pattern, endpoint);
        }
        else
        {
            appRoute[method] = new RoutingTable();
            appRoute[method].RegisterNewRoute(pattern, endpoint);
        }
    }
    private RequestDelegate BuildPipeline()
    {
        RequestDelegate app = context =>
        {
            return Task.CompletedTask;
        };

        for (int i = middlewarePipeline.Count - 1; i >= 0; i--)
        {
            IMiddleware middleware = middlewarePipeline[i];

            RequestDelegate next = app;

            app = context =>
            {
                return middleware.InvokeAsync(context, next);
            };
        }

        return app;
    }
    public WebApplication()
    {
        appRoute = new();
        middlewarePipeline = new();
        middlewarePipeline.Add(new ExceptionMiddleware());
        middlewarePipeline.Add(new RoutingMiddleware(this));
        middlewarePipeline.Add(new EndpointMiddleware());
        _compiledPipeline = BuildPipeline();
    }
    public bool TryGetEndpoint(HttpMethod method , string pattern , 
        out RequestDelegate endpoint , 
        out Dictionary<string, string> routeValues){
        if (appRoute.TryGetValue(method, out RoutingTable? routingTable)) {
            if (routingTable != null) {
                if (routingTable.TryMatch(pattern, out endpoint , out routeValues))
                {
                    return true;
                }
            }
        }
        endpoint = null!;
        routeValues = null!;
        return false;
    }

    public void UseMiddleware(IMiddleware middleware)
    {
        middlewarePipeline.Add(middleware);
    }

    public async Task HandleRequest(HttpContext context)
    {
        await _compiledPipeline(context);
    }

    public void MapGet(string pattern , RequestDelegate endpoint)
    {
        MapMethod(HttpMethod.Get, pattern, endpoint);
    }

    public void MapPost(string pattern, RequestDelegate endpoint) {
        MapMethod(HttpMethod.Post , pattern , endpoint);
    }

    public void MapPut(string pattern, RequestDelegate endpoint)
    {
        MapMethod(HttpMethod.Put, pattern, endpoint);
    }

    public void MapDelete(string pattern, RequestDelegate endpoint)
    {
        MapMethod(HttpMethod.Delete, pattern, endpoint);
    }

    public void MapHead(string pattern, RequestDelegate endpoint)
    {
        MapMethod(HttpMethod.Head, pattern, endpoint);
    }

    public void MapOptions(string pattern, RequestDelegate endpoint)
    {
        MapMethod(HttpMethod.Options , pattern, endpoint);
    }

    public void MapPatch(string pattern, RequestDelegate endpoint)
    {
        MapMethod(HttpMethod.Patch, pattern, endpoint);
    }

    public void MapTrace(string pattern, RequestDelegate endpoint)
    {
        MapMethod(HttpMethod.Trace, pattern, endpoint);
    }

    public void MapConnect(string pattern, RequestDelegate endpoint)
    {
        MapMethod(HttpMethod.Connect, pattern, endpoint);
    }

}