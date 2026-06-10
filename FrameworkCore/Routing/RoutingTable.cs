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
    private List<KeyValuePair<List<string> , RequestDelegate>> _dynamicRouts = new();

    RoutingTable()
    {
    }

    public RequestDelegate MatchPattern(string pattern)
    {
        if(_staticRouts.TryGetValue(pattern , out RequestDelegate endpoint))
            return endpoint;

        return null;
    }
}
