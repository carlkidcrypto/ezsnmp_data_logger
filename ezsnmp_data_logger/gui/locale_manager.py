"""Locale and internationalization manager for ezsnmp_data_logger GUIs.

Loads `locale.json` to externalize all user-facing UI strings (titles, buttons,
tabs, cards, sections, table headers, labels, and dialog messages) for
effortless customization and multi-language deployment across desktop and web.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

DEFAULT_LOCALE_PATH = Path(__file__).resolve().parent / "locale.json"


class LocaleManager:
    """Manages string resolution, language selection, and fallback for GUIs."""

    def __init__(
        self,
        locale: str = "en",
        locale_file: str | Path | None = None,
    ) -> None:
        self.locale = (locale or "en").strip().lower()
        self.locale_file = Path(locale_file).resolve() if locale_file else DEFAULT_LOCALE_PATH
        self._catalog: dict[str, Any] = {}
        self.reload()

    def reload(self) -> None:
        """Load or reload locale definitions from JSON file."""
        if not self.locale_file.is_file():
            # Fall back to built-in default locale if custom file is missing
            self.locale_file = DEFAULT_LOCALE_PATH

        try:
            self._catalog = json.loads(self.locale_file.read_text(encoding="utf-8"))
        except Exception:
            if self.locale_file != DEFAULT_LOCALE_PATH and DEFAULT_LOCALE_PATH.is_file():
                self._catalog = json.loads(DEFAULT_LOCALE_PATH.read_text(encoding="utf-8"))
            else:
                self._catalog = {"default_locale": "en", "locales": {"en": {}}}

    def supported_locales(self) -> list[str]:
        """Return list of supported locale codes in the loaded catalog."""
        locales = self._catalog.get("locales", {})
        if isinstance(locales, dict):
            return sorted(locales.keys())
        return ["en"]

    def set_locale(self, locale: str) -> None:
        """Switch active locale (e.g. 'en', 'es')."""
        if locale:
            self.locale = locale.strip().lower()

    def _resolve_nested(self, container: Any, path: str) -> Any | None:
        parts = path.split(".")
        curr = container
        for p in parts:
            if isinstance(curr, dict) and p in curr:
                curr = curr[p]
            else:
                return None
        return curr

    def t(self, key: str, default: str | None = None, **kwargs: Any) -> str:
        """Translate a dotted key into the active locale with keyword formatting.

        Falls back to 'en' if not found in active locale, or default / key.
        """
        locales = self._catalog.get("locales", {})
        active_dict = locales.get(self.locale) if isinstance(locales, dict) else None
        val = self._resolve_nested(active_dict, key) if active_dict else None

        # Fallback to default/en locale
        if val is None and self.locale != "en":
            en_dict = locales.get("en") if isinstance(locales, dict) else None
            val = self._resolve_nested(en_dict, key) if en_dict else None

        if val is None:
            val = default if default is not None else key

        if isinstance(val, str) and kwargs:
            try:
                return val.format(**kwargs)
            except Exception:
                return val
        return str(val) if val is not None else ""

    def get_locale_dict(self, locale: str | None = None) -> dict[str, Any]:
        """Return dictionary of all strings for a locale (falling back to English)."""
        loc = (locale or self.locale).strip().lower()
        locales = self._catalog.get("locales", {})
        base_en = locales.get("en", {}) if isinstance(locales, dict) else {}
        target = locales.get(loc, {}) if isinstance(locales, dict) else {}

        # Merge target over base English
        merged: dict[str, Any] = {}
        for section, sec_val in base_en.items():
            if isinstance(sec_val, dict):
                merged[section] = dict(sec_val)
                target_sec = target.get(section, {})
                if isinstance(target_sec, dict):
                    merged[section].update(target_sec)
            else:
                merged[section] = target.get(section, sec_val)
        return merged


def get_default_locale_manager() -> LocaleManager:
    """Return a singleton or standard LocaleManager instance."""
    return LocaleManager(locale="en")
