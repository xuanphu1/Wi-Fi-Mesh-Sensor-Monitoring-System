SET(SOURCES ui_Screen1.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_matrix0_png.c
    ui_img_1612021159.c
    ui_img_48530715.c
    ui_img_746023423.c
    ui_img_733045785.c
    ui_img_136761274.c
    ui_img_arrowdown_png.c
    ui_img_arrowup_png.c
    ui_img_latency_png.c
    ui_img_throught_png.c
    ui_font_Arteon10.c
    ui_font_Arteon12.c
    ui_font_Arteon14.c
    ui_font_Arteon16.c
    ui_font_Arteon20S.c
    ui_font_Arteon24.c
    ui_font_Arteon30.c
    ui_font_Arteon40.c)

    idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS "."
    REQUIRES lvgl)