set_project("eventide")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_allowedplats("windows", "linux", "macosx")

add_requires("libuv", { version = "v1.51.0" })
add_rules("plugin.compile_commands.autoupdate", { outputdir = "build", lsp = "clangd" })

target("unit_tests", function()
	set_kind("binary")
	add_files("tests/**.cpp")
	add_packages("libuv")
end)
