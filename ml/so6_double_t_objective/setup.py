
from __future__ import annotations

import os
from pathlib import Path
from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

import pybind11


HERE = Path(__file__).resolve().parent
REPO_ENV = os.environ.get("EXACT_SYNTHESIS_DIR")
if not REPO_ENV:
    raise RuntimeError("Set EXACT_SYNTHESIS_DIR to the Exact-Synthesis repo root before building.")

REPO = Path(REPO_ENV).resolve()
if not REPO.exists():
    raise RuntimeError(f"EXACT_SYNTHESIS_DIR does not exist: {REPO}")

debug = os.environ.get("READONLY_LUT_DEBUG", "").strip() == "1"
asan = os.environ.get("READONLY_LUT_ASAN", "").strip() == "1"
ubsan = os.environ.get("READONLY_LUT_UBSAN", "").strip() == "1"
disable_indicators = os.environ.get("READONLY_LUT_DISABLE_INDICATORS", "").strip() == "1"

sources = [
    str(HERE / "readonly_lut_exact_synthesis.cpp"),
    str(REPO / "src" / "SO6.cpp"),
    str(REPO / "src" / "algo" / "Canonicalizer.cpp"),
    str(REPO / "src" / "Globals.cpp"),
    str(REPO / "src" / "algo" / "Generate.cpp"),
    str(REPO / "src" / "T_Operator.cpp"),
    str(REPO / "src" / "TT_Operator.cpp"),
    str(REPO / "src" / "MITM.cpp"),
]

include_dirs: list[str] = []
system_include_dirs = [
    pybind11.get_include(),
    str(REPO),
    str(REPO / "include"),
    str(REPO / "include" / "third_party"),
]

extra_compile_args = ["-std=c++20"]
extra_link_args = []
extra_compile_args += [f"-isystem{path}" for path in system_include_dirs]
ml_warning_args = [
    "-Wextra",
    "-Wpedantic",
    "-Wunused-function",
]

if debug:
    extra_compile_args += ["-O0", "-g3", "-fno-omit-frame-pointer"]
else:
    extra_compile_args += [
        "-O3",
        "-DNDEBUG",
        "-march=native",
        "-funroll-loops",
        "-fomit-frame-pointer",
        "-fno-semantic-interposition",
    ]

if disable_indicators:
    extra_compile_args.append("-DEXACT_DISABLE_INDICATORS")

if asan:
    extra_compile_args += ["-fsanitize=address", "-fno-omit-frame-pointer"]
    extra_link_args += ["-fsanitize=address"]

if ubsan:
    extra_compile_args += ["-fsanitize=undefined", "-fno-omit-frame-pointer"]
    extra_link_args += ["-fsanitize=undefined"]

print("== readonly_lut build configuration ==")
print(f"repo                 : {REPO}")
print(f"debug                : {debug}")
print(f"asan                 : {asan}")
print(f"ubsan                : {ubsan}")
print(f"disable_indicators   : {disable_indicators}")
print(f"compile args         : {' '.join(extra_compile_args)}")
print(f"ml warning args      : {' '.join(ml_warning_args)}")
if not debug:
    extra_link_args += ["-Wl,-O3", "-Wl,--as-needed"]
if extra_link_args:
    print(f"link args            : {' '.join(extra_link_args)}")
print()


class MlOnlyWarningsBuildExt(build_ext):
    """Enable warnings only on the ML binding source for this extension build."""

    def build_extensions(self) -> None:
        compiler = self.compiler
        original_compile = compiler._compile
        ml_source = os.path.normpath(str(HERE / "readonly_lut_exact_synthesis.cpp"))

        def compile_with_ml_warning_scope(obj, src, ext, cc_args, extra_postargs, pp_opts):
            scoped_postargs = list(extra_postargs or [])
            if os.path.normpath(src) == ml_source:
                scoped_postargs.extend(ml_warning_args)
            else:
                scoped_postargs.append("-w")
            return original_compile(obj, src, ext, cc_args, scoped_postargs, pp_opts)

        compiler._compile = compile_with_ml_warning_scope
        try:
            super().build_extensions()
        finally:
            compiler._compile = original_compile

ext_modules = [
    Extension(
        name="readonly_lut",
        sources=sources,
        include_dirs=include_dirs,
        language="c++",
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args,
        libraries=["tbb"],
    )
]

setup(
    name="readonly_lut",
    version="0.5.0",
    description="Debuggable read-only pybind bridge from Exact-Synthesis LUTs to Python",
    ext_modules=ext_modules,
    cmdclass={"build_ext": MlOnlyWarningsBuildExt},
)
