// SPDX-License-Identifier: GPL-3.0-or-later
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json;

internal static class Program
{
    private sealed class VerificationException(string code, string message) : Exception(message)
    { internal string Code { get; } = code; }

    private static async Task<int> Main(string[] args)
    {
        bool coordinated = false;
        try
        {
            bool verifyOnly = args.Length == 6 && args[0] == "--verify-only";
            if (!verifyOnly && !(args.Length == 8 && args[0] == "--install"))
                throw new VerificationException("arguments", "Expected mode, MSI, digest, publisher, version, architecture and optional parent ID/start time.");
            string package = Path.GetFullPath(args[1]);
            if (!File.Exists(package) || Path.GetExtension(package) != ".msi")
                throw new VerificationException("package", "Expected an MSI update.");
            // Keep the authenticated bytes immutable through msiexec: other handles may read,
            // but cannot modify or replace the package while this handle remains open.
            using (var lockedPackage = new FileStream(package, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                string digest = Convert.ToHexString(SHA256.HashData(lockedPackage));
                if (!string.Equals(digest, args[2], StringComparison.OrdinalIgnoreCase))
                    throw new VerificationException("digest", "Update digest changed.");
                VerifyPublisher(package, args[3]);
                VerifyIdentity(package, args[4], args[5]);
                if (verifyOnly)
                {
                    Console.WriteLine("{\"verified\":true}");
                    return 0;
                }
                int parentId = int.Parse(args[6]);
                long parentStarted = long.Parse(args[7]);
                using var parent = Process.GetProcessById(parentId);
                if (parent.Id == Environment.ProcessId || parent.StartTime.ToUniversalTime().Ticks != parentStarted ||
                    !string.Equals(parent.ProcessName, "EWAF", StringComparison.OrdinalIgnoreCase))
                    throw new VerificationException("coordinator", "The original EWAF process is no longer running.");
                Console.WriteLine("READY");
                Console.Out.Flush();
                // The app can reject this handshake if work began while verification ran.
                string? command = await Console.In.ReadLineAsync().WaitAsync(TimeSpan.FromMinutes(2));
                if (command != "COMMIT") return 0;
                coordinated = true;
                await parent.WaitForExitAsync().WaitAsync(TimeSpan.FromMinutes(2));
                if (Process.GetProcessesByName("EWAF").Any())
                    throw new VerificationException("coordinator", "Close all EWAF instances before installation.");
                var start = new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "msiexec.exe"))
                { UseShellExecute = false };
                foreach (string arg in new[] { "/i", package, "/passive", "REBOOT=ReallySuppress", "MSIRESTARTMANAGERCONTROL=Disable" })
                    start.ArgumentList.Add(arg);
                using var installer = Process.Start(start) ?? throw new IOException("Could not start Windows Installer.");
                await installer.WaitForExitAsync();
                if (installer.ExitCode != 0 && installer.ExitCode != 3010)
                    throw new VerificationException("installation", $"Windows Installer failed ({installer.ExitCode}). Its transaction restores the previous installation on failure.");
                if (installer.ExitCode == 3010)
                    MessageBox(IntPtr.Zero, "Windows requires a restart to finish this update. Save your work and restart Windows before reopening EWAF.", "EWAF update", 0x40);
            }
            try { File.Delete(package); }
            catch (IOException) { /* Successful installation must not be reported as a failure to clean up staging. */ }
            catch (UnauthorizedAccessException) { }
            return 0;
        }
        catch (Exception error)
        {
            string code = error is VerificationException v ? v.Code : "runtime";
            try { Console.Error.WriteLine(JsonSerializer.Serialize(new { verified = false, code, error = error.Message })); }
            catch (IOException) { /* The application has already closed its redirected pipe. */ }
            if (coordinated) MessageBox(IntPtr.Zero, error.Message, "EWAF update could not be installed", 0x10);
            return 1;
        }
    }

    private static void VerifyPublisher(string path, string publisher)
    {
        var file = new WinTrustFileInfo { Size = (uint)Marshal.SizeOf<WinTrustFileInfo>(), Path = path };
        IntPtr filePointer = Marshal.AllocHGlobal(Marshal.SizeOf<WinTrustFileInfo>());
        Marshal.StructureToPtr(file, filePointer, false);
        var data = new WinTrustData
        {
            Size = (uint)Marshal.SizeOf<WinTrustData>(), UIChoice = 2, RevocationChecks = 1,
            UnionChoice = 1, File = filePointer, StateAction = 1, ProviderFlags = 0x80
        };
        Guid policy = new("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");
        try
        {
            if (WinVerifyTrust(new IntPtr(-1), ref policy, ref data) != 0)
                throw new VerificationException("signature", "Update publisher signature is invalid.");
            using var certificate = new X509Certificate2(X509Certificate.CreateFromSignedFile(path));
            if (string.IsNullOrEmpty(publisher) || certificate.Subject != publisher)
                throw new VerificationException("publisher", "Update publisher does not match the enrolled identity.");
        }
        finally
        {
            data.StateAction = 2;
            WinVerifyTrust(new IntPtr(-1), ref policy, ref data);
            Marshal.DestroyStructure<WinTrustFileInfo>(filePointer);
            Marshal.FreeHGlobal(filePointer);
        }
    }

    private static void VerifyIdentity(string path, string version, string architecture)
    {
        CheckMsi(MsiOpenDatabase(path, IntPtr.Zero, out uint database));
        try
        {
            CheckMsi(MsiGetSummaryInformation(database, null, 0, out uint summary));
            try
            {
                var value = new StringBuilder(1024); uint length = 1023;
                CheckMsi(MsiSummaryInfoGetProperty(summary, 7, out _, out _, IntPtr.Zero, value, ref length));
                if (architecture is not ("x64" or "arm64") || !value.ToString().Split(';')[0].Equals(architecture, StringComparison.OrdinalIgnoreCase))
                    throw new VerificationException("identity", "MSI architecture mismatch.");
            }
            finally { MsiCloseHandle(summary); }
            if (!Property(database, "UpgradeCode").Equals("{33C1F535-C25C-48C6-BDE6-F186ABFCB085}", StringComparison.OrdinalIgnoreCase) ||
                Property(database, "ProductName") != "EWAF" || Property(database, "ProductVersion") != version ||
                Property(database, "Manufacturer") != "TLO Labs")
                throw new VerificationException("identity", "MSI belongs to another application or version.");
        }
        finally { MsiCloseHandle(database); }
    }

    private static string Property(uint database, string name)
    {
        CheckMsi(MsiDatabaseOpenView(database, $"SELECT `Value` FROM `Property` WHERE `Property`='{name}'", out uint view));
        try
        {
            CheckMsi(MsiViewExecute(view, 0));
            CheckMsi(MsiViewFetch(view, out uint record));
            try
            {
                var value = new StringBuilder(1024); uint length = 1023;
                CheckMsi(MsiRecordGetString(record, 1, value, ref length));
                return value.ToString();
            }
            finally { MsiCloseHandle(record); }
        }
        finally { MsiCloseHandle(view); }
    }
    private static void CheckMsi(uint result)
    {
        if (result != 0) throw new VerificationException("identity", $"Could not read MSI identity ({result}).");
    }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct WinTrustFileInfo { internal uint Size; [MarshalAs(UnmanagedType.LPWStr)] internal string Path; internal IntPtr File, KnownSubject; }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct WinTrustData
    {
        internal uint Size; internal IntPtr PolicyCallback, SIPClient; internal uint UIChoice, RevocationChecks, UnionChoice;
        internal IntPtr File; internal uint StateAction; internal IntPtr StateData, URLReference;
        internal uint ProviderFlags, UIContext; internal IntPtr SignatureSettings;
    }
    [DllImport("wintrust.dll", ExactSpelling = true)] private static extern int WinVerifyTrust(IntPtr window, ref Guid policy, ref WinTrustData data);
    [DllImport("msi.dll", CharSet = CharSet.Unicode, EntryPoint = "MsiOpenDatabaseW")] private static extern uint MsiOpenDatabase(string path, IntPtr persist, out uint database);
    [DllImport("msi.dll", CharSet = CharSet.Unicode, EntryPoint = "MsiDatabaseOpenViewW")] private static extern uint MsiDatabaseOpenView(uint database, string query, out uint view);
    [DllImport("msi.dll")] private static extern uint MsiViewExecute(uint view, uint record);
    [DllImport("msi.dll")] private static extern uint MsiViewFetch(uint view, out uint record);
    [DllImport("msi.dll", CharSet = CharSet.Unicode, EntryPoint = "MsiRecordGetStringW")] private static extern uint MsiRecordGetString(uint record, uint field, StringBuilder value, ref uint length);
    [DllImport("msi.dll", CharSet = CharSet.Unicode, EntryPoint = "MsiGetSummaryInformationW")] private static extern uint MsiGetSummaryInformation(uint database, string? path, uint updateCount, out uint summary);
    [DllImport("msi.dll", CharSet = CharSet.Unicode, EntryPoint = "MsiSummaryInfoGetPropertyW")] private static extern uint MsiSummaryInfoGetProperty(uint summary, uint property, out uint type, out int integer, IntPtr time, StringBuilder value, ref uint length);
    [DllImport("msi.dll")] private static extern uint MsiCloseHandle(uint handle);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int MessageBox(IntPtr window, string text, string caption, uint type);
}
