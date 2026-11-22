#include "TutorialOverlay.hpp"

#include <QObject>

//the tutorial, in order: each lesson switches to its view, leaves its target undimmed and shows its text next to it
//
//	text		markdown (headings, **bold**, *italic*, lists, [links](https://...)); empty: only the buttons are shown
//	view		ANALYSIS or PLAY, switched to before the lesson shows
//	target		which part stays clear, see TutorialTarget; the drawer and the eval sidebar are opened for their lesson
//	customArea	with target CUSTOM: the clear area in fractions of the window, e.g. {0.5, 0.0, 0.5, 0.25} is the top right quarter
//	padding		grows the clear area around the target on each side (left, top, right, bottom), negative values shrink it
//
//changing anything here does not show the tutorial again by itself; for that, the text in the marker file
//(MARKER_TEXT in TutorialOverlay.cpp) has to change, or the file has to be deleted
std::vector<TutorialLesson> buildTutorialLessons() {
	return {
		//welcome: an example of a finished lesson
		{
			.text = QObject::tr(
				"### Welcome!\n"
				"This is a short tour through the app. It takes about a minute.\n\n"
				"Click **Next** to go on, or **Skip** if you'd rather explore on your own."),
			.view = ViewMode::ANALYSIS,
			.target = TutorialTarget::NONE,
		},

		//engine settings in the play view
		{
			.text = {},	//TODO
			.view = ViewMode::PLAY,
			.target = TutorialTarget::ENGINE_SETTINGS,
			.padding = {10, 10, 10, 10},
		},

		//play / analysis switch
		{
			.text = {},	//TODO
			.view = ViewMode::PLAY,
			.target = TutorialTarget::MODE_SWITCH,
		},

		//M for the move tree: it takes over the mouse when open, so this one is text only
		{
			.text = {},	//TODO
			.view = ViewMode::ANALYSIS,
			.target = TutorialTarget::NONE,
		},

		//analysis settings: the evaluation column
		{
			.text = {},	//TODO
			.view = ViewMode::ANALYSIS,
			.target = TutorialTarget::EVAL_SIDEBAR,
		},

		//content drawer with the principles
		{
			.text = {},	//TODO
			.view = ViewMode::ANALYSIS,
			.target = TutorialTarget::PRINCIPLE_DRAWER,
			.padding = {0, 0, 0, 0},
		},

		//goodbye: where to find the tutorial again
		{
			.text = {},	//TODO
			.view = ViewMode::ANALYSIS,
			.target = TutorialTarget::TUTORIAL_BUTTON,
		},
	};
}
