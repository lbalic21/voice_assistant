set(COMPONENT_SRCS 
"main.c"
"Application/VoiceAssistant.cpp"
"Application/AudioRecorder.cpp"
"Application/FeatureProvider.cpp"
"Application/FeatureGenerator.cpp"
"Application/CommandRecognizer.cpp"
"Application/CommandResponder.cpp"
"Application/ringBuff.c"
"Application/model.cc"
"Application/projectConfig.cpp"
)

set(COMPONENT_ADD_INCLUDEDIRS   .
                                Application)

register_component()