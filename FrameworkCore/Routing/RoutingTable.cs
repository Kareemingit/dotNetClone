using FrameworkCore.Http;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
namespace FrameworkCore.Routing;

public class RoutingTable
{
    private Dictionary<string, RequestDelegate> _staticRouts = new();
    private List<KeyValuePair<string[] , RequestDelegate>> _dynamicRouts = new();
    private void AddDynamicRoute(string pattern , RequestDelegate endpoint)
    {
        string[] template = pattern.Trim('/').Split('/', StringSplitOptions.RemoveEmptyEntries);
        //validate { }
        var pair = KeyValuePair.Create(template , endpoint);
        _dynamicRouts.Add(pair);
    }

    public void RegisterNewRoute(string pattern , RequestDelegate endpoint){
        if (pattern != null)
        {
            if (pattern.Contains("{") && pattern.Contains("}"))
            {
                AddDynamicRoute(pattern, endpoint);
            }
            else {
                _staticRouts[pattern] = endpoint;
            }
        }
    }
    public bool TryMatch(string path,out RequestDelegate endpoint,out Dictionary<string, string> routeValues)
    {
        routeValues = new Dictionary<string, string>();
        if (_staticRouts.TryGetValue(path, out endpoint)){
            return true;
        }
        string[] requestSegments = path.Trim('/').Split('/', StringSplitOptions.RemoveEmptyEntries);

        foreach (var route in _dynamicRouts)
        {
            string[] templateSegments = route.Key;

            if (templateSegments.Length != requestSegments.Length)
                continue;

            routeValues.Clear();
            bool matched = true;

            for (int i = 0; i < templateSegments.Length; i++)
            {
                string templateSegment = templateSegments[i];
                string requestSegment = requestSegments[i];
                if (templateSegment.StartsWith('{') && templateSegment.EndsWith('}'))
                {
                    string parameterName = templateSegment[1..^1];
                    routeValues[parameterName] = requestSegment;
                }
                else if (!templateSegment.Equals(requestSegment,StringComparison.OrdinalIgnoreCase))
                {
                    matched = false;
                    break;
                }
            }
            if (matched)
            {
                endpoint = route.Value;
                return true;
            }
        }

        endpoint = null!;
        routeValues.Clear();
        return false;
    }
}
