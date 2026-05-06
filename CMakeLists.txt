if(CONFIG_ZMK_AGENT_IOS_POINTER_NUDGE)
  zephyr_library_named(agent_keyboard_ios_pointer_nudge)
  zephyr_library_sources(src/ios_pointer_nudge.c)
endif()
