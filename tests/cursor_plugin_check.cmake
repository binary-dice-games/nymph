# MIT License © 2026 Binary Dice Games
#
# spec/12_cursor_integration/PLAN.md step 3: the Cursor plugin's manifest,
# MCP entry, marketplace, rule and skill have the fields Cursor documents
# (https://cursor.com/docs/reference/plugins).
#
#   cmake -DSOURCE_DIR=<repo> -P cursor_plugin_check.cmake

set(plugin "${SOURCE_DIR}/cursor/plugin")

function(fail what)
  message(FATAL_ERROR "cursor plugin: ${what}")
endfunction()

# The manifest: only "name" is required; ours names the components too.
file(READ "${plugin}/.cursor-plugin/plugin.json" manifest)
string(JSON name GET "${manifest}" name)
if(NOT name STREQUAL "nymph")
  fail("manifest name is '${name}'")
endif()
string(JSON version GET "${manifest}" version)
if(NOT version MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
  fail("manifest version '${version}' is not x.y.z")
endif()
foreach(key rules skills mcpServers logo)
  string(JSON path GET "${manifest}" ${key})
  if(NOT EXISTS "${plugin}/${path}")
    fail("manifest ${key} '${path}' does not exist")
  endif()
endforeach()

# The MCP entry runs the launcher through the plugin root, with "mcp".
file(READ "${plugin}/mcp.json" mcp)
string(JSON args GET "${mcp}" mcpServers nymph args)
string(JSON launcher GET "${mcp}" mcpServers nymph args 0)
string(JSON last GET "${mcp}" mcpServers nymph args 1)
if(NOT launcher STREQUAL "\${CURSOR_PLUGIN_ROOT}/bin/nymph" OR NOT last STREQUAL "mcp")
  fail("mcp.json args are ${args}")
endif()
if(NOT EXISTS "${plugin}/bin/nymph")
  fail("bin/nymph is missing")
endif()

# The marketplace lists the plugin by a directory that holds its manifest.
file(READ "${SOURCE_DIR}/.cursor-plugin/marketplace.json" market)
foreach(key name owner plugins)
  string(JSON ignored GET "${market}" ${key})
endforeach()
string(JSON source GET "${market}" plugins 0 source)
string(JSON listed GET "${market}" plugins 0 name)
if(NOT listed STREQUAL "nymph" OR NOT EXISTS "${SOURCE_DIR}/${source}/.cursor-plugin/plugin.json")
  fail("marketplace entry '${listed}' source '${source}' is not the plugin")
endif()

# The rule: front matter with a description, not always on, no globs.
file(READ "${plugin}/rules/nymph.mdc" rule)
if(NOT rule MATCHES "^---\n")
  fail("rule has no front matter")
endif()
string(REGEX MATCH "^---\n(.*)\n---\n" ignored "${rule}")
set(front "${CMAKE_MATCH_1}")
if(NOT front MATCHES "(^|\n)description: [^\n]+")
  fail("rule has no description")
endif()
if(NOT front MATCHES "(^|\n)alwaysApply: false(\n|$)")
  fail("rule is not alwaysApply: false")
endif()
if(front MATCHES "globs")
  fail("rule has globs")
endif()
# It points at what exists: the tool and the skill.
if(NOT rule MATCHES "render_chart" OR NOT rule MATCHES "chart_schema")
  fail("rule does not name the nymph tools")
endif()

# The skill: Agent Skills front matter, its name the folder's.
file(READ "${plugin}/skills/nymph/SKILL.md" skill)
if(NOT skill MATCHES "^---\nname: nymph\ndescription: ")
  fail("SKILL.md front matter lacks name: nymph and a description")
endif()
message(STATUS "cursor plugin: manifest, mcp.json, marketplace, rule and skill are valid")
