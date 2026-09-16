using System.Runtime.InteropServices;
using System.Text.Json;
namespace EWAF;
internal sealed record PlanInput(string start, string end, uint weekday);
internal sealed class CoreException(string code, string message) : Exception(message)
{
    public string Code { get; } = code;
}
internal static class Core
{
    [DllImport("ewaf_ffi", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr ewaf_request(byte[] input, nuint length);
    [DllImport("ewaf_ffi", CallingConvention = CallingConvention.Cdecl)]
    private static extern void ewaf_string_free(IntPtr value);
    internal static JsonElement Call(object request)
    {
        var bytes = JsonSerializer.SerializeToUtf8Bytes(request);
        var response = ewaf_request(bytes, (nuint)bytes.Length);
        try
        {
            using var document =
                JsonDocument.Parse(Marshal.PtrToStringUTF8(response) ?? throw new IOException("No core response."));
            var root = document.RootElement;
            if (!root.GetProperty("ok").GetBoolean())
            {
                var error = root.GetProperty("error");
                throw new CoreException(error.GetProperty("code").GetString()!,
                                        error.GetProperty("message").GetString()!);
            }
            return root.GetProperty("value").Clone();
        }
        finally
        {
            ewaf_string_free(response);
        }
    }
    internal static Task<JsonElement> Create(PlanInput plan, string destination, CancellationToken cancellation,
                                             IProgress<JsonElement> progress) =>
        Task.Run(() =>
                 {
                     var handle = Call(new { op = "begin", plan, destination }).GetProperty("handle").GetUInt64();
                     using var registration = cancellation.Register(() => Call(new { op = "cancel", handle }));
                     try
                     {
                         JsonElement result;
                         do
                         {
                             result =
                                 Call(new { op = "step", handle, cancelled = cancellation.IsCancellationRequested });
                             progress.Report(result);
                         } while (!result.GetProperty("done").GetBoolean());
                         return result;
                     }
                     finally
                     {
                         Call(new { op = "release", handle });
                     }
                 });
    internal static string Summary(JsonElement result)
    {
        var counts =
            $"Created {result.GetProperty("created").GetInt32():N0} folders. Already existed: {result.GetProperty("existing").GetInt32():N0}.";
        if (result.GetProperty("failureReason").ValueKind == JsonValueKind.String)
            return counts + " " + result.GetProperty("failureReason").GetString();
        return result.GetProperty("cancelled").GetBoolean()
                   ? "Canceled. " + counts + " You can safely run the same range again."
                   : counts;
    }
}
