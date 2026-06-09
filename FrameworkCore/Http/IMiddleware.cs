namespace FrameworkCore.Http;
public interface IMiddleware
{
    Task InvokeAsync(HttpContext context, RequestDelegate next);
}