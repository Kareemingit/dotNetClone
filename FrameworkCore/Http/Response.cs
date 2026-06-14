using System.Text;
using System.Text.Json;
using static System.Net.Mime.MediaTypeNames;
namespace FrameworkCore.Http;

public unsafe class Response
{
    private nuint ClientSocket;
    private string GetContentType(string filePath)
    {
        string extension = Path.GetExtension(filePath).ToLower();
        return extension switch
        {
            ".html" => "text/html",
            ".css" => "text/css",
            ".js" => "application/javascript",
            ".json" => "application/json",
            ".png" => "image/png",
            ".jpg" => "image/jpeg",
            ".jpeg" => "image/jpeg",
            ".gif" => "image/gif",
            _ => "application/octet-stream",
        };
    }
    public readonly List<string> Cookies = new();
    public int StatusCode { get; set; } = 200;
    public string StatusText { get; set; } = "OK";
    public string ContentType { get; set; } = "text/plain";
    public Dictionary<string, string> Headers { get; } = new (StringComparer.OrdinalIgnoreCase);
    public ReadOnlyMemory<byte> Body { get; set; }
    public Response(UIntPtr clientSocket)
    {
        ClientSocket = clientSocket;
    }

    public void SetHeader(string name, string value)
    {
        Headers[name] = value;
    }
    public string? GetHeader(string key)
    {
        if (Headers.TryGetValue(key, out var value))
            return value;
        return null;
    }

    public void Write(string text)
    {
        Body = Encoding.UTF8.GetBytes(text);
        SetHeader("Content-Type", "text/plain");
        SetHeader("Content-Length", Body.Length.ToString());
    }

    public void Json<T>(T obj)
    {
        string json = JsonSerializer.Serialize(obj);
        ContentType = "application/json";
        Body = Encoding.UTF8.GetBytes(json);
        SetHeader("Content-Type", "application/json");
        SetHeader("Content-Length", Body.Length.ToString());
    }

    public void Html(string html)
    {
        ContentType = "text/html";
        Body = Encoding.UTF8.GetBytes(html);
        SetHeader("Content-Type", "text/html");
        SetHeader("Content-Length", Body.Length.ToString());
    }

    public void File(string filePath)
    {
        string baseDir = Directory.GetCurrentDirectory();
        string stopAt = "dotnetClone";
        int index = baseDir.IndexOf(stopAt);

        if (index != -1)
        {
            baseDir = baseDir.Substring(0, index + stopAt.Length) + "\\FrameworkCore\\UserApplication";
        }
        string fullPath = Path.Combine(baseDir, filePath);
        
        if (!System.IO.File.Exists(fullPath))
        {
            StatusCode = 404;
            StatusText = "Not Found";
            Body = Encoding.UTF8.GetBytes("File not found.");
            return;
        }
        byte[] fileBytes = System.IO.File.ReadAllBytes(fullPath);
        string contentType = GetContentType(fullPath);
        ContentType = contentType;
        Body = fileBytes;
    }

    public void Redirect(string url, int statusCode = 302)
    {
        StatusCode = statusCode;
        StatusText = StatusCode switch
        {
            301 => "Moved Permanently",
            302 => "Found",
            303 => "See Other",
            307 => "Temporary Redirect",
            308 => "Permanent Redirect",
            _ => "Found"
        };
        SetHeader("Location", url);
        Body = Encoding.UTF8.GetBytes($"<html><body>Redirecting to <a href=\"{url}\">{url}</a></body></html>");
        SetHeader("Content-Type", "text/html");
        SetHeader("Content-Length", Body.Length.ToString());
    }

    public void Ok()
    {
        StatusCode = 200;
        StatusText = "OK";
    }

    public void NotFound()
    {
        StatusCode = 404;
        StatusText = "Not Found";
        Body = Encoding.UTF8.GetBytes("Resource not found.");
    }

    public void BadRequest()
    {
        StatusCode = 400;
        StatusText = "Bad Request";
        Body = Encoding.UTF8.GetBytes("Bad Request");
    }

    public void Unauthorized()
    {
        StatusCode = 401;
        StatusText = "Unauthorized";
        Body = Encoding.UTF8.GetBytes("Unauthorized Access");
    }
    public void NoContent()
    {
        StatusCode = 204;
        StatusText = "No Content";
        Body = ReadOnlyMemory<byte>.Empty;
    }
    public void Forbidden()
    {
        StatusCode = 403;
        StatusText = "Forbidden";
    }
}
