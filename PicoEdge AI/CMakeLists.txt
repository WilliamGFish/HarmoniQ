if(CONFIG_PICOEDGE_AI)
  zephyr_include_directories(include)
  zephyr_library_named(picoedge_ai)
  zephyr_library_sources(
    src/picoedge_dsp.c
    src/picoedge_core.c
    src/picoedge_serde.c
    src/picoedge_smf.c
  )
  zephyr_library_compile_options(-Os -Wall -Wextra -Werror)
endif()
