import tempfile
import unittest
from datetime import date
from pathlib import Path

from ewaf_core import FolderCreationError, create_folders, generate_folder_dates


class GenerateFolderDatesTests(unittest.TestCase):
    def test_generates_matching_dates_with_inclusive_boundaries(self):
        self.assertEqual(
            generate_folder_dates(date(2026, 9, 3), date(2026, 9, 17), 3),
            ["09-03-2026", "09-10-2026", "09-17-2026"],
        )

    def test_advances_to_first_matching_weekday(self):
        self.assertEqual(
            generate_folder_dates(date(2026, 9, 4), date(2026, 9, 10), 3),
            ["09-10-2026"],
        )

    def test_returns_empty_list_when_range_has_no_matching_weekday(self):
        self.assertEqual(
            generate_folder_dates(date(2026, 9, 4), date(2026, 9, 5), 3),
            [],
        )

    def test_rejects_reversed_range(self):
        with self.assertRaisesRegex(ValueError, "on or before"):
            generate_folder_dates(date(2026, 9, 5), date(2026, 9, 4), 3)

    def test_rejects_invalid_weekday(self):
        for weekday in (-1, 7):
            with self.subTest(weekday=weekday), self.assertRaises(ValueError):
                generate_folder_dates(date(2026, 9, 4), date(2026, 9, 5), weekday)

    def test_handles_maximum_date_without_overflow(self):
        self.assertEqual(
            generate_folder_dates(date.max, date.max, date.max.weekday()),
            ["12-31-9999"],
        )
        self.assertEqual(
            generate_folder_dates(date.max, date.max, (date.max.weekday() + 1) % 7),
            [],
        )


class CreateFoldersTests(unittest.TestCase):
    def test_reports_created_and_existing_directories(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            base = Path(temporary_directory)
            (base / "09-03-2026").mkdir()

            result = create_folders(base, ["09-03-2026", "09-10-2026"])

            self.assertEqual(result.existing, (base / "09-03-2026",))
            self.assertEqual(result.created, (base / "09-10-2026",))
            self.assertTrue((base / "09-10-2026").is_dir())

    def test_file_collision_is_reported_with_partial_progress(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            base = Path(temporary_directory)
            (base / "collision").write_text("not a directory")

            with self.assertRaises(FolderCreationError) as caught:
                create_folders(base, ["created", "collision", "not-created"])

            self.assertEqual(caught.exception.result.processed_count, 1)
            self.assertEqual(caught.exception.failed_path, base / "collision")
            self.assertTrue((base / "created").is_dir())
            self.assertFalse((base / "not-created").exists())

    def test_rejects_names_outside_base_directory(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            base = Path(temporary_directory)
            with self.assertRaises(ValueError):
                create_folders(base, ["safe", "../escaped"])
            self.assertFalse((base / "safe").exists())

    def test_requires_an_existing_base_directory(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            missing = Path(temporary_directory) / "missing"
            with self.assertRaises(NotADirectoryError):
                create_folders(missing, ["09-03-2026"])


if __name__ == "__main__":
    unittest.main()
