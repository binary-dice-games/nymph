# MIT License © 2026 Binary Dice Games
#
# Assembles the GitHub Pages site (spec/15_web_app/DESIGN.md section 3.1)
# into OUT: the app at the root, the viewer's files at their published
# paths, the editor module, fonts, examples, templates, the demo page,
# version.json, config.json and the service worker. Run with cmake -P:
#   -DWEB=<build>/web -DSOURCE=<nymph source> -DOUT=<site>
#   -DVERSION=<x.y.z> -DCOMMIT=<sha>
#   -DGOOGLE_CLIENT_ID= -DGOOGLE_API_KEY= -DGOOGLE_APP_ID=
#   -DAGENT_CONNECTORS=webmcp,bridge (spec/17_agent_connectors)

foreach(var WEB SOURCE OUT VERSION COMMIT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NymphSite.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${OUT}")
file(MAKE_DIRECTORY "${OUT}")

# The app (web/app/), less the service worker's template.
file(GLOB _app_top LIST_DIRECTORIES false "${SOURCE}/web/app/*")
list(FILTER _app_top EXCLUDE REGEX "sw\\.js\\.in$")
file(COPY ${_app_top} DESTINATION "${OUT}")
file(COPY "${SOURCE}/web/app/app" "${SOURCE}/web/app/templates" DESTINATION "${OUT}")
file(COPY "${SOURCE}/resources/icon.png" DESTINATION "${OUT}")

# The viewer at the paths embedders use, the editor, fonts, examples, demo.
foreach(f nymph-viewer.js nymph-viewer-core.js nymph-viewer-core.wasm nymph-viewer.single.js)
  file(COPY "${WEB}/${f}" DESTINATION "${OUT}")
endforeach()
file(COPY "${WEB}/editor" "${WEB}/fonts" "${WEB}/examples" "${WEB}/demo" DESTINATION "${OUT}")

# The agent guide and the element reference, for connectors (section 11.2).
file(MAKE_DIRECTORY "${OUT}/agent")
file(COPY "${SOURCE}/docs/agent-guide.md" "${SOURCE}/docs/elements.md" "${SOURCE}/docs/format.md" DESTINATION "${OUT}/agent")
file(COPY "${SOURCE}/docs/elements" DESTINATION "${OUT}/agent")
# The workflow guide agents read in the app (spec/17_agent_connectors).
configure_file("${SOURCE}/docs/agent-web-app.md" "${OUT}/agent/web-app.md" COPYONLY)

# What the app reads at start.
file(WRITE "${OUT}/version.json" "{\"version\": \"${VERSION}\", \"commit\": \"${COMMIT}\"}\n")
set(_google "")
if(GOOGLE_CLIENT_ID AND GOOGLE_API_KEY)
  set(_google "\"google\": {\"clientId\": \"${GOOGLE_CLIENT_ID}\", \"apiKey\": \"${GOOGLE_API_KEY}\", \"appId\": \"${GOOGLE_APP_ID}\"}, ")
endif()
# The agent connectors offered (spec/17_agent_connectors/DESIGN.md section 6).
if(NOT DEFINED AGENT_CONNECTORS)
  set(AGENT_CONNECTORS "webmcp,bridge")
endif()
string(REPLACE "," ";" AGENT_CONNECTORS "${AGENT_CONNECTORS}")
set(_agents "")
list(FIND AGENT_CONNECTORS "webmcp" _webmcp)
list(FIND AGENT_CONNECTORS "bridge" _bridge)
if(_webmcp GREATER -1)
  string(APPEND _agents "\"webmcp\": {}")
endif()
if(_bridge GREATER -1)
  if(_agents)
    string(APPEND _agents ", ")
  endif()
  string(APPEND _agents "\"bridge\": {}")
endif()
file(WRITE "${OUT}/config.json" "{${_google}\"agents\": {${_agents}}}\n")

# The service worker, with this build's commit and the app's modules.
file(GLOB_RECURSE _app_js RELATIVE "${OUT}" "${OUT}/app/*.js")
list(SORT _app_js)
set(_files "")
foreach(f IN LISTS _app_js)
  string(APPEND _files "  '${f}',\n")
endforeach()
file(READ "${SOURCE}/web/app/sw.js.in" _sw)
string(REPLACE "@NYMPH_COMMIT@" "${COMMIT}" _sw "${_sw}")
string(REPLACE "@NYMPH_APP_FILES@\n" "${_files}" _sw "${_sw}")
file(WRITE "${OUT}/sw.js" "${_sw}")
