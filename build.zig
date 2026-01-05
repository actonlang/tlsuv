const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const t = target.result;

    const enable_http = b.option(bool, "http", "enable HTTP/websocket support") orelse false;
    const enable_keychain = b.option(bool, "keychain", "enable keychain support") orelse true;
    const tls_lib = b.option([]const u8, "tlslib", "TLS implementation library (openssl|mbedtls)") orelse "mbedtls";

    const use_openssl = std.mem.eql(u8, tls_lib, "openssl");
    const use_mbedtls = std.mem.eql(u8, tls_lib, "mbedtls");
    if (!use_openssl and !use_mbedtls) {
        @panic("Unsupported TLS library");
    }

    const dep_libuv = b.dependency("libuv", .{
        .target = target,
        .optimize = optimize,
    });

    const dep_libmbedtls = if (use_mbedtls) b.dependency("libmbedtls", .{
        .target = target,
        .optimize = optimize,
    }) else null;

    const lib = b.addStaticLibrary(.{
        .name = "tlsuv",
        .target = target,
        .optimize = optimize,
    });

    var cflags = std.ArrayList([]const u8).init(b.allocator);
    defer cflags.deinit();
    cflags.append("-std=c99") catch unreachable;

    const base_sources = [_][]const u8{
        "src/tlsuv.c",
        "src/um_debug.c",
        "src/base64.c",
        "src/tls_engine.c",
        "src/p11.c",
        "src/socket.c",
        "src/connector.c",
        "src/alloc.c",
        "src/keychain.c",
        "src/url.c",
    };

    lib.addCSourceFiles(.{
        .files = &base_sources,
        .flags = cflags.items,
    });

    if (enable_keychain) {
        if (t.isDarwin()) {
            lib.addCSourceFile(.{
                .file = b.path("src/apple/keychain.c"),
                .flags = cflags.items,
            });
            lib.linkFramework("CoreFoundation");
            lib.linkFramework("Security");
        } else if (t.os.tag == .windows) {
            lib.addCSourceFile(.{
                .file = b.path("src/win32/win32_keychain.c"),
                .flags = cflags.items,
            });
            lib.linkSystemLibrary("crypt32");
            lib.linkSystemLibrary("ncrypt");
        }
    }

    if (enable_http) {
        const http_sources = [_][]const u8{
            "src/http.c",
            "src/websocket.c",
            "src/http_req.c",
            "src/tls_link.c",
            "src/compression.c",
        };
        lib.addCSourceFiles(.{
            .files = &http_sources,
            .flags = cflags.items,
        });

        const uv_link_sources = [_][]const u8{
            "deps/uv_link_t/src/defaults.c",
            "deps/uv_link_t/src/uv_link_t.c",
            "deps/uv_link_t/src/uv_link_source_t.c",
            "deps/uv_link_t/src/uv_link_observer_t.c",
        };
        lib.addCSourceFiles(.{
            .files = &uv_link_sources,
            .flags = cflags.items,
        });

        lib.addIncludePath(b.path("deps/uv_link_t/include"));
        lib.addIncludePath(b.path("deps/uv_link_t"));
        lib.linkSystemLibrary("z");
        lib.linkSystemLibrary("llhttp");
        lib.defineCMacro("TLSUV_HTTP", null);
    }

    if (use_openssl) {
        const ssl_sources = [_][]const u8{
            "src/openssl/engine.c",
            "src/openssl/keys.c",
        };
        lib.addCSourceFiles(.{
            .files = &ssl_sources,
            .flags = cflags.items,
        });
        lib.linkSystemLibrary("ssl");
        lib.linkSystemLibrary("crypto");
        lib.defineCMacro("USE_OPENSSL", null);
        lib.defineCMacro("TLS_IMPL", "openssl");
    } else if (use_mbedtls) {
        const ssl_sources = [_][]const u8{
            "src/mbedtls/engine.c",
            "src/mbedtls/keys.c",
            "src/mbedtls/mbed_p11.c",
            "src/mbedtls/p11_ecdsa.c",
            "src/mbedtls/p11_rsa.c",
        };
        lib.addCSourceFiles(.{
            .files = &ssl_sources,
            .flags = cflags.items,
        });
        if (dep_libmbedtls) |mbedtls| {
            lib.addIncludePath(mbedtls.path("include"));
            lib.linkLibrary(mbedtls.artifact("mbedcrypto"));
            lib.linkLibrary(mbedtls.artifact("mbedtls"));
            lib.linkLibrary(mbedtls.artifact("mbedx509"));
        }
        lib.defineCMacro("USE_MBEDTLS", null);
        lib.defineCMacro("TLS_IMPL", "mbedtls");
    }

    lib.addIncludePath(b.path("include"));
    lib.addIncludePath(b.path("src"));

    lib.linkLibrary(dep_libuv.artifact("uv"));
    lib.linkLibC();

    if (t.os.tag == .windows) {
        lib.defineCMacro("WIN32_LEAN_AND_MEAN", null);
        lib.defineCMacro("WINVER", "0x0A00");
        lib.defineCMacro("_WIN32_WINNT", "0x0A00");
        lib.defineCMacro("_CRT_SECURE_NO_WARNINGS", null);
        lib.defineCMacro("_CRT_NONSTDC_NO_DEPRECATE", null);
        lib.defineCMacro("_WINSOCK_DEPRECATED_NO_WARNINGS", null);
    }

    if (t.os.tag == .linux) {
        lib.defineCMacro("_POSIX_C_SOURCE", "200112");
        lib.defineCMacro("_GNU_SOURCE", null);
    }

    lib.defineCMacro("TLSUV_VERSION", "v0.0.0");

    lib.installHeadersDirectory(b.path("include/tlsuv"), "tlsuv", .{});
    b.installArtifact(lib);
}
