set_project("eventide")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_allowedplats("windows", "linux", "macosx")

option("dev", { default = true })
option("test", { default = true })
option("serde_simdjson", {
	default = false,
	showmenu = true,
	description = "Enable simdjson dependency for serde tests/headers",
})
option("build_all_tests", {
	default = false,
	showmenu = true,
	description = "Enable all optional unit-test features (CI preset)",
})

local build_all_tests = has_config("build_all_tests")
local enable_simdjson = build_all_tests or has_config("serde_simdjson")

if has_config("dev") then
	-- Don't fetch system package
	set_policy("package.install_only", true)
	set_policy("build.ccache", true)
	-- Keep Windows toolchains aligned with third-party prebuilt packages (e.g. cpptrace),
	-- otherwise ASan-specific /failifmismatch metadata can break linking.
	if is_mode("debug") and not is_plat("windows") then
		set_policy("build.sanitizer.address", true)
	end

	add_rules("plugin.compile_commands.autoupdate", { outputdir = "build", lsp = "clangd" })

	if is_plat("windows") then
		set_runtimes("MD")

		local toolchain = get_config("toolchain")
		if toolchain == "clang" then
			add_ldflags("-fuse-ld=lld-link")
			add_shflags("-fuse-ld=lld-link")
		elseif toolchain == "clang-cl" then
			set_toolset("ld", "lld-link")
			set_toolset("sh", "lld-link")
		end
	elseif is_plat("macosx") then
		-- https://conda-forge.org/docs/maintainer/knowledge_base/#newer-c-features-with-old-sdk
		add_defines("_LIBCPP_DISABLE_AVAILABILITY=1")
		add_ldflags("-fuse-ld=lld")
		add_shflags("-fuse-ld=lld")

		add_requireconfs("**|cmake", {
			configs = {
				ldflags = "-fuse-ld=lld",
				shflags = "-fuse-ld=lld",
				cxflags = "-D_LIBCPP_DISABLE_AVAILABILITY=1",
			},
		})
	end
end

set_languages("c++23")

add_requires("libuv v1.52.0", "cpptrace v1.0.4")
if enable_simdjson then
	add_requires("simdjson v4.2.4")
end
if (build_all_tests or has_config("test")) and is_plat("windows") then
	add_requires("unistd_h")
end

target("ztest", function()
	set_kind("$(kind)")
	add_files("src/zest/*.cpp")
	add_includedirs("include", { public = true })
	add_headerfiles("include/(zest/*.h)", "include/(reflection/*.h)", "include/(reflection/*.inl)")
	add_cxflags("cl::/Zc:preprocessor", { public = true })
	add_packages("cpptrace", { public = true })
end)

target("eventide", function()
	set_kind("$(kind)")
	add_files("src/eventide/*.cpp")
	add_includedirs("include", { public = true })
	add_headerfiles("include/(eventide/*.h)")
	add_packages("libuv")

	if enable_simdjson then
		add_packages("simdjson", { public = true })
	end
end)

target("unit_tests", function()
	set_default(false)
	set_kind("binary")
	if enable_simdjson then
		add_files("tests/**.cpp")
		add_files("src/language/server.cpp", "src/language/transport.cpp")
		add_packages("simdjson")
	else
		add_files("tests/**.cpp|serde/**.cpp|language/**.cpp")
	end
	add_includedirs("include")
	add_deps("ztest", "eventide")

	if (build_all_tests or has_config("test")) and is_plat("windows") then
		add_packages("unistd_h")
	end

	add_tests("default")
end)
