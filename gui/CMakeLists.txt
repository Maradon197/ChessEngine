qt_add_library(ChessGui STATIC
    GuiTypes.hpp
    MoveBits.hpp
    Theme.hpp               Theme.cpp
    KeyBindings.hpp         KeyBindings.cpp
    ToolBarWidget.hpp       ToolBarWidget.cpp
    Doorman.hpp             Doorman.cpp
    BoardWidget.hpp         BoardWidget.cpp
    TreeLayout.hpp
    MoveTreeOverlay.hpp     MoveTreeOverlay.cpp
    SearchRecorder.hpp      SearchRecorder.cpp
    SearchTreeModel.hpp     SearchTreeModel.cpp
    SearchTreeView.hpp      SearchTreeView.cpp
    SearchWindow.hpp        SearchWindow.cpp
    PrincipleDrawer.hpp     PrincipleDrawer.cpp
    EvalSquareWidget.hpp    EvalSquareWidget.cpp
    EvalCriteriaWidget.hpp  EvalCriteriaWidget.cpp
    AnalysisSidebar.hpp     AnalysisSidebar.cpp
    PlayView.hpp            PlayView.cpp
    ModeSwitch.hpp          ModeSwitch.cpp
    TutorialOverlay.hpp     TutorialOverlay.cpp
    TutorialLessons.cpp
    MainWindow.hpp          MainWindow.cpp
)

target_include_directories(ChessGui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ChessGui PUBLIC
    ChessCore
    ChessUtil
    ChessEngine
    Qt::Core
    Qt::Widgets
)
