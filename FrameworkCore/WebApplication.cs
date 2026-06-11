using FrameworkCore.Http;
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

    private void MapMethode(HttpMethod method, string pattern , RequestDelegate endpoint)
    {
        if (appRoute.TryGetValue(method, out RoutingTable routingTable))
        {
            routingTable.RegisterNewRoute(pattern, endpoint);
        }
        else
        {
            appRoute[method] = new RoutingTable();
            appRoute[method].RegisterNewRoute(pattern, endpoint);
        }
    }

    public WebApplication()
    {
        appRoute = new Dictionary<HttpMethod, RoutingTable>();
    }
    public bool TryGetEndpoint(HttpMethod method , string pattern , 
        out RequestDelegate endpoint , 
        out Dictionary<string, string> routeValues){
        if (appRoute.TryGetValue(method, out RoutingTable routingTable)) {
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

    public void MapGet(string pattern , RequestDelegate endpoint)
    {
        MapMethode(HttpMethod.Get, pattern, endpoint);
    }

    public void MapPost(string pattern, RequestDelegate endpoint) {
        MapMethode(HttpMethod.Post , pattern , endpoint);
    }

    public void MapPut(string pattern, RequestDelegate endpoint)
    {
        MapMethode(HttpMethod.Put, pattern, endpoint);
    }

    public void MapDelete(string pattern, RequestDelegate endpoint)
    {
        MapMethode(HttpMethod.Delete, pattern, endpoint);
    }

    public void MapHead(string pattern, RequestDelegate endpoint)
    {
        MapMethode(HttpMethod.Head, pattern, endpoint);
    }

    public void MapOptions(string pattern, RequestDelegate endpoint)
    {
        MapMethode(HttpMethod.Options , pattern, endpoint);
    }

    public void MapPatch(string pattern, RequestDelegate endpoint)
    {
        MapMethode(HttpMethod.Patch, pattern, endpoint);
    }

    public void MapTrace(string pattern, RequestDelegate endpoint)
    {
        MapMethode(HttpMethod.Trace, pattern, endpoint);
    }

    public void MapConnect(string pattern, RequestDelegate endpoint)
    {
        MapMethode(HttpMethod.Connect, pattern, endpoint);
    }

}