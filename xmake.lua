set_project("Inexor")

set_languages("cxx20")

option("INEXOR_BUILD_BENCHMARKS")
set_default(false)
set_showmenu(true)
add_defines("INEXOR_BUILD_BENCHMARKS")
after_check(function(option)
  print(string.format("%s: %s", option:name(), tostring(option:enabled())))
  option:enable(false)
end)

option("INEXOR_BUILD_DOC")
set_default(false)
set_showmenu(true)
add_defines("INEXOR_BUILD_DOC")
after_check(function(option)
  print(string.format("%s: %s", option:name(), tostring(option:enabled())))
  option:enable(false)
end)

option("INEXOR_BUILD_EXAMPLE")
set_default(true)
set_showmenu(true)
add_defines("INEXOR_BUILD_EXAMPLE")
after_check(function(option)
  print(string.format("%s: %s", option:name(), tostring(option:enabled())))
  option:enable(false)
end)

option("INEXOR_BUILD_TESTS")
set_default(false)
set_showmenu(true)
add_defines("INEXOR_BUILD_TESTS")
after_check(function(option)
  print(string.format("%s: %s", option:name(), tostring(option:enabled())))
  option:enable(false)
end)

INEXOR_ENGINE_NAME = "Inexor Engine"
INEXOR_APP_NAME = "Inexor Vulkan-renderer example"

INEXOR_ENGINE_VERSION_MAJOR = 0
INEXOR_ENGINE_VERSION_MINOR = 1
INEXOR_ENGINE_VERSION_PATCH = 0

INEXOR_APP_VERSION_MAJOR = 0
INEXOR_APP_VERSION_MINOR = 1
INEXOR_APP_VERSION_PATCH = 0

includes("./shaders/")
includes("./src/")

if has_config("INEXOR_BUILD_BENCHMARKS") then
  includes("./benchmarks/")
end

if has_config("INEXOR_BUILD_DOC") then
  includes("./documentation/")
end

if has_config("INEXOR_BUILD_EXAMPLE") then
  includes("./example/")
end

if has_config("INEXOR_BUILD_TESTS") then
  includes("./tests/")
end
