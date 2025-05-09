file(GLOB_RECURSE SOURCES ./led_strip/src/*.c)

idf_component_register(SRCS "rgb.c" ${SOURCES}
                    INCLUDE_DIRS "include"
                                 "./led_strip/include"
                                 "./led_strip/interface"
                    REQUIRES driver)
