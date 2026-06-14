using FrameworkCore.Http;
using FrameworkCore.UserApplication.Models;

namespace FrameworkCore.UserApplication.Controllers;
public static class UserController
{
    private static List<UserModel> users = new List<UserModel>();
    public static void Index(HttpContext ctx)
    {
        ctx.Response.File("views/index.html");
    }
    public static void add(HttpContext ctx)
    {
        UserModel? user = ctx.Request.ReadJson<UserModel>();
        user.id = (users.Count + 1).ToString();
        if (user != null)
        {
            users.Add(user);
        }
    }
    public static void showAll(HttpContext ctx)
    {
        ctx.Response.Json<List<UserModel>>(users);
    }
    public static void show(HttpContext ctx)
    {
        int id = int.Parse(ctx.Request.RouteValues["id"]);
        UserModel user = users[id - 1];
        ctx.Response.Json<UserModel>(user);
    }
}
