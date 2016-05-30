package(default_visibility = ["//visibility:public"])

config_setting(
    name = "is_osx",
    values = {"cpu": "darwin"}
)

cc_library(
    name = "main",
    srcs = select({
        ":is_osx": ["lib/libprotobuf.10.dylib"],
        "//conditions:default": ["lib/libprotobuf.so.10"]
    }),
    hdrs = glob([
        "include/**/*.h",
        "include/**/*.hpp",
        "include/**/*.inl",
    ]),
    includes = ["include/"],
    linkopts = ["-Wl,-rpath,/usr/local/lib"],
)
