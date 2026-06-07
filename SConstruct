#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

# For reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp") + Glob("src/**/*.cpp") + Glob("src/**/**/*.cpp")

# Exclude the generated doc source from the globbed sources; it is added
# explicitly below (and only for editor/debug targets). Without this the
# recursive glob also matches src/gen/doc_data.gen.cpp once it exists on disk,
# and the file gets linked twice (LNK4042).
sources = [s for s in sources if "/gen/" not in str(s).replace("\\", "/")]

# Compile the class-reference XML in doc_classes/ into the library so the
# descriptions show up in the Godot editor (Ctrl+click a class / F1 Help).
# Only editor + debug template builds carry docs.
if env["target"] in ["editor", "template_debug"]:
    doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
    sources.append(doc_data)

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "godot-fsw-tcp/bin/libsim.{}.{}.framework/libsim.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
elif env["platform"] == "ios":
    if env["ios_simulator"]:
        library = env.StaticLibrary(
            "godot-fsw-tcp/bin/libsim.{}.{}.simulator.a".format(env["platform"], env["target"]),
            source=sources,
        )
    else:
        library = env.StaticLibrary(
            "godot-fsw-tcp/bin/libsim.{}.{}.a".format(env["platform"], env["target"]),
            source=sources,
        )
else:
    library = env.SharedLibrary(
        "godot-fsw-tcp/bin/libsim{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
