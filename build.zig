const std = @import("std");
const print = @import("std").debug.print;

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});
    const target = b.standardTargetOptions(.{});

    const lib = b.addStaticLibrary(.{
        .name = "tlsuv",
        .target = target,
        .optimize = optimize,
    });

    var flags = std.ArrayList([]const u8).init(b.allocator);
    defer flags.deinit();

    flags.append("-DUSE_MBEDTLS") catch unreachable;
    flags.append("-DTLSUV_TLSLIB=mbedtls") catch unreachable;

    flags.append("-D_POSIX_C_SOURCE=200112") catch unreachable;
    flags.append("-D_GNU_SOURCE") catch unreachable;

    lib.addCSourceFiles(.{
        .files = &.{
            "src/common.c",
            "src/tlsuv.c",
            "src/bio.c",
            //        "src/http.c",
            "src/tcp_src.c",
            "src/um_debug.c",
            //        "src/websocket.c",
            //        "src/http_req.c",
            "src/tls_link.c",
            "src/base64.c",
            "src/tls_engine.c",
            //        "src/compression.c",
            //        "src/compression.h",
            "src/p11.c",
        },
        .flags = flags.items
    });

    // Hard code to use MbedTLS
    lib.addCSourceFiles(.{
        .files = &.{
            "src/mbedtls/engine.c",
            "src/mbedtls/keys.c",
            "src/mbedtls/mbed_p11.c",
            "src/mbedtls/p11_ecdsa.c",
            "src/mbedtls/p11_rsa.c"
        },
        .flags = flags.items
    });

    // Just bake uv_link-t into same output archive for now
    lib.addCSourceFiles(.{
        .files = &.{
            "deps/uv_link_t/src/defaults.c",
            "deps/uv_link_t/src/uv_link_t.c",
            "deps/uv_link_t/src/uv_link_source_t.c",
        },
        .flags = flags.items
    });

    // Just bake llhttp into same output archive for now
    lib.addCSourceFiles(.{
        .files = &.{
            "deps/llhttp/src/llhttp.c",
        },
        .flags = flags.items
    });

    lib.addIncludePath(b.path("deps/llhttp/include"));
    lib.addIncludePath(b.path("deps/uv_link_t/include"));
    lib.addIncludePath(b.path("deps/uv_link_t"));
    lib.addIncludePath(b.path("src"));
    lib.addIncludePath(b.path("include"));
    lib.addIncludePath(b.path("../../depsout/include"));
    lib.installHeader(b.path("deps/uv_link_t/include/uv_link_t.h"), "uv_link_t.h");
    lib.installHeadersDirectory(b.path("include/tlsuv"), "tlsuv", .{});
    lib.linkLibC();

    b.installArtifact(lib);
}
