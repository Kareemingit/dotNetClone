using System.Text;

namespace FrameworkCore.Http;
public class ResponseSerializer
{
    public static byte[] SerializeResponse(Response response)
    {
        StringBuilder sb = new();
        sb.Append($"HTTP/1.1 {response.StatusCode} {response.StatusText}\r\n");
        foreach (var header in response.Headers)
        {
            sb.Append($"{header.Key}: {header.Value}\r\n");
        }
        foreach (var cookie in response.Cookies)
        {
            sb.Append($"Set-Cookie: {cookie}\r\n");
        }
        sb.Append($"Content-Type: {response.ContentType}\r\n");
        sb.Append($"Content-Length: {response.Body.Length}\r\n");
        sb.Append("Connection: close\r\n");
        sb.Append("\r\n");
        byte[] headerBytes = Encoding.UTF8.GetBytes(sb.ToString());
        byte[] responseBytes = new byte[headerBytes.Length + response.Body.Length];
        Buffer.BlockCopy(headerBytes, 0, responseBytes, 0, headerBytes.Length);
        Buffer.BlockCopy(response.Body.ToArray(), 0, responseBytes, headerBytes.Length, response.Body.Length);
        return responseBytes;
    }
}
