const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const t = target.result;
    const optimize = b.standardOptimizeOption(.{});

    // Read version from file
    const version = "0.0.1"; // TODO: implement version.txt reading

    // Options
    const enable_http = b.option(
        bool,
        "http",
        "enable HTTP/websocket support",
    ) orelse true;

    const tls_lib = b.option(
        []const u8,
        "tlslib",
        "TLS implementation library (openssl|mbedtls)",
    ) orelse "mbedtls";

    if (!std.mem.eql(u8, tls_lib, "openssl") and !std.mem.eql(u8, tls_lib, "mbedtls")) {
        @panic("Unsupported TLS library");
    }

    const use_openssl = std.mem.eql(u8, tls_lib, "openssl");
    const use_mbedtls = std.mem.eql(u8, tls_lib, "mbedtls");

    const enable_keychain = b.option(
        bool,
        "keychain",
        "enable keychain support on platforms that support it",
    ) orelse true;

    // Get dependencies
    const dep_libuv = b.dependency("libuv", .{
        .target = target,
        .optimize = optimize,
    });

    const dep_libmbedtls = if (use_mbedtls) b.dependency("libmbedtls", .{
        .target = target,
        .optimize = optimize,
    }) else null;

    // Create static library
    const lib = b.addStaticLibrary(.{
        .name = "tlsuv",
        .target = target,
        .optimize = optimize,
    });

    // Base source files
    const sources = [_][]const u8{
        "src/tlsuv.c",
        "src/um_debug.c",
        "src/base64.c",
        "src/tls_engine.c",
        "src/p11.c",
        "src/socket.c",
        "src/connector.c",
        "src/alloc.c",
        "src/keychain.c",
    };

    // Add base sources
    lib.addCSourceFiles(.{
        .files = &sources,
        .flags = &[_][]const u8{"-std=c99"},
    });

    // Platform-specific keychain integration
    if (enable_keychain) {
        if (t.isDarwin()) {
            lib.addCSourceFile(.{
                .file = b.path("src/apple/keychain.c"),
                .flags = &[_][]const u8{"-std=c99"},
            });
            // Add Apple frameworks
            lib.linkFrameworkNeeded("CoreFoundation");
            lib.linkFrameworkNeeded("Security");

        } else if (t.os.tag == .windows) {
            lib.addCSourceFile(.{
                .file = b.path("src/win32/win32_keychain.c"),
                .flags = &[_][]const u8{"-std=c99"},
            });
            // Add Windows-specific libraries
            lib.linkSystemLibrary("crypt32");
            lib.linkSystemLibrary("ncrypt");
        }
    }

    // HTTP-related sources
    if (enable_http) {
        const http_sources = [_][]const u8{
            "src/http.c",
            "src/tcp_src.c",
            "src/websocket.c",
            "src/http_req.c",
            "src/tls_link.c",
            "src/compression.c",
        };
        lib.addCSourceFiles(.{
            .files = &http_sources,
            .flags = &[_][]const u8{"-std=c99"},
        });

        // TODO: Add HTTP-related dependencies properly through build.zig.zon
        // For now, linking against system libraries
        lib.linkSystemLibrary("z");
        lib.linkSystemLibrary("llhttp");
        lib.linkSystemLibrary("uv_link");
    }

    // TLS implementation sources
    if (use_openssl) {
        const ssl_dir = "src/openssl";
        const ssl_files = [_][]const u8{
            ssl_dir ++ "/verify.c",
            ssl_dir ++ "/tls_link.c",
            ssl_dir ++ "/engine.c",
            // Add other OpenSSL-specific files here
        };
        lib.addCSourceFiles(.{
            .files = &ssl_files,
            .flags = &[_][]const u8{"-std=c99"},
        });
        // TODO: Add OpenSSL dependencies properly through build.zig.zon
        lib.linkSystemLibrary("ssl");
        lib.linkSystemLibrary("crypto");
    } else if (use_mbedtls) {
        const ssl_dir = "src/mbedtls";
        const ssl_files = [_][]const u8{
            ssl_dir ++ "/engine.c",
            ssl_dir ++ "/keys.c",
            ssl_dir ++ "/mbed_p11.c",
            ssl_dir ++ "/p11_ecdsa.c",
            ssl_dir ++ "/p11_rsa.c"
        };
        lib.addCSourceFiles(.{
            .files = &ssl_files,
            .flags = &[_][]const u8{"-std=c99"},
        });
        // Link with mbedTLS libraries
        if (dep_libmbedtls) |mbedtls| {
            lib.linkLibrary(mbedtls.artifact("mbedcrypto"));
            lib.linkLibrary(mbedtls.artifact("mbedtls"));
            lib.linkLibrary(mbedtls.artifact("mbedx509"));
        }
    }

    // Include directories
    lib.addIncludePath(b.path("include"));
    lib.addIncludePath(b.path("src"));

    // Link with libuv
    lib.linkLibrary(dep_libuv.artifact("uv"));

    // System-specific definitions
    if (t.os.tag == .windows) {
        lib.defineCMacro("WIN32_LEAN_AND_MEAN", null);
        lib.defineCMacro("WINVER", "0x0A00");
        lib.defineCMacro("_WIN32_WINNT", "0x0A00");
        lib.defineCMacro("_CRT_SECURE_NO_WARNINGS", null);
    }

    if (t.os.tag == .linux) {
        lib.defineCMacro("_POSIX_C_SOURCE", "200112");
        lib.defineCMacro("_GNU_SOURCE", null);
    }

    // Version definition
    lib.defineCMacro("TLSUV_VERSION", "v" ++ version);

    // TLS-specific definitions
    if (use_mbedtls) {
        lib.defineCMacro("USE_MBEDTLS", null);
        lib.defineCMacro("TLSUV_TLSLIB", "mbedtls");
    } else if (use_openssl) {
        lib.defineCMacro("USE_OPENSSL", null);
    }

    lib.linkLibC();

    // Install headers
    lib.installHeadersDirectory(b.path("include/tlsuv"), "tlsuv", .{});

    // Installation
    b.installArtifact(lib);
}
