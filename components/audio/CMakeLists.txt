# Audio 组件 CMakeLists.txt
idf_component_register(
    SRCS "audio.c" "piano_samples.c" "drum_samples.c" "gm_drum_samples.c"
    INCLUDE_DIRS "."
    REQUIRES driver log esp_adc
)
