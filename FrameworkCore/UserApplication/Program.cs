using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using FrameworkCore;
using FrameworkCore.UserApplication.Controllers;
using FrameworkCore.UserApplication.Middlewares;
namespace UserApplication;
public static class Program
{
    public static WebApplication Build()
    {
        WebApplication app = new WebApplication();
        app.UseMiddleware(new CustomMiddleware());
        app.MapGet("/", async ctx => UserController.Index(ctx));
        app.MapGet("/user/{id}", async ctx => UserController.show(ctx));
        app.MapPost("/user", async ctx => UserController.add(ctx));
        app.MapGet("/users" , async ctx => UserController.showAll(ctx));
        app.Run();
        return app;
    }
}
