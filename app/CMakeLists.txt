# No WIN32 flag on purpose: the console window stays open for the core's debug output.
# Add WIN32 after qt_add_executable(ChessEngineGUI ...) to hide it.
qt_add_executable(ChessEngineGUI
    main.cpp
)

target_link_libraries(ChessEngineGUI PRIVATE
    ChessCore
    ChessGui
)

# Windows: copy the Qt DLLs and plugins next to the exe after every build,
# so it starts from Visual Studio / Qt Creator / Explorer without touching PATH.
if(WIN32)
    if(TARGET Qt6::windeployqt)
        set(CHESS_WINDEPLOYQT Qt6::windeployqt)
    else()
        find_program(CHESS_WINDEPLOYQT windeployqt HINTS "${QT6_INSTALL_PREFIX}/bin" REQUIRED)
    endif()
    add_custom_command(TARGET ChessEngineGUI POST_BUILD
        COMMAND ${CHESS_WINDEPLOYQT} --no-translations --no-compiler-runtime --verbose 0 "$<TARGET_FILE:ChessEngineGUI>"
        COMMENT "Deploying Qt DLLs next to ChessEngineGUI"
        VERBATIM
    )
endif()

install(TARGETS ChessEngineGUI
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ChessEngineGUI
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
