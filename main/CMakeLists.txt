set(COMPONENT_SRCS 
"main.c"
"Application/Main/VoiceAssistant.cpp"
"Application/ESP32/AudioESP.cpp"
)

set(COMPONENT_ADD_INCLUDEDIRS   . 
                                Application/Main
                                Application/Interface
                                Application/ESP32)

register_component()