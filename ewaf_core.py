"""Core date generation and folder creation logic for E.W.A.F."""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
from datetime import date, timedelta
from os import PathLike
from pathlib import Path

DATE_FORMAT = "%m-%d-%Y"
WEEKDAYS = (
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    "Sunday",
)


@dataclass(frozen=True)
class FolderCreationResult:
    """Paths created by an operation and paths that already existed."""

    created: tuple[Path, ...]
    existing: tuple[Path, ...]

    @property
    def processed_count(self) -> int:
        return len(self.created) + len(self.existing)


class FolderCreationError(OSError):
    """An I/O failure with the safely completed portion of the operation."""

    def __init__(
        self,
        failed_path: Path,
        result: FolderCreationResult,
        cause: OSError,
    ) -> None:
        super().__init__(cause.errno, cause.strerror or str(cause), str(failed_path))
        self.failed_path = failed_path
        self.result = result


def generate_folder_dates(start_date: date, end_date: date, weekday: int) -> list[str]:
    """Return inclusive weekly folder names for ``weekday`` within a date range."""

    if not 0 <= weekday < len(WEEKDAYS):
        raise ValueError("weekday must be an integer from 0 (Monday) to 6 (Sunday)")
    if start_date > end_date:
        raise ValueError("start_date must be on or before end_date")

    days_until_weekday = (weekday - start_date.weekday()) % 7
    if days_until_weekday > (end_date - start_date).days:
        return []
    current_date = start_date + timedelta(days=days_until_weekday)
    folders_to_create: list[str] = []

    while current_date <= end_date:
        folders_to_create.append(current_date.strftime(DATE_FORMAT))
        if (end_date - current_date).days < 7:
            break
        current_date += timedelta(days=7)

    return folders_to_create


def create_folders(
    base_path: str | PathLike[str], folder_names: Iterable[str]
) -> FolderCreationResult:
    """Create direct child folders and report new and existing directories."""

    base_directory = Path(base_path)
    if not base_directory.is_dir():
        raise NotADirectoryError(f"Base directory does not exist: {base_directory}")

    validated_names = tuple(folder_names)
    for folder_name in validated_names:
        if (
            not folder_name
            or Path(folder_name).name != folder_name
            or folder_name in {".", ".."}
        ):
            raise ValueError(
                f"Folder name must be a direct child name: {folder_name!r}"
            )

    created: list[Path] = []
    existing: list[Path] = []

    for folder_name in validated_names:
        path = base_directory / folder_name
        try:
            path.mkdir()
        except FileExistsError as error:
            if path.is_dir():
                existing.append(path)
                continue
            result = FolderCreationResult(tuple(created), tuple(existing))
            raise FolderCreationError(path, result, error) from error
        except OSError as error:
            result = FolderCreationResult(tuple(created), tuple(existing))
            raise FolderCreationError(path, result, error) from error
        else:
            created.append(path)

    return FolderCreationResult(tuple(created), tuple(existing))
