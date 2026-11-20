#!/usr/bin/env python3
"""Print a libretro core's name, version and declared options as JSON.

Loads the core with ctypes and calls only retro_get_system_info and
retro_set_environment; no content is loaded and no emulator runs. run.py
invokes this in a child process so a misbehaving core cannot take the sweep
down with it. Handles core options v0 (SET_VARIABLES), v1 and v2.
"""

from __future__ import annotations

import ctypes as c
import json
import sys
from typing import Any

GET_CORE_OPTIONS_VERSION = 52
SET_VARIABLES = 16
SET_CORE_OPTIONS = 53
SET_CORE_OPTIONS_INTL = 54
SET_CORE_OPTIONS_V2 = 67
SET_CORE_OPTIONS_V2_INTL = 68
GET_LANGUAGE = 39


class Value(c.Structure):
    _fields_ = [("value", c.c_char_p), ("label", c.c_char_p)]


class DefinitionV1(c.Structure):
    _fields_ = [("key", c.c_char_p), ("desc", c.c_char_p), ("info", c.c_char_p),
                ("values", Value * 128), ("default_value", c.c_char_p)]


class DefinitionV2(c.Structure):
    _fields_ = [("key", c.c_char_p), ("desc", c.c_char_p), ("desc_categorized", c.c_char_p),
                ("info", c.c_char_p), ("info_categorized", c.c_char_p), ("category_key", c.c_char_p),
                ("values", Value * 128), ("default_value", c.c_char_p)]


class OptionsV2(c.Structure):
    _fields_ = [("categories", c.c_void_p), ("definitions", c.POINTER(DefinitionV2))]


class IntlV1(c.Structure):
    _fields_ = [("us", c.POINTER(DefinitionV1)), ("local", c.POINTER(DefinitionV1))]


class IntlV2(c.Structure):
    _fields_ = [("us", c.POINTER(OptionsV2)), ("local", c.POINTER(OptionsV2))]


class Variable(c.Structure):
    _fields_ = [("key", c.c_char_p), ("value", c.c_char_p)]


class SystemInfo(c.Structure):
    _fields_ = [("library_name", c.c_char_p), ("library_version", c.c_char_p),
                ("valid_extensions", c.c_char_p), ("need_fullpath", c.c_bool), ("block_extract", c.c_bool)]


def text(value: bytes | None) -> str | None:
    return value.decode(errors="replace") if value is not None else None


def query(core_path: str) -> dict[str, Any]:
    options: dict[str, dict[str, Any]] = {}

    def definitions(pointer: Any) -> None:
        index = 0
        while pointer and pointer[index].key:
            item = pointer[index]
            values = []
            for value in item.values:
                if not value.value:
                    break
                values.append(text(value.value))
            options[text(item.key)] = {"default": text(item.default_value) or (values[0] if values else None),
                                       "values": values}
            index += 1

    @c.CFUNCTYPE(c.c_bool, c.c_uint, c.c_void_p)
    def environment(cmd: int, data: int | None) -> bool:
        cmd &= 0xFFFF  # strip RETRO_ENVIRONMENT_EXPERIMENTAL/PRIVATE
        if cmd == GET_CORE_OPTIONS_VERSION and data:
            c.cast(data, c.POINTER(c.c_uint))[0] = 2
            return True
        if cmd == GET_LANGUAGE and data:
            c.cast(data, c.POINTER(c.c_uint))[0] = 0
            return True
        if cmd == SET_CORE_OPTIONS_V2 and data:
            definitions(c.cast(data, c.POINTER(OptionsV2)).contents.definitions)
            return True
        if cmd == SET_CORE_OPTIONS_V2_INTL and data:
            definitions(c.cast(data, c.POINTER(IntlV2)).contents.us.contents.definitions)
            return True
        if cmd == SET_CORE_OPTIONS and data:
            definitions(c.cast(data, c.POINTER(DefinitionV1)))
            return True
        if cmd == SET_CORE_OPTIONS_INTL and data:
            definitions(c.cast(data, c.POINTER(IntlV1)).contents.us)
            return True
        if cmd == SET_VARIABLES and data:
            pointer = c.cast(data, c.POINTER(Variable))
            index = 0
            while pointer[index].key:
                # "Description; first|second|third", first is the default.
                choices = text(pointer[index].value).split("; ", 1)[-1].split("|")
                options[text(pointer[index].key)] = {"default": choices[0], "values": choices}
                index += 1
            return True
        return False

    core = c.CDLL(core_path)
    info = SystemInfo()
    core.retro_get_system_info.argtypes = [c.POINTER(SystemInfo)]
    core.retro_get_system_info(c.byref(info))
    library = {"name": text(info.library_name), "version": text(info.library_version)}
    core.retro_set_environment.argtypes = [type(environment)]
    core.retro_set_environment(environment)
    return {"library": library, "options": options}


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: core_info.py CORE.so")
    print(json.dumps(query(sys.argv[1]), sort_keys=True))
