cmrc_add_resource_library(flycast-resources ALIAS flycast::res NAMESPACE flycast)

cmrc_add_resources(flycast-resources
        WHENCE resources
        resources/flash/alienfnt.nvmem.zip	# network settings
        resources/flash/gunsur2.nvmem.zip
        resources/flash/mazan.nvmem.zip		# free play, calibrated
        resources/flash/otrigger.nvmem.zip
        resources/flash/wldkicks.nvmem.zip	# free play
        resources/flash/wldkicksj.nvmem.zip	# free play
        resources/flash/wldkicksu.nvmem.zip	# free play
        resources/flash/f355.nvmem.zip		# printer on
        resources/flash/f355twin.nvmem.zip	# printer on
        resources/flash/f355twn2.nvmem.zip	# printer on
        resources/flash/dirtypig.nvmem.zip	# 4 players
        resources/flash/dirtypig.nvmem2.zip
        resources/flash/vf4.nvmem.zip		# card all day, stage select
        resources/flash/vf4evob.nvmem.zip	# card all day, stage select
        resources/flash/vf4tuned.nvmem.zip  # card all day, stage select, 45 sec time limit, 3 match
        resources/flash/magicpop.nvmem.zip	# debug: comm and all errors disabled
        resources/flash/ochaken.nvmem.zip	# debug: comm and all errors disabled
        resources/flash/puyomedal.nvmem.zip	# debug: comm and all errors disabled
        resources/flash/unomedal.nvmem.zip	# debug: comm and all errors disabled
        resources/flash/westdrmg.nvmem.zip	# debug: comm and all errors disabled
        resources/flash/smarinef.nvmem.zip	# standard cabinet
        resources/picture/f355_print_template.png)

cmrc_add_resources(flycast-resources
        fonts/printer_ascii8x16.bin.zip
        fonts/printer_ascii12x24.bin.zip
        fonts/printer_kanji16x16.bin.zip
        fonts/printer_kanji24x24.bin.zip)

if(NOT LIBRETRO)
    cmrc_add_resources(flycast-resources
            fonts/Roboto-Medium.ttf.zip
            fonts/Roboto-Regular.ttf.zip
            fonts/fa-solid-900.ttf.zip)
    if(ANDROID OR IOS)
        cmrc_add_resources(flycast-resources
                WHENCE resources
                resources/picture/buttons.png
                resources/picture/buttons-arcade.png)
    endif()
endif()

cmrc_add_resources(flycast-resources fonts/biosfont.bin.zip)

cmrc_add_resources(flycast-resources
	WHENCE resources
	resources/i18n/fr.po
	resources/i18n/hu.po
	resources/i18n/ja.po
	resources/i18n/pt_BR.po
	resources/i18n/sv.po
	resources/i18n/zh_CN.po
	resources/i18n/zh_HK.po
	resources/i18n/zh_TW.po)
