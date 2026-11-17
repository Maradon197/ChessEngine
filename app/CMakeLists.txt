#no WIN32: keeps the console for debug output
qt_add_executable(ChessEngineGUI
    main.cpp
)

target_link_libraries(ChessEngineGUI PRIVATE
    ChessCore
    ChessUtil
    ChessGui
)

#principles are read straight from the source tree, so a refresh picks up edits without rebuilding
target_compile_definitions(ChessEngineGUI PRIVATE CHESS_DATA_DIR="${CMAKE_SOURCE_DIR}/data")

#copy the Qt DLLs next to the exe after every build
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
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/ DESTINATION ${CMAKE_INSTALL_BINDIR}/data)
