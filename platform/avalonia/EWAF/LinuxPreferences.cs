// SPDX-License-Identifier: GPL-3.0-or-later
using System.Runtime.InteropServices;
namespace EWAF;
// Migration-only OS adapter. Reads the existing dconf database without a GTK schema,
// a desktop process, or a dependency on the optional dconf command-line package.
internal static class LinuxPreferences
{
    [DllImport("libdconf.so.1")] private static extern IntPtr dconf_client_new();
    [DllImport("libdconf.so.1")] private static extern IntPtr dconf_client_read(IntPtr client, [MarshalAs(UnmanagedType.LPUTF8Str)] string key);
    [DllImport("libglib-2.0.so.0")] private static extern IntPtr g_variant_print(IntPtr value, bool annotate);
    [DllImport("libglib-2.0.so.0")] private static extern void g_variant_unref(IntPtr value);
    [DllImport("libglib-2.0.so.0")] private static extern void g_free(IntPtr value);
    [DllImport("libgobject-2.0.so.0")] private static extern void g_object_unref(IntPtr value);
    internal static string? Read(string key)
    {
        if (!OperatingSystem.IsLinux()) return null;
        try
        {
            var client = dconf_client_new();
            if (client == IntPtr.Zero) return null;
            try
            {
                var value = dconf_client_read(client, "/com/tlolabs/ewaf/" + key);
                if (value == IntPtr.Zero) return null;
                try
                {
                    var printed = g_variant_print(value, false);
                    try { return Marshal.PtrToStringUTF8(printed); }
                    finally { g_free(printed); }
                }
                finally { g_variant_unref(value); }
            }
            finally { g_object_unref(client); }
        }
        catch (DllNotFoundException) { return null; } // New installs on desktops without dconf have no old preferences.
    }
}
