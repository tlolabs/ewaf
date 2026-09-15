import unittest
from datetime import date

from ewaf import parse_date


class ParseDateTests(unittest.TestCase):
    def test_parses_displayed_format_and_surrounding_whitespace(self):
        self.assertEqual(parse_date(" 02-29-2024 "), date(2024, 2, 29))

    def test_rejects_invalid_calendar_date(self):
        with self.assertRaises(ValueError):
            parse_date("02-29-2025")


if __name__ == "__main__":
    unittest.main()
