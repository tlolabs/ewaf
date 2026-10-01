# Proposed additional permission — awaiting owner approval

This is a proposal, not an operative license grant. The repository remains GPL-3.0-or-later.

## Proposed text

As an additional permission under section 7 of the GNU General Public License, version 3, Thomas Lothian permits the EWAF code for which he holds copyright to be linked or combined with Avalonia and its Silverlight-derived components licensed under the Microsoft Public License (Ms-PL), and permits distribution of the resulting combination. All other requirements of GPL-3.0-or-later for the covered EWAF code remain in effect. This permission does not relicense any third-party code; its applicable license terms and notices must be preserved. Modified versions may extend this permission to their modifications, but are not required to do so.

## Evidence and scope

Avalonia 12.1.3 NuGet metadata declares MIT, but the pinned upstream source at `8eeda4f6f546165b3f72e63c9f42247abb306905` identifies Calendar, CalendarDatePicker, AutoCompleteBox and selection adapters as Ms-PL. These are inside the shipped Avalonia.Controls assembly; avoiding a calendar control alone does not remove them. The complete upstream NOTICE is retained in `licenses/avalonia/Avalonia-NOTICE.md`.

- [Exact CalendarDatePicker source](https://github.com/AvaloniaUI/Avalonia/blob/8eeda4f6f546165b3f72e63c9f42247abb306905/src/Avalonia.Controls/CalendarDatePicker/CalendarDatePicker.cs)
- [Exact upstream notice](https://github.com/AvaloniaUI/Avalonia/blob/8eeda4f6f546165b3f72e63c9f42247abb306905/NOTICE.md)
- [GNU assessment of Ms-PL](https://www.gnu.org/licenses/license-list.html#ms-pl)

The owner must approve any additional grant. No third-party GPL code can be covered by this permission. The Rust dependency inventory must continue to be reviewed independently. This draft does not clear the existing production signing/update enrollment gates.
