set(COMPONENT_SRCS 
"main.c"
"Application/Main/VoiceAssistant.cpp"
"Application/ESP32Lyrat/LyratAudio.cpp"
)
set(COMPONENT_ADD_INCLUDEDIRS . Application/Main
                                Application/Interface
                                Application/ESP32Lyrat)

register_component()