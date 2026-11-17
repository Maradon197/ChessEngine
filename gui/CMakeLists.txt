qt_add_library(ChessGui STATIC
    GuiTypes.hpp
    MoveBits.hpp
    Theme.hpp               Theme.cpp
    ToolBarWidget.hpp       ToolBarWidget.cpp
    Doorman.hpp             Doorman.cpp
    BoardWidget.hpp         BoardWidget.cpp
    MoveTreeOverlay.hpp     MoveTreeOverlay.cpp
    EvalSquareWidget.hpp    EvalSquareWidget.cpp
    EvalCriteriaWidget.hpp  EvalCriteriaWidget.cpp
    MainWindow.hpp          MainWindow.cpp
)

target_include_directories(ChessGui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ChessGui PUBLIC
    ChessCore
    Qt::Core
    Qt::Widgets
)
