using EWAF;
using System.Text.Json;
static void Check(bool value)
{
    if (!value)
        throw new Exception("Native binding contract failed.");
}
var plan = new PlanInput("09-03-2026", "09-17-2026", 5);
var preview = Core.Call(new { op = "plan", plan, search = "09-10" });
Check(preview.GetProperty("count").GetInt32() == 3 && preview.GetProperty("dates").GetArrayLength() == 1);
var path = Path.Combine(Path.GetTempPath(), "EWAF-" + Guid.NewGuid());
Directory.CreateDirectory(path);
try
{
    var result = await Core.Create(plan, path, CancellationToken.None, new Progress<JsonElement>());
    Check(result.GetProperty("created").GetInt32() == 3);
    var contents = Path.Combine(path, "09-03-2026", "keep.txt");
    File.WriteAllText(contents, "Keep my work");
    result = await Core.Create(plan, path, CancellationToken.None, new Progress<JsonElement>());
    Check(result.GetProperty("existing").GetInt32() == 3 && File.ReadAllText(contents) == "Keep my work");
    using var token = new CancellationTokenSource();
    token.Cancel();
    result = await Core.Create(plan, path, token.Token, new Progress<JsonElement>());
    Check(result.GetProperty("cancelled").GetBoolean());
}
finally
{
    Directory.Delete(path, true);
}
Console.WriteLine("EWAF C# / Rust integration passed.");
